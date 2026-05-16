#!/usr/bin/env bash

set -u

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$SCRIPT_DIR/sample_test_common.sh"

RUN_DIR="${1:-$BUILD_DIR/sample_parser_runs}"

check_parser_sample() {
    local sample="$1"
    local name="$2"
    local out_dir="$3"

    run_compiler_stage "parser" "$sample" "$out_dir"
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
        *)
            if [ "$compiler_code" -eq 0 ] && [ -f "$out_dir/ast.txt" ] && [ -f "$out_dir/reduce.txt" ]; then
                record_pass "$name" "语法分析通过，已生成 ast.txt 和 reduce.txt"
            else
                record_fail "$name" "语法分析应通过，实际退出码 $compiler_code，阶段 $actual_stage"
            fi
            ;;
    esac
}

main() {
    ensure_compiler
    prepare_run_dir "$RUN_DIR"

    local sample
    for sample in "$SAMPLE_DIR"/*.sy; do
        local file
        local name
        local out_dir
        file="$(basename "$sample")"
        name="${file%.sy}"
        out_dir="$RUN_DIR/$name"
        check_parser_sample "$sample" "$name" "$out_dir"
    done

    print_summary "parser sample test" "$RUN_DIR"
}

main "$@"
