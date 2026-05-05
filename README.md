# C-- Compiler

TJU 2026 spring "Principles and Techniques of Compilation" final project.

C-- 语言编译器前端，并生成 LLVM IR 文本文件。

## 任务要求
- 词法分析器：手写基于自动机的词法分析，完成 NFA 到 DFA 的确定化和 DFA 最小化，输出 token 序列。
- 语法分析器：手写 SLR 语法分析器，构造 FIRST/FOLLOW 集合、LR(0) 项目集规范族和 SLR 分析表，输出移进/规约过程。
- AST 构建：在语法分析过程中生成语法树，供中间代码生成阶段遍历。
- 中间代码生成：遍历 AST，调用提供的 `compiler_ir` 中端代码，输出 `.ll` 格式 LLVM IR。

C-- 支持：

- 变量和常量声明，例如 `int a;`、`int a = 3;`、`const int a = 3;`
- 变量赋值，例如 `a = 3;`、`a = b;`
- 函数定义、返回类型和参数，例如 `int main()`、`int add(int a, int b)`
- 表达式运算，包括 `+ - * / %`、一元 `+ - !`、比较运算和逻辑运算
- 代码块、`if/else`、`return`

## 目录说明

- `src/lexer`：词法分析实现。
- `src/parser`：语法分析和 AST 构建实现。
- `src/ir`：中间代码生成实现。
- `include/c--`：项目头文件。
- `grammar`：文法文件。
- `docs`：开发日志、AST 约定、架构说明等文档。
- `tests`：测试输入和输出结果。
- `third_part/compiler_ir`：课程提供的中端代码。

## 输出文件

程序运行后计划输出：

- `token.txt`：词法分析 token 序列。
- `reduce.txt`：语法分析移进/规约过程。
- `ast.txt`：AST 文本。
- `output.ll`：LLVM IR。
- `error.txt`：错误信息。
- `run_info.txt`：本次运行信息。

## 注意事项

- 不使用 Lex/Yacc、ANTLR 等自动生成工具。
- 语法分析部分按 SLR 方法实现。
- 中间代码生成部分使用 C++，并对接 `third_part/compiler_ir`。
- `build/` 是本地构建产物目录，不提交到仓库。

## 分工
syk: 语法分析器

kmr: 中间代码生成

lhc: 词法分析

yhn: 测试+撰写

lzl: 测试+撰写
