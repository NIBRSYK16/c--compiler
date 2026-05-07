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

什么都没干，在忙其他任务，没抽出时间推进编译原理的进度

Todo list：

- [ ] 整理附录文法，把文档里的 EBNF 写法改成普通 BNF，例如把 `*`、`?`、`|` 展开成明确产生式。
- [x] 统一词法 token 和文法终结符的名字，例如 `IDN` 对应 `Ident`，`INT` 对应 `IntConst`。
- [ ] 在 `grammar/c--_bnf.txt` 中写入最终使用的 BNF 文法，作为 Parser 构造分析表的依据。
- [ ] 设计产生式结构体，记录产生式编号、左部、右部，方便后面输出规约日志。
- [ ] 实现 FIRST 集合计算。
- [ ] 实现 FOLLOW 集合计算。
- [ ] 实现 LR(0) 项目、项目集闭包 `closure` 和转移 `goto`。
- [ ] 构造 LR(0) 项目集规范族。
- [ ] 根据 LR(0) 项目集和 FOLLOW 集合构造 SLR 分析表。
- [ ] 检查 SLR 表是否存在冲突，重点注意 `if-else` 和表达式优先级相关问题。
- [ ] 实现 SLR 分析栈，完成移进、规约、接受和报错流程。
- [ ] 按作业要求输出规约过程，格式类似 `序号 栈顶符号#面临输入符号 动作`。
- [ ] 在规约时同步构造 AST，AST 结构必须符合 `docs/AST_SPEC.md`。
- [ ] 先跑通最小样例：全局变量、`main` 函数、局部变量、赋值、表达式、`return`。
- [ ] 补充语法错误信息，至少能说明出错 token、行列号和当前期望的符号。
- [ ] 整理语法分析部分的测试样例和输出截图，给测试报告使用。
