## 开发日志（syk）

### 2026.4.27 - 2026-04-29
主要是把项目框架先搭起来，方便大家后面并行开发，做了这些事：

- 把主流程梳理了：输入 `input.sy` 后，按 **词法 -> 语法 -> IR** 走完整条链路，完整逻辑在`src/main.cpp`中。
- 补了最小骨架代码：`lexer/parser/IRGenerator` 的头文件和占位实现先放好，后续每个人可以直接接着填。
- 写了框架文档 `docs/architecture.md`，把数据流、输出文件、错误处理和协作规则先定下来。
- 规划了测试方向：准备 3 类样例（正常通过、词法报错、语法报错），后面用于联调和回归。

我这边接下来重点做语法分析实现，同时把 AST 和规约日志格式定死。

### 2026.5.5

把 Parser 和 IRGenerator 之间的 AST 约定定下来
已完成：

- 在 `include/c--/parser/AST.h` 中补充了统一的 AST 节点名，例如 `CompUnit`、`FuncDef`、`VarDecl`、`BinaryExpr` 等。
- 增加了 `createNode(...)` 辅助函数，后面创建 AST 节点时尽量统一用这个函数，避免到处手写字符串。
- 新增 `docs/AST_SPEC.md`，用比较简洁的方式说明每种 AST 节点的 `name`、`value` 和 `children` 顺序。
- 和中间代码生成部分对齐了最小开发范围：先支持变量声明、变量赋值、函数定义、代码块、表达式和 `return`，后续再补 `if/else`、函数调用和更完整的类型处理。

词法分析部分做了迁移和适配：

- 把之前实验三 `lab3` 中的自动机构造代码迁移到当前项目，新增 `include/c--/lexer/automata.h` 和 `src/lexer/automata.cpp`。
- 保留了 NFA 构造、NFA 到 DFA 的确定化、DFA 最小化和最长匹配扫描这些核心逻辑。
- 将原来直接 `printf` 输出 token 的方式，改成当前大作业框架需要的 `Lexer::tokenize(...) -> LexResult`。
- 按大作业文档修正了关键字、运算符、界符的属性编号，并补充了 Parser 需要的 `grammar` 字段，例如 `IDN -> Ident`、`INT -> IntConst`、`FLOAT -> floatConst`。
- 将浮点数规则调整为 `[0-9]+'.'[0-9]+`，和附录文法保持一致。
- 额外支持了单独的 `!`，因为语法要求中有一元表达式 `!0`。
- 用临时小程序做了词法冒烟测试，确认变量声明、浮点数、`if`、`return`、`!=`、`||`、`!` 等 token 可以正常识别。
### 2026.5.6

模块化测试部分做了临时方案：

- 新增顶层 `Makefile`，先用于阶段测试，后面完整流程稳定后再整理正式构建脚本。
- 新增 `tests/drivers/lexer_main.cpp`，用于单独测试词法分析阶段，输入 `.sy` 文件，输出临时格式 `token.tsv`。
- 新增 `tests/drivers/parser_main.cpp`，用于单独测试语法分析阶段，输入 `token.tsv`，输出 `ast.txt` 和 `reduce.txt`。
- 新增 `tests/drivers/ir_main.cpp`，用于单独测试中间代码生成阶段，输入 `ast.txt`，输出 `output.ll`。
- 新增 `tests/drivers/driver_utils.h`，放置测试 driver 共用的文件读写、token 表读写、AST 文本读写工具函数。
- 在 `README.md` 中补充了三阶段测试的输入输出约定和临时 Makefile 使用方法。
- 已验证 `make lexer_test parser_test`、`make ir_test`、`make run-lexer` 可以执行；其中 Parser 目前还是占位实现，所以 `run-parser` 暂时只用于后续联调。

接下来我的重点是继续做语法分析器，先把作业要求里 SLR 语法分析相关的内容完成。


### 2026.5.7

语法分析部分完成了第一版实现：

- 按课程要求实现了 SLR 语法分析流程，包括 FIRST 集、FOLLOW 集、LR(0) 项目集规范族、SLR 分析表、移进/规约分析栈和语法错误提示。
- 在 `src/parser/parser.cpp` 中把 Parser 从占位实现改成可运行版本，输入词法阶段生成的 token 序列，输出 AST 和规约日志。
- 对附录文法做了等价左因子化，解决顶层 `int a;` 和 `int add(...)` 都以 `int Ident` 开头导致的 SLR 冲突。
- 在 `grammar/c--_bnf.txt` 中写入当前 Parser 实际使用的 BNF 文法，方便后面写报告和检查。
- 已验证 `case1_ok` 正常通过，`case3_parse_error` 可以输出语法错误信息。
- 额外用包含常量声明、变量初始化、函数参数、函数调用、复合表达式、`if/else` 的临时样例做了语法冒烟测试。

