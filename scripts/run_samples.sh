#!/usr/bin/env bash

set -u

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SAMPLE_DIR="$ROOT_DIR/tests"
BUILD_DIR="$ROOT_DIR/build"
RUN_DIR="${1:-$BUILD_DIR/sample_runs}"
COMPILER="$BUILD_DIR/c--compiler"

PASS_COUNT=0
FAIL_COUNT=0

expected_ok_exit() {
    case "$1" in
        ok_001_minimal_return) echo 0 ;;
        ok_002_local_const_decl) echo 8 ;;
        ok_003_local_decl_assign) echo 11 ;;
        ok_004_arithmetic_precedence) echo 20 ;;
        ok_005_parenthesized_expression) echo 21 ;;
        ok_006_unary_expression) echo 6 ;;
        ok_007_if_else_branch) echo 1 ;;
        ok_008_modulo_expression) echo 2 ;;
        ok_009_function_call) echo 14 ;;
        ok_010_void_function_expr_stmt) echo 6 ;;
        ok_011_block_scope) echo 1 ;;
        ok_012_line_comment) echo 11 ;;
        ok_013_block_comment) echo 6 ;;
        ok_014_nested_if_else) echo 7 ;;
        ok_015_logic_and_or) echo 2 ;;
        *) echo "unknown" ;;
    esac
}

error_stage() {
    local run_info="$1/run_info.txt"
    if [ ! -f "$run_info" ]; then
        echo "no_run_info"
        return
    fi

    local stage
    stage="$(grep '^error_stage:' "$run_info" | sed 's/error_stage: //' | head -n 1)"
    if [ -z "$stage" ]; then
        echo "ok"
    else
        echo "$stage"
    fi
}

record_pass() {
    PASS_COUNT=$((PASS_COUNT + 1))
    printf "[PASS] %-42s %s\n" "$1" "$2"
}

record_fail() {
    FAIL_COUNT=$((FAIL_COUNT + 1))
    printf "[FAIL] %-42s %s\n" "$1" "$2"
}

run_compiler() {
    local sample="$1"
    local out_dir="$2"

    rm -rf "$out_dir"
    mkdir -p "$out_dir"
    bash -c '"$1" "$2" -o "$3"; code=$?; exit "$code"' _ "$COMPILER" "$sample" "$out_dir" >"$out_dir/compiler.stdout" 2>"$out_dir/compiler.stderr"
}

check_ok_sample() {
    local sample="$1"
    local name="$2"
    local out_dir="$3"
    local expected_exit
    expected_exit="$(expected_ok_exit "$name")"

    if [ "$expected_exit" = "unknown" ]; then
        record_fail "$name" "没有登记正确样例的预期退出码"
        return
    fi

    run_compiler "$sample" "$out_dir"
    local compiler_code=$?
    if [ "$compiler_code" -ne 0 ]; then
        record_fail "$name" "编译器应成功，但退出码为 $compiler_code"
        return
    fi

    if [ ! -f "$out_dir/output.ll" ]; then
        record_fail "$name" "编译器成功但没有生成 output.ll"
        return
    fi

    clang "$out_dir/output.ll" -o "$out_dir/a.out" >"$out_dir/clang.stdout" 2>"$out_dir/clang.stderr"
    local clang_code=$?
    if [ "$clang_code" -ne 0 ]; then
        record_fail "$name" "clang 应成功，但退出码为 $clang_code"
        return
    fi

    "$out_dir/a.out"
    local actual_exit=$?
    if [ "$actual_exit" -ne "$expected_exit" ]; then
        record_fail "$name" "运行退出码应为 $expected_exit，实际为 $actual_exit"
        return
    fi

    record_pass "$name" "完整流程通过，退出码 $actual_exit"
}

check_error_sample() {
    local sample="$1"
    local name="$2"
    local out_dir="$3"
    local expected_stage="$4"

    run_compiler "$sample" "$out_dir"
    local compiler_code=$?
    local actual_stage
    actual_stage="$(error_stage "$out_dir")"

    if [ "$compiler_code" -eq 0 ]; then
        record_fail "$name" "编译器应失败，但实际成功"
        return
    fi

    if [ "$expected_stage" = "ir" ]; then
        if [ "$actual_stage" = "ir" ] || [ "$actual_stage" = "no_run_info" ]; then
            record_pass "$name" "按预期在 IR 阶段失败或异常退出"
        else
            record_fail "$name" "应在 IR 阶段失败，实际阶段为 $actual_stage"
        fi
        return
    fi

    if [ "$actual_stage" = "$expected_stage" ]; then
        record_pass "$name" "按预期在 $expected_stage 阶段失败"
    else
        record_fail "$name" "应在 $expected_stage 阶段失败，实际阶段为 $actual_stage"
    fi
}

check_ir_invalid_sample() {
    local sample="$1"
    local name="$2"
    local out_dir="$3"

    run_compiler "$sample" "$out_dir"
    local compiler_code=$?
    if [ "$compiler_code" -ne 0 ]; then
        record_fail "$name" "编译器流程应先成功生成 output.ll，但退出码为 $compiler_code"
        return
    fi

    if [ ! -f "$out_dir/output.ll" ]; then
        record_fail "$name" "编译器成功但没有生成 output.ll"
        return
    fi

    clang "$out_dir/output.ll" -o "$out_dir/a.out" >"$out_dir/clang.stdout" 2>"$out_dir/clang.stderr"
    local clang_code=$?
    if [ "$clang_code" -eq 0 ]; then
        record_fail "$name" "clang 应拒绝非法 IR，但实际成功"
    else
        record_pass "$name" "编译器生成 IR 后，被 clang 按预期拒绝"
    fi
}

main() {
    if [ ! -d "$SAMPLE_DIR" ]; then
        echo "sample directory not found: $SAMPLE_DIR"
        exit 1
    fi

    if ! command -v clang >/dev/null 2>&1; then
        echo "clang not found; cannot verify ok_*.sy and ir_invalid_*.sy"
        exit 1
    fi

    make -C "$ROOT_DIR" c--compiler >/dev/null
    if [ ! -x "$COMPILER" ]; then
        echo "compiler not found: $COMPILER"
        exit 1
    fi

    rm -rf "$RUN_DIR"
    mkdir -p "$RUN_DIR"

    local sample
    for sample in "$SAMPLE_DIR"/*.sy; do
        local file
        local name
        local out_dir
        file="$(basename "$sample")"
        name="${file%.sy}"
        out_dir="$RUN_DIR/$name"

        case "$name" in
            ok_*)
                check_ok_sample "$sample" "$name" "$out_dir"
                ;;
            lex_error_*)
                check_error_sample "$sample" "$name" "$out_dir" "lexer"
                ;;
            parse_error_*)
                check_error_sample "$sample" "$name" "$out_dir" "parser"
                ;;
            ir_error_*)
                check_error_sample "$sample" "$name" "$out_dir" "ir"
                ;;
            semantic_error_*)
                check_error_sample "$sample" "$name" "$out_dir" "semantic"
                ;;
            ir_invalid_*)
                check_ir_invalid_sample "$sample" "$name" "$out_dir"
                ;;
            *)
                record_fail "$name" "未知命名格式"
                ;;
        esac
    done

    echo
    echo "sample test summary:"
    echo "  passed: $PASS_COUNT"
    echo "  failed: $FAIL_COUNT"
    echo "  output: $RUN_DIR"

    if [ "$FAIL_COUNT" -ne 0 ]; then
        exit 1
    fi
}

main "$@"
