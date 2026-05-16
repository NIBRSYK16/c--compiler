# C-- 测试样例说明

本目录用于回归测试词法分析、语法分析和中间代码生成。文件命名规则如下：

- `ok_*.sy`：预期完整流程通过。
- `lex_error_*.sy`：预期词法分析阶段失败。
- `parse_error_*.sy`：预期词法通过、语法分析失败。
- `ir_error_*.sy`：预期词法和语法通过、IR 生成失败。
- `ir_invalid_*.sy`：编译器流程可能返回成功，但生成的 LLVM IR 不能通过 `clang` 校验。

注意：当前词法分析器支持 `//` 单行注释和 `/* ... */` 多行注释。注释会被跳过，不生成 token。

| 文件 | 预期 | 失败阶段 | 覆盖点 | 预期结果 |
|---|---|---|---|---|
| `ok_001_minimal_return.sy` | 正确 | 无 | 最小 `main`、`return` | 完整流程通过，退出码 0 |
| `ok_002_local_const_decl.sy` | 正确 | 无 | 局部常量声明、局部变量初始化 | 完整流程通过，退出码 8 |
| `ok_003_local_decl_assign.sy` | 正确 | 无 | 局部声明、初始化、赋值 | 完整流程通过，退出码 11 |
| `ok_004_arithmetic_precedence.sy` | 正确 | 无 | `+ - * /` 优先级 | 完整流程通过，退出码 20 |
| `ok_005_parenthesized_expression.sy` | 正确 | 无 | 括号表达式 | 完整流程通过，退出码 21 |
| `ok_006_unary_expression.sy` | 正确 | 无 | 一元 `+ - !` | 完整流程通过，退出码 6 |
| `ok_007_if_else_branch.sy` | 正确 | 无 | 整数条件的 `if/else` 分支 | 完整流程通过，退出码 1 |
| `ok_008_modulo_expression.sy` | 正确 | 无 | 取模表达式 `%` | 完整流程通过，退出码 2 |
| `ok_009_function_call.sy` | 正确 | 无 | 多函数定义、函数调用、参数传递 | 完整流程通过，退出码 14 |
| `ok_010_void_function_expr_stmt.sy` | 正确 | 无 | `void` 函数、表达式语句、空语句 | 完整流程通过，退出码 6 |
| `ok_011_block_scope.sy` | 正确 | 无 | 代码块和局部作用域遮蔽 | 完整流程通过，退出码 1 |
| `ok_012_line_comment.sy` | 正确 | 无 | `//` 单行注释 | 完整流程通过，退出码 11 |
| `ok_013_block_comment.sy` | 正确 | 无 | `/* ... */` 多行注释 | 完整流程通过，退出码 6 |
| `lex_error_101_illegal_at.sy` | 错误 | lexer | 非法字符 `@` | 报词法错误 |
| `lex_error_102_bad_float_dot.sy` | 错误 | lexer | 不完整浮点数 `1.` | 报词法错误 |
| `lex_error_103_non_ascii.sy` | 错误 | lexer | 非 ASCII 标识符 | 报词法错误 |
| `lex_error_104_array_bracket.sy` | 错误 | lexer | 数组符号 `[` `]` 未支持 | 报词法错误 |
| `lex_error_105_string_literal.sy` | 错误 | lexer | 字符串字面量未支持 | 报词法错误 |
| `lex_error_106_unclosed_block_comment.sy` | 错误 | lexer | 未闭合的多行注释 | 报词法错误 |
| `parse_error_201_missing_semicolon.sy` | 错误 | parser | 缺少分号 | 报语法错误 |
| `parse_error_202_keyword_as_identifier.sy` | 错误 | parser | 关键字作标识符 | 报语法错误 |
| `parse_error_203_unmatched_brace.sy` | 错误 | parser | 缺少右花括号 | 报语法错误 |
| `parse_error_204_bad_param_list.sy` | 错误 | parser | 函数形参列表多余逗号 | 报语法错误 |
| `parse_error_205_unsupported_while.sy` | 错误 | parser | 循环语句未支持 | 报语法错误 |
| `parse_error_206_missing_operand.sy` | 错误 | parser | 表达式缺少右操作数 | 报语法错误 |
| `ir_error_301_float_init.sy` | 错误 | ir | 浮点常量初始化未支持 | 报 IR 生成错误 |
| `ir_error_302_float_binary.sy` | 错误 | ir | 浮点运算未支持 | 报 IR 生成错误 |
| `ir_error_303_forward_function_call.sy` | 错误 | ir | 函数先调用后定义 | 报未定义函数 |
| `ir_error_304_undefined_variable.sy` | 错误 | ir | 未定义变量 | 报未定义变量 |
| `ir_error_305_duplicate_global.sy` | 错误 | ir | 重复全局变量 | 报重复定义 |
| `ir_error_306_call_arg_count.sy` | 错误 | ir | 函数调用实参数量错误 | 报参数数量不匹配 |
| `ir_error_307_float_decl_only.sy` | 错误 | ir | `float` 变量声明 | 当前 IR/第三方库对 `float` 支持不完整，可能报错或异常退出 |
| `ir_error_308_global_variable.sy` | 错误 | ir | 全局变量声明、初始化、读写 | 当前全局变量 IR 输出存在异常退出风险 |
| `ir_invalid_309_comparison_condition.sy` | 错误 | LLVM IR 校验 | 比较表达式作为 `if` 条件 | 编译器流程可能成功，但 `clang output.ll` 会报 `icmp ne i1 ..., i32 0` 类型错误 |
| `ir_invalid_310_nested_if_duplicate_label.sy` | 错误 | LLVM IR 校验 | 嵌套 `if/else` | 编译器流程可能成功，但 IR 中会出现重复基本块 label |
| `ir_invalid_311_logic_and_or.sy` | 错误 | LLVM IR 校验 | `&&` / `||` | 编译器流程可能成功，但当前逻辑运算生成的 IR 类型不匹配 |

推荐单个样例测试方式：

```bash
make c--compiler
build/c--compiler tests/ok_001_minimal_return.sy -o output/sample_check
```

如果样例预期完整流程通过，可以继续编译运行 LLVM IR：

```bash
clang output/sample_check/output.ll -o output/sample_check/a.out
./output/sample_check/a.out
echo $?
```

如果是 `ir_invalid_*.sy`，预期是本项目编译器可能先生成 `output.ll`，但继续执行下面命令时被 `clang` 发现 LLVM IR 不合法：

```bash
clang output/sample_check/output.ll -o output/sample_check/a.out
```
