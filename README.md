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

## 完整工具使用

现在项目可以编译成一个完整命令行工具：

```bash
make c--compiler
```

生成的程序是：

```text
build/c--compiler
```

使用格式：

```bash
build/c--compiler [lexer|parser|ir|ide|all] <file> [-o output_dir]
```

如果不写模式，默认执行完整流程，也就是词法分析、语法分析、中间代码生成三个阶段：

```bash
build/c--compiler tests/case1_ok/input.sy
```

常用模式：

```bash
build/c--compiler lexer tests/case1_ok/input.sy -o output/lexer_case
build/c--compiler parser tests/case1_ok/input.sy -o output/parser_case
build/c--compiler ir tests/case1_ok/input.sy -o output/ir_case
build/c--compiler ide tests/case1_ok/input.sy
```

说明：

- `lexer`：只执行词法分析，输出 `token.txt`。
- `parser`：执行词法分析和语法分析，输出 `token.txt`、`reduce.txt`、`ast.txt`。
- `ir` / `all`：执行完整三阶段，输出 `token.txt`、`reduce.txt`、`ast.txt`、`output.ll`。
- `ide`：打开命令行 C-- 编辑器，不生成输出目录。
- `-o` 可以指定输出目录；如果不指定，会自动生成 `output/result_YYYYMMDD_HHMMSS`。

## 临时单阶段测试

为了方便模块化开发，当前仍然保留单阶段测试目标，可以分别测试词法、语法和 IR 三个阶段。

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
make c--compiler
```

也可以一次性编译临时测试程序、编辑器和完整工具：

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
build/token.tsv
```

### 运行语法分析

```bash
make run-parser
```

默认读取 `build/token.tsv`，输出：

```text
build/ast.txt
build/reduce.txt
```

### 运行中间代码生成

```bash
make run-ir
```

默认读取 `build/ast.txt`，输出：

```text
build/output.ll
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
- 粘贴代码时使用 bracketed paste 模式按原文插入，不会对粘贴内容里的每个换行重新自动缩进。

清理临时编译和输出文件：

```bash
make clean-temp
```

## 待解决 Todo

- [x] 补齐正式主程序依赖的工具函数实现：`parseArgs`、`stageToString`、`printHelp`、`readTextFile`、`writeTextFile`、`writeTokens`、`writeLines`、`writeASTFile`、`writeRunInfo` 等。
- [x] 在 `Makefile` 中增加正式编译器目标 `c--compiler`，把 `src/main.cpp`、词法、语法、IR、公共工具和 `third_part/compiler_ir` 一起编译，形成一条命令跑完整流程。
- [x] 检查并统一正式工具的最终输出格式，`token.txt` 使用大作业要求的 `单词<TAB><种别,属性>` 格式，`reduce.txt` 输出移进/规约序列。
- [ ] 修复 `third_part/compiler_ir/include/IRbuilder.h` 中 `create_iand`、`create_ior` 目前错误调用 `create_sdiv` 的问题，否则 `&&` 和 `||` 生成的 LLVM IR 语义不正确。
- [ ] 明确 `float` 的支持范围。当前词法和语法支持 `float` / `floatConst`，但 `src/ir/IRGenerator.cpp` 中浮点常量、浮点变量初始化、浮点运算和浮点返回值会报错；如果老师测试包含浮点，需要继续补 IR 生成。
- [ ] 修正 `tests/samples/*_ok.sy` 中与当前规则冲突的样例，例如关键字不区分大小写时，`CONST` 会被识别为 `const` 关键字，不能作为普通标识符。
- [ ] 补充覆盖完整必做功能的回归测试：全局/局部变量、常量、函数参数、函数调用、赋值、一元/二元/复合表达式、比较、`&&` / `||`、`if/else`、词法错误、语法错误和 IR 生成错误。

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