Todo list：

- [x] 整理附录文法，把文档里的 EBNF 写法改成普通 BNF，例如把 `*`、`?`、`|` 展开成明确产生式。
- [x] 统一词法 token 和文法终结符的名字，例如 `IDN` 对应 `Ident`，`INT` 对应 `IntConst`。
- [x] 在 `grammar/c--_bnf.txt` 中写入最终使用的 BNF 文法，作为 Parser 构造分析表的依据。
- [x] 设计产生式结构体，记录产生式编号、左部、右部，方便后面输出规约日志。
- [x] 实现 FIRST 集合计算。
- [x] 实现 FOLLOW 集合计算。
- [x] 实现 LR(0) 项目、项目集闭包 `closure` 和转移 `goto`。
- [x] 构造 LR(0) 项目集规范族。
- [x] 根据 LR(0) 项目集和 FOLLOW 集合构造 SLR 分析表。
- [x] 检查 SLR 表是否存在冲突，重点注意 `if-else` 和表达式优先级相关问题。
- [x] 实现 SLR 分析栈，完成移进、规约、接受和报错流程。
- [x] 按作业要求输出规约过程，格式类似 `序号 栈顶符号#面临输入符号 动作`。
- [x] 在规约时同步构造 AST，AST 结构必须符合 `docs/AST_SPEC.md`。
- [x] 先跑通最小样例：全局变量、`main` 函数、局部变量、赋值、表达式、`return`。
- [x] 补充语法错误信息，至少能说明出错 token、行列号和当前期望的符号。
- [ ] 整理语法分析部分的测试样例和输出截图，给测试报告使用。


### 2026.5.8
命令行 C-- 编辑器初步实现：

- 新增 `src/tools/cminus_editor.cpp`，使用 `ncurses` 做终端界面。
- 左侧为 C-- 代码编辑区，支持插入、删除、换行、方向键移动、保存、恢复到上次保存内容和退出。
- 右侧上半区实时显示词法分析 token 输出，右侧下半区实时显示语法分析结果、AST 摘要和规约日志。
- 编辑区根据词法分析结果做基础语法高亮，区分关键字、标识符、数字、运算符、界符。
- 在 `Makefile` 中增加 `editor` 和 `run-editor` 目标，在 `README.md` 中补充运行方式和快捷键。
- 已验证 `make editor` 可以编译通过。
- 补充右侧 Tokens/Parser 面板滚动：鼠标点击面板后可用滚轮滚动，也支持键盘 `PageUp/PageDown/Home/End` 滚动当前面板。
- 增加鼠标兼容处理：显式开启 xterm 鼠标上报，并在 ncurses 没解析出 `KEY_MOUSE` 时手动解析常见鼠标转义序列。
- 增加联动高亮：编辑区光标所在 token 会在源码、Token 面板和 Parser 面板相关行中高亮显示。
- 增加键盘兜底操作：`F1/F2/F3` 切换焦点，`[`/`]` 滚动当前焦点区域，避免终端鼠标不兼容时无法演示。
- 修复 macOS ncurses 下滚轮向下不生效的问题：为缺失的 `BUTTON5_PRESSED` 补真实位掩码，并统一处理 xterm 滚轮上下方向。

### 2026.5.9
- 增加编辑区错误标记：从词法/语法错误信息中提取行列号，在源码左侧编辑区用红色加粗下划线标记错误 token；行尾或空行错误用红色 `~` 标出。
- 增加编辑器自动缩进：回车继承当前行前导空格，`{` 后回车自动增加两个空格缩进。
- 修复粘贴代码时缩进被二次加工的问题：启用 bracketed paste 模式，粘贴期间按原文批量插入，结束后只重新分析一次。
- 修复触控板滚动距离过短的问题：滚轮事件改为按当前区域可见高度动态滚动，编辑区约半屏、右侧面板约半个面板；`[`/`]` 仍保留小步滚动。

### 2026.5.10
- 新增 `docs/parser_design.md`，详细整理语法分析器实现，包括文法设计、FIRST/FOLLOW 计算、LR(0) 项目集构造、SLR 分析表、移进规约流程、错误处理和 AST 构造规则。
- 文档中结合 `src/parser/parser.cpp` 的关键代码说明 `Grammar`、`SLRTable`、`Parser::parse`、`buildSemantic` 等核心类和方法，方便后续写报告和组员阅读语法分析实现。
