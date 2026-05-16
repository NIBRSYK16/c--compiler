# C-- Compiler 项目框架

## 1. 目标

实现 C-- 编译流程的三个阶段：

1. 词法分析（Lex）
2. 语法分析（Parse）
3. 中间代码生成（IR）

入口位是 `src/main.cpp`，按阶段串行执行。

---

## 2. 数据流

输入文件：`input.sy`

数据流：

`input.sy`（文件）
-> `source`（`std::string`）
-> `tokens`（`std::vector<Token>`）
-> `AST`（`ASTNode` 树，根节点由 `ParseResult` 管理）
-> `irText`（`std::string`）

输出文件（位于本次输出目录）：

- `input.sy`：输入副本
- `token.txt`：词法分析结果
- `reduce.txt`：语法规约日志
- `ast.txt`：语法树文本
- `output.ll`：LLVM IR 文本
- `error.txt`：错误信息（无错误时为空）
- `run_info.txt`（或同类运行信息文件）：本次运行阶段和状态

---

## 3. 规定和约束

### 3.1 词法分析阶段

- 输入：`std::string source`
- 输出：`LexResult`
  - `success`
  - `tokens`
  - `errorMessage`

`Token` 字段语义：

- `lexeme`：源代码原始词素
- `type`：Token 类型（Keyword/Identifier/Integer/...）
- `grammar`：供语法分析使用的文法终结符（如 `int`、`Ident`、`IntConst`）
- `attr`：属性值（标识符名、数值文本、关键字编号等）
- `line`/`column`：位置信息（从 1 开始）

### 3.2 语法分析阶段

- 输入：`std::vector<Token>`
- 输出：`ParseResult`
  - `success`
  - `root`（AST 根）
  - `reduceLogs`
  - `errorMessage`

AST 节点约定：

- `name`：节点类型（如 `CompUnit`、`FuncDef`、`ReturnStmt`、`BinaryExpr`）
- `value`：节点附加值（如标识符名、常量值、运算符）
- `children`：子节点列表（有序）

### 3.3 IR 生成阶段

- 输入：`ASTNode* root`
- 输出：`IRResult`
  - `success`
  - `irText`
  - `errorMessage`

---

## 4. 输出文件格式规范

### 4.1 token.txt

每行一个 token，格式：

`<待测代码中的单词符号>\t<单词符号种别,单词符号内容>`

例如：

```text
int	<KW,1>
a	<IDN,a>
=	<OP,11>
10	<INT,10>
;	<SE,24>
```

其中 `KW` 表示关键字，`OP` 表示运算符，`SE` 表示界符，`IDN` 表示标识符，`INT` 表示整数，`FLOAT` 表示浮点数。

### 4.2 reduce.txt

每行一条 SLR 分析动作记录，格式：

`<序号>\t<栈顶符号>#<面临输入符号>\t<执行动作>`

例如：

```text
1	$#int	move
2	Type#main	reduction
35	Program#EOF	accept
```

其中执行动作为 `move`、`reduction`、`accept` 或 `error`。

### 4.3 ast.txt

树状缩进格式，建议格式：

`<NodeName> value=<value>`

例如：

```text
CompUnit
  FuncDef value=main
    Type value=int
```

### 4.4 output.ll

LLVM IR 文本，按函数输出。

### 4.5 error.txt

无错误为空；
有错误时单行格式：

`[<stage>] <line>:<column> <message>`

例如：

`[lexer] 1:6 invalid token '@'`

---

## 5. 错误处理约束

- 任一阶段失败时，后续阶段不再执行。
- 即使失败，也必须生成：
  - `input.sy`
  - 已完成阶段的输出
  - `error.txt`
  - `run_info.txt`
- `error.txt` 必须写入人类可读错误。

---

## 6. 协作规范

- 分支命名：`dev_name`, 比如`dev_syk`。
- 在自己的仓库中的main分支修改，提交pr到组长仓库的`dev_*`分支中。
- Commit 粒度小、信息明确、只在自己的分支和对应文件上修改。
- 至少三天提交一次pr，即使没有代码的改动也要更新自己的开发日志`docs/dev_log_*.md`，如果没有进度就写：暂未改动。
