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
- `src/tools`：辅助命令行工具，目前包含 C-- 实时编辑器。
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

## 临时单阶段测试

为了方便模块化开发，当前先提供一个临时 `Makefile`，分别测试词法、语法和 IR 三个阶段。后面三个阶段都完成后，再整理正式构建方式。

### 阶段输入约定

| 阶段 | 测试程序 | 输入 | 输出 |
|---|---|---|---|
| 词法分析 | `lexer_test` | `.sy` 源程序 | `token.tsv` |
| 语法分析 | `parser_test` | `token.tsv` | `ast.txt`、`reduce.txt` |
| 中间代码生成 | `ir_test` | `ast.txt` | `output.ll` |

说明：

- `token.tsv` 是临时测试格式，列为 `line column type grammar attr lexeme`，用于给 Parser 单独测试。
- `ast.txt` 使用 `docs/AST_SPEC.md` 中的缩进格式，例如 `FuncDef value=main`。
- Parser 已经实现，`run-parser` 会输出 AST 和移进/规约日志。

### 编译

```bash
make lexer_test
make parser_test
make ir_test
make editor
```

也可以一次性编译三个临时测试程序：

```bash
make all
```

### 运行词法分析

默认输入是 `tests/case1_ok/input.sy`：

```bash
make run-lexer
```

指定输入文件：

```bash
make run-lexer INPUT=tests/case1_ok/input.sy
```

输出默认写到：

```text
.tmp_build/token.tsv
```

### 运行语法分析

```bash
make run-parser
```

默认读取 `.tmp_build/token.tsv`，输出：

```text
.tmp_build/ast.txt
.tmp_build/reduce.txt
```

### 运行中间代码生成

```bash
make run-ir
```

默认读取 `.tmp_build/ast.txt`，输出：

```text
.tmp_build/output.ll
```

### 运行 C-- 命令行编辑器

编辑器使用 `ncurses` 实现，界面左侧是代码编辑区，右侧上半部分实时显示词法分析结果，右侧下半部分实时显示语法分析结果。

```bash
make editor
make run-editor FILE=tests/case1_ok/input.sy
```

快捷键：

- `Ctrl-S`：保存当前文件。
- `Ctrl-R`：取消修改，恢复到上次保存的内容。
- `Ctrl-Q`：退出编辑器。
- 方向键、`Home`、`End`、`PageUp`、`PageDown`：移动光标。
- `Enter`、`Backspace`、`Delete`：基础编辑。
- 鼠标点击右侧 `Tokens` 或 `Parser` 面板后，可以用滚轮滚动该面板。
- 右侧面板获得焦点后，也可以用方向键、`PageUp`、`PageDown`、`Home`、`End` 滚动内容。
- 如果终端没有正确传递鼠标事件，可以用 `F1/F2/F3` 切换焦点到编辑区、Tokens 面板、Parser 面板。
- `[` 和 `]` 可以滚动当前焦点区域。
- 编辑区光标落在某个 token 上时，左侧对应 token、右侧 token 输出行和语法分析中相关行会高亮。
- 词法/语法错误也会在左侧编辑区用红色加粗下划线标出；如果错误发生在行尾或空行，会在对应位置显示红色 `~`。
- 回车会自动继承上一行缩进；在 `{` 后回车会自动多缩进两个空格。

清理临时编译和输出文件：

```bash
make clean-temp
```

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
