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

### 2026.5.8

今天没有直接推进 SLR 核心表构造，先做了一层 Parser 周边的诊断增强，避免后面 SLR 一上来就面对质量很差的 token 流。这个功能不替代正式语法分析，只作为进入 SLR 前的预检和调试辅助。

已完成：

- 新增公共诊断结构 `Diagnostic`，后续语法分析错误可以和词法错误共用统一格式。
- 在 `ParseResult` 中增加 `diagnostics` 字段，后续 Parser 可以返回结构化错误，而不仅是一段普通字符串。
- 增加 `TokenStreamSummary`，用于记录 token 总数、EOF 数量、最大括号深度、最大代码块深度和诊断列表。
- 在 `Parser` 中新增 `precheckTokenStream(...)` 接口，专门做 SLR 前的 token 流质量检查。
- 实现括号和大括号匹配检查：能发现多余的 `)`、`}`，也能发现没有闭合的 `(`、`{`。
- 实现 EOF 检查：能发现缺少 EOF、重复 EOF、EOF 后仍有 token 等输入流问题。
- 实现 `else` 上下文预检：对明显没有可见匹配 `if` 的 `else` 给出错误。
- 实现二元运算符右操作数检查，例如 `return a +;` 会在进入 SLR 前给出更清楚的提示。
- 实现相邻表达式 token 的 warning，例如两个标识符或常量异常相邻时给出提醒。
- 在 `parser_main` 中增加 `PARSER_PREFLIGHT` 环境变量开关，可以单独输出 token 流预检摘要。
- 增加 JSON Lines 诊断输出，`DIAGNOSTICS_JSON=1` 时可以把语法预检问题输出成机器可读格式。
- 新增 `docs/parser_diagnostics.md`，说明这层诊断和正式 SLR 分析的关系，避免和后续计划任务混淆。
- 新增 `tests/samples/parser_preflight_syk.sy`，用于覆盖括号不匹配和操作数缺失等语法预检场景。

这部分的价值主要在于：

- 后续实现 SLR 时，可以先调用 `precheckTokenStream` 过滤掉明显结构错误。
- 测试同学可以拿预检摘要辅助定位“词法没问题但语法输入流明显坏掉”的样例。
- 后续正式 SLR 报错也可以复用同一套 `Diagnostic`，把状态栈、当前 token 和期望符号组合成更友好的错误信息。

Todo list 补充：

- [x] 增加 Parser token 流预检接口。
- [x] 增加括号/大括号匹配诊断。
- [x] 增加 EOF 完整性诊断。
- [x] 增加二元运算符缺少操作数的提前诊断。
- [x] 增加 parser 诊断文档和预检样例。
- [ ] 后续 SLR 完成后，把分析表报错也接入 `Diagnostic`。

### 2026.5.8 晚

最后又做了一个偏展示和工程体验的工具：C-- 实时命令行编辑器。这个工具不属于 SLR 主线算法，但可以把词法分析、语法预检、诊断输出和源码展示串起来，后面答辩或联调时比较直观。

已完成：

- 新增 `tests/drivers/live_editor_main.cpp`，实现独立的 `cminus_editor` 命令行编辑器。
- 在 `Makefile` 中新增 `editor` 和 `run-editor` 目标，可以用 `make editor` 编译，用 `make run-editor` 打开默认样例。
- 支持打开和保存 `.sy` 文件，命令包括 `:open`、`:save`、`:show`、`:insert`、`:append`、`:replace`、`:delete` 等。
- 支持实时分析：每次插入、替换、删除源码后，自动调用 `Lexer::tokenize(...)` 和 `Parser::precheckTokenStream(...)`。
- 支持语法高亮显示：关键字、标识符、数字、运算符、界符分别使用不同 ANSI 颜色展示。
- 支持 `:tokens` 查看当前缓冲区的 token 表，方便检查 Parser 收到的输入流。
- 支持 `:analyze` 输出完整诊断，包括词法错误、语法预检错误、warning、token 数量、括号深度和代码块深度。
- 支持 `:live on/off` 开关实时分析，支持 `:color on/off` 或 `NO_COLOR=1` 关闭颜色，兼容不支持 ANSI 的终端。
- 新增 `docs/live_editor.md`，说明编辑器的命令、使用方式、展示能力和它与正式 Parser 的关系。

这个工具的意义：

- 可以实时展示词法模块和语法预检模块的效果，比单独跑测试文件更直观。
- 后续 SLR Parser 完成后，可以继续把真正的规约错误接到编辑器里，形成一个简易前端 IDE。
- 对测试同学也有帮助：可以边改样例边看 token 和预检诊断，不用每次手动跑多个 driver。

Todo list 补充：

- [x] 实现命令行编辑器基本缓冲区操作。
- [x] 接入词法分析和 Parser 预检。
- [x] 实现 ANSI 语法高亮。
- [x] 支持 token 表查看和实时诊断摘要。
- [x] 补充编辑器使用文档。
- [ ] 后续正式 SLR 完成后，将规约栈和期望符号也显示到编辑器诊断面板中。
