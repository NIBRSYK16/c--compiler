# C-- Compiler

TJU 2026 spring "Principles and Techniques of Compilation" final project.

C-- 语言编译器前端，并生成 LLVM IR 文本文件。

## 任务要求
- 词法分析器：手写基于自动机的词法分析，完成 NFA 到 DFA 的确定化和 DFA 最小化，输出 token 序列。
- 语法分析器：手写 SLR 语法分析器，构造 FIRST/FOLLOW 集合、LR(0) 项目集规范族和 SLR 分析表，输出移进/规约过程。
- AST 构建：在语法分析过程中生成语法树，供中间代码生成阶段遍历。
- 语义分析器：独立遍历 AST，检查符号表、作用域、类型、函数调用和返回语句。
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
- `src/semantic`：语义分析实现。
- `src/ir`：中间代码生成实现。
- `src/tools`：辅助命令行工具，目前包含 C-- 实时编辑器。
- `include/c--`：项目头文件。
- `grammar`：文法文件。
- `docs`：开发日志、AST 约定、架构说明等文档。
- `tests`：测试样例和测试驱动程序，样例直接放在该目录下。
- `third_part/compiler_ir`：课程提供的中端代码。

## 输出文件

程序运行后计划输出：

- `token.txt`：词法分析 token 序列。
- `reduce.txt`：语法分析移进/规约过程，格式为 `序号<TAB>栈顶符号#面临输入符号<TAB>执行动作`。
- `ast.txt`：AST 文本。
- `semantic.txt`：语义分析记录。
- `output.ll`：LLVM IR。
- `error.txt`：错误信息。
- `run_info.txt`：本次运行信息。

其中 `token.txt` 按大作业要求输出为：

```text
待测代码中的单词符号<TAB><单词符号种别,单词符号内容>
```

例如 `int	<KW,1>`、`a	<IDN,a>`、`=	<OP,11>`。

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
build/c--compiler [lexer|parser|semantic|ir|ide|all] <file> [-o output_dir]
```

如果不写模式，默认执行完整流程，也就是词法分析、语法分析、语义分析、中间代码生成：

```bash
build/c--compiler tests/ok_001_minimal_return.sy
```

常用模式：

```bash
build/c--compiler lexer tests/ok_001_minimal_return.sy -o output/lexer_case
build/c--compiler parser tests/ok_001_minimal_return.sy -o output/parser_case
build/c--compiler semantic tests/ok_001_minimal_return.sy -o output/semantic_case
build/c--compiler ir tests/ok_001_minimal_return.sy -o output/ir_case
build/c--compiler ide tests/ok_001_minimal_return.sy
```

说明：

- `lexer`：只执行词法分析，输出 `token.txt`。
- `parser`：执行词法分析和语法分析，输出 `token.txt`、`reduce.txt`、`ast.txt`。
- `semantic`：执行词法分析、语法分析和语义分析，输出 `token.txt`、`reduce.txt`、`ast.txt`、`semantic.txt`。
- `ir` / `all`：执行完整流程，输出 `token.txt`、`reduce.txt`、`ast.txt`、`semantic.txt`、`output.ll`。
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

默认输入是 `tests/ok_001_minimal_return.sy`：

```bash
make run-lexer
```

指定输入文件：

```bash
make run-lexer INPUT=tests/ok_001_minimal_return.sy
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

### 回归测试脚本

规范测试样例直接放在 `tests/` 目录下，说明见 `tests/README.md`。可以用下面脚本批量测试：

```bash
scripts/run_samples.sh
scripts/run_sample_lexer.sh
scripts/run_sample_parser.sh
scripts/run_sample_semantic.sh
scripts/run_sample_ir.sh
```

其中 `run_samples.sh` 会跑完整流程；后四个脚本分别测试词法、语法、语义和 IR 阶段。

### 运行 C-- 命令行编辑器

编辑器使用 `ncurses` 实现，界面左侧是代码编辑区，右侧上半部分实时显示词法分析结果，右侧下半部分实时显示语法分析结果。

```bash
make editor
make run-editor FILE=tests/ok_001_minimal_return.sy
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
- [x] 增加独立语义分析阶段，先检查符号表、作用域、类型、函数调用和返回语句，再进入 IR 生成。
- [x] 避开 `third_part/compiler_ir/include/IRbuilder.h` 中 `create_iand`、`create_ior` 的问题，使用条件跳转和基本块生成 `&&`、`||` 的短路求值 IR。
- [ ] 明确 `float` 的支持范围。当前词法和语法支持 `float` / `floatConst`，但 `src/ir/IRGenerator.cpp` 中浮点常量、浮点变量初始化、浮点运算和浮点返回值会报错；如果老师测试包含浮点，需要继续补 IR 生成。
- [x] 整理 `tests` 目录结构，将规范样例直接放在 `tests/` 下，并补充词法、语法、IR 和完整流程回归测试脚本。

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


## 后续拓展

下面这些方向都比较合理，适合后续长期维护时逐步加入：

- [x] 注释功能：支持 `//` 单行注释和 `/* ... */` 多行注释。
- [ ] 循环功能：支持 `while`、`break`、`continue`，并补充对应 AST 和 IR 生成。
- [ ] 输出语句：支持 `print` / `write` 一类输出语句，或者对接运行时库函数。
- [ ] 内嵌 IR：允许在 C-- 源码中插入特定 IR 片段，用于调试和实验。
- [ ] 数组功能：支持一维数组的声明、访问、赋值和参数传递。
- [x] 独立语义分析阶段：把变量定义检查、作用域检查、类型检查、函数调用检查从 IR 生成中拆出来。
- [ ] 更完整的 `float` 支持：补齐浮点变量初始化、浮点运算、浮点比较和浮点返回值。
- [x] 逻辑运算短路求值：让 `&&` 和 `||` 按 C 语言习惯短路执行，而不是直接普通二元运算。
- [ ] 生成可执行文件：在 `c--compiler` 中增加 `-S`、`-c`、`-run` 等选项，直接生成汇编、目标文件或可执行文件。
- [ ] IR 优化：加入常量折叠、死代码删除、简单代数化简等基础优化。
- [ ] 错误提示优化：让词法、语法、语义错误都能显示源码行、错误位置和更清楚的提示。
- [ ] IDE 增强：增加跳转到错误位置、自动补全、格式化、文件树、多文件编辑等功能。
- [ ] IDE 断点调试：支持设置断点、单步执行和查看变量值，初期可以基于 AST 或 IR 解释执行实现。
- [ ] 测试体系增强：加入更多大样例、自动对比期望输出，并接入 GitHub Actions 等 CI。
- [ ] 文档完善：补充设计报告、模块接口说明、AST 到 IR 的对应关系和二次开发指南。
