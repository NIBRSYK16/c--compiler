#!/usr/bin/env bash

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/sample_test_common.sh"

RUN_DIR="${1:-$BUILD_DIR/sample_ir_runs}"

check_ok_ir_sample() {
    local name="$1"
    local out_dir="$2"
    local expected_exit
    expected_exit="$(expected_ok_exit "$name")"

    if [ "$expected_exit" = "unknown" ]; then
        record_fail "$name" "没有登记正确样例的预期退出码"
        return
    fi

    if [ ! -f "$out_dir/output.ll" ]; then
        record_fail "$name" "IR 阶段成功但没有生成 output.ll"
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

    record_pass "$name" "IR 可编译运行，退出码 $actual_exit"
}

check_ir_invalid_sample() {
    local name="$1"
    local out_dir="$2"

    if [ ! -f "$out_dir/output.ll" ]; then
        record_fail "$name" "IR 阶段成功但没有生成 output.ll"
        return
    fi

    clang "$out_dir/output.ll" -o "$out_dir/a.out" >"$out_dir/clang.stdout" 2>"$out_dir/clang.stderr"
    local clang_code=$?
    if [ "$clang_code" -eq 0 ]; then
        record_fail "$name" "clang 应拒绝非法 IR，但实际成功"
    else
        record_pass "$name" "IR 按预期被 clang 拒绝"
    fi
}

check_ir_sample() {
    local sample="$1"
    local name="$2"
    local out_dir="$3"

    run_compiler_stage "ir" "$sample" "$out_dir"
    local compiler_code=$?
    local actual_stage
    actual_stage="$(error_stage "$out_dir")"

    case "$name" in
        lex_error_*)
            if [ "$compiler_code" -ne 0 ] && [ "$actual_stage" = "lexer" ]; then
                record_pass "$name" "按预期在 lexer 阶段失败"
            else
                record_fail "$name" "应在 lexer 阶段失败，实际退出码 $compiler_code，阶段 $actual_stage"
            fi
            ;;
        parse_error_*)
            if [ "$compiler_code" -ne 0 ] && [ "$actual_stage" = "parser" ]; then
                record_pass "$name" "按预期在 parser 阶段失败"
            else
                record_fail "$name" "应在 parser 阶段失败，实际退出码 $compiler_code，阶段 $actual_stage"
            fi
            ;;
        ir_error_*)
            if [ "$compiler_code" -ne 0 ] && { [ "$actual_stage" = "ir" ] || [ "$actual_stage" = "no_run_info" ]; }; then
                record_pass "$name" "按预期在 IR 阶段失败或异常退出"
            else
                record_fail "$name" "应在 IR 阶段失败，实际退出码 $compiler_code，阶段 $actual_stage"
            fi
            ;;
        ir_invalid_*)
            if [ "$compiler_code" -eq 0 ]; then
                check_ir_invalid_sample "$name" "$out_dir"
            else
                record_fail "$name" "IR 阶段应先生成 output.ll，实际退出码 $compiler_code，阶段 $actual_stage"
            fi
            ;;
        ok_*)
            if [ "$compiler_code" -eq 0 ]; then
                check_ok_ir_sample "$name" "$out_dir"
            else
                record_fail "$name" "IR 阶段应成功，实际退出码 $compiler_code，阶段 $actual_stage"
            fi
            ;;
        *)
            record_fail "$name" "未知命名格式"
            ;;
    esac
}

main() {
    ensure_compiler
    ensure_clang
    prepare_run_dir "$RUN_DIR"

    local sample
    for sample in "$SAMPLE_DIR"/*.sy; do
        local file
        local name
        local out_dir
        file="$(basename "$sample")"
        name="${file%.sy}"
        out_dir="$RUN_DIR/$name"
        check_ir_sample "$sample" "$name" "$out_dir"
    done

    print_summary "ir sample test" "$RUN_DIR"
}

main "$@"
