#!/usr/bin/env bash

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SAMPLE_DIR="$ROOT_DIR/tests"
BUILD_DIR="$ROOT_DIR/build"
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

ensure_compiler() {
    if [ ! -d "$SAMPLE_DIR" ]; then
        echo "sample directory not found: $SAMPLE_DIR"
        exit 1
    fi

    make -C "$ROOT_DIR" c--compiler >/dev/null
    if [ ! -x "$COMPILER" ]; then
        echo "compiler not found: $COMPILER"
        exit 1
    fi
}

ensure_clang() {
    if ! command -v clang >/dev/null 2>&1; then
        echo "clang not found; cannot verify LLVM IR"
        exit 1
    fi
}

prepare_run_dir() {
    local run_dir="$1"
    rm -rf "$run_dir"
    mkdir -p "$run_dir"
}

run_compiler_stage() {
    local stage="$1"
    local sample="$2"
    local out_dir="$3"

    rm -rf "$out_dir"
    mkdir -p "$out_dir"
    bash -c '"$1" "$2" "$3" -o "$4"; code=$?; exit "$code"' _ "$COMPILER" "$stage" "$sample" "$out_dir" >"$out_dir/compiler.stdout" 2>"$out_dir/compiler.stderr"
}

print_summary() {
    local title="$1"
    local run_dir="$2"

    echo
    echo "$title summary:"
    echo "  passed: $PASS_COUNT"
    echo "  failed: $FAIL_COUNT"
    echo "  output: $run_dir"

    if [ "$FAIL_COUNT" -ne 0 ]; then
        exit 1
    fi
}
