# 语法分析器实现说明

本文档说明当前项目语法分析器的设计和实现细节。语法分析器采用 **SLR 自底向上分析方法**，主要完成这些工作：

- 根据 C-- 文法构造产生式列表。
- 自动计算 FIRST 集和 FOLLOW 集。
- 构造 LR(0) 项目集规范族。
- 根据 LR(0) 项目集和 FOLLOW 集构造 SLR 分析表。
- 使用分析栈对 token 序列进行移进、规约、接受或报错。
- 在规约时同步构造 AST。
- 输出规约日志，供测试报告和调试使用。

相关源码：

- [Common.h](../include/c--/common/Common.h)：词法分析输出的 token 数据结构和 `ParseResult` 返回结构。
- [AST.h](../include/c--/parser/AST.h)：AST 节点结构和节点名约定。
- [parser.h](../include/c--/parser/parser.h)：`Parser` 类接口。
- [parser.cpp](../src/parser/parser.cpp)：SLR 语法分析器完整实现。
- [c--_bnf.txt](../grammar/c--_bnf.txt)：当前 Parser 使用的 BNF 文法说明。
- [AST_SPEC.md](AST_SPEC.md)：AST 节点结构约定。

## 1. 总体流程

语法分析器入口是 [Parser::parse](../src/parser/parser.cpp#L856)：

```cpp
ParseResult Parser::parse(const std::vector<Token>& tokens);
```

输入是词法分析器生成的 token 序列，输出是：

```cpp
struct ParseResult {
    bool success = false;
    std::unique_ptr<ASTNode> root;
    std::vector<std::string> reduceLogs;
    std::string errorMessage;
};
```

整体流程如下：

```text
token 序列
  -> 构造 Grammar
  -> 计算 FIRST / FOLLOW
  -> 构造 LR(0) 项目集规范族
  -> 构造 SLR ACTION / GOTO 表
  -> 使用状态栈和语义值栈进行分析
  -> 移进时压入 token
  -> 规约时弹出右部并构造 AST 节点
  -> 接受时返回 AST 根节点
  -> 出错时返回错误行列和期望符号
```

代码中每次调用 `parse` 都会重新构造文法和分析表。这样实现比较直接，适合课程项目和调试；如果后续追求性能，可以把 `Grammar` 和 `SLRTable` 做成静态缓存。

## 2. 核心数据结构

语法分析器内部数据结构都放在 [parser.cpp](../src/parser/parser.cpp) 的匿名命名空间中，只给 `Parser::parse` 使用。

### 2.1 Production

定义位置：[parser.cpp](../src/parser/parser.cpp#L20)

```cpp
struct Production {
    std::string lhs;
    std::vector<std::string> rhs;
    std::string tag;
};
```

`Production` 表示一条产生式。

字段含义：

- `lhs`：产生式左部，例如 `Stmt`。
- `rhs`：产生式右部，例如 `LVal = Exp ;`。
- `tag`：语义动作标签，用来告诉 `buildSemantic` 规约时应该怎么构造 AST。

例子：

```cpp
add("Stmt", {"LVal", "=", "Exp", ";"}, "assign_stmt");
add("Stmt", {"return", "ReturnExpOpt", ";"}, "return_stmt");
add("AddExp", {"AddExp", "AddOp", "MulExp"}, "binary_expr");
```

这里的 `assign_stmt`、`return_stmt`、`binary_expr` 不是文法符号，而是我们自己给语义动作起的名字。

### 2.2 Item

定义位置：[parser.cpp](../src/parser/parser.cpp#L26)

```cpp
struct Item {
    int production = 0;
    int dot = 0;
};
```

`Item` 表示 LR(0) 项目，也就是“产生式 + 点的位置”。

例如产生式：

```text
Stmt -> LVal = Exp ;
```

会产生这些 LR(0) 项目：

```text
Stmt -> . LVal = Exp ;
Stmt -> LVal . = Exp ;
Stmt -> LVal = . Exp ;
Stmt -> LVal = Exp . ;
```

在代码里：

- `production` 是产生式编号。
- `dot` 是点在右部中的位置。

`Item` 实现了 `operator<` 和 `operator==`，是为了能放进 `std::set<Item>`，方便比较项目集是否相同。

### 2.3 Action 和 ActionType

定义位置：[parser.cpp](../src/parser/parser.cpp#L42)

```cpp
enum class ActionType {
    Shift,
    Reduce,
    Accept
};

struct Action {
    ActionType type = ActionType::Shift;
    int value = -1;
};
```

`Action` 表示 SLR 分析表中的 ACTION 项。

含义：

- `Shift`：移进，`value` 表示下一个状态编号。
- `Reduce`：规约，`value` 表示使用哪条产生式规约。
- `Accept`：接受，表示分析成功。

例如：

```text
ACTION[3, int] = Shift 8
ACTION[10, ;] = Reduce 23
ACTION[5, EOF] = Accept
```

### 2.4 SemanticValue

定义位置：[parser.cpp](../src/parser/parser.cpp#L53)

```cpp
struct SemanticValue {
    std::string symbol;
    std::string text;
    int line = -1;
    int column = -1;
    ASTNode* ast;
    std::vector<ASTNode*> list;
};
```

`SemanticValue` 是语义值栈中保存的数据。它不只保存文法符号，还保存 AST 构造需要的信息。

字段含义：

- `symbol`：当前语义值对应的文法符号，例如 `Exp`、`Stmt`、`Ident`。
- `text`：终结符文本，例如变量名 `a`、操作符 `+`、类型 `int`。
- `line` / `column`：源码位置，用于 AST 节点和错误提示。
- `ast`：如果该符号已经构造出一个 AST 节点，就放在这里。
- `list`：如果该符号表示节点列表，例如参数列表、语句列表，就放在这里。

为什么需要 `list`？

因为很多文法是递归列表，例如：

```text
BlockItems -> BlockItem BlockItems
ParamListTail -> , Param ParamListTail
```

这类规约结果不是单个节点，而是一组节点，所以用 `list` 暂存。

## 3. Grammar 类

定义位置：[parser.cpp](../src/parser/parser.cpp#L79)

`Grammar` 负责保存文法，并计算 FIRST 集和 FOLLOW 集。

主要成员：

```cpp
std::vector<Production> productions;
std::map<std::string, std::vector<int> > byLhs;
std::set<std::string> nonterminals;
std::set<std::string> terminals;
std::map<std::string, std::set<std::string> > first;
std::map<std::string, std::set<std::string> > follow;
```

含义：

- `productions`：全部产生式。
- `byLhs`：按左部查找产生式编号，例如 `Stmt -> ...` 有多条。
- `nonterminals`：非终结符集合。
- `terminals`：终结符集合。
- `first`：FIRST 集。
- `follow`：FOLLOW 集。

构造函数：

```cpp
Grammar() {
    buildProductions();
    buildSymbols();
    buildFirst();
    buildFollow();
}
```

### 3.1 buildProductions

位置：[parser.cpp](../src/parser/parser.cpp#L106)

`buildProductions` 用 `add(...)` 把 C-- 文法写进程序。

例子：

```cpp
add("Program", {"CompUnit"}, "program");
add("Stmt", {"LVal", "=", "Exp", ";"}, "assign_stmt");
add("Stmt", {"return", "ReturnExpOpt", ";"}, "return_stmt");
add("AddExp", {"AddExp", "AddOp", "MulExp"}, "binary_expr");
```

其中 `add` 的逻辑是：

```cpp
void add(const std::string& lhs, const std::vector<std::string>& rhs, const std::string& tag) {
    Production production;
    production.lhs = lhs;
    production.rhs = rhs;
    production.tag = tag;
    byLhs[lhs].push_back((int)productions.size());
    productions.push_back(production);
    nonterminals.insert(lhs);
}
```

设计要点：

- 文法不是运行时从 `grammar/c--_bnf.txt` 读取，而是在代码中手写。
- `grammar/c--_bnf.txt` 是说明文档，用来和代码文法保持一致。
- 每条产生式都有 `tag`，方便规约时构造不同 AST。

### 3.2 顶层文法左因子化

在 C-- 中，顶层变量声明和函数定义都可能以 `int Ident` 开头：

```c
int a;
int add(int x, int y) { ... }
```

如果直接写：

```text
TopItem -> VarDecl
TopItem -> FuncDef
VarDecl -> Type VarDef VarDefList ;
FuncDef -> FuncType Name ( FuncFParamsOpt ) Block
```

SLR 分析时会在看到 `int` 后无法稳定判断后面应该走变量声明还是函数定义，容易产生冲突。

所以当前实现做了等价左因子化：

```cpp
add("TopItem", {"Type", "Name", "TopAfterTypeName"}, "top_type_item");
add("TopAfterTypeName", {"(", "FuncFParamsOpt", ")", "Block"}, "top_func_tail");
add("TopAfterTypeName", {"TopVarDefRest", "VarDefList", ";"}, "top_var_tail");
```

含义：

```text
先统一读 Type Name
如果后面是 '('，说明是函数定义
如果后面是 '='、','、';'，说明是变量声明
```

这是为了让 SLR 分析表更稳定，也是当前 Parser 和 `grammar/c--_bnf.txt` 中顶层文法略不同于原始附录写法的原因。

### 3.3 buildSymbols

位置：[parser.cpp](../src/parser/parser.cpp#L211)

`buildSymbols` 根据产生式自动区分终结符和非终结符。

逻辑：

```text
所有产生式左部都是非终结符
遍历所有右部符号
如果某个右部符号不在非终结符集合中，就认为它是终结符
额外加入 EOF
```

例如：

```text
Stmt、Exp、AddExp 是非终结符
int、return、+、;、Ident、IntConst 是终结符
```

### 3.4 buildFirst

位置：[parser.cpp](../src/parser/parser.cpp#L278)

FIRST 集表示一个符号或符号串可能以哪些终结符开头。

当前实现采用迭代算法：

```text
1. 所有终结符 t：FIRST(t) = { t }
2. 所有非终结符先初始化为空
3. 反复扫描所有产生式
4. 对 A -> X1 X2 ... Xn
5. 把 FIRST(X1) 中非 epsilon 的符号加入 FIRST(A)
6. 如果 X1 可空，再看 X2
7. 如果所有 Xi 都可空，把 epsilon 加入 FIRST(A)
8. 直到没有集合发生变化
```

代码核心：

```cpp
bool changed = true;
while (changed) {
    changed = false;
    for (size_t i = 0; i < productions.size(); i++) {
        const Production& production = productions[i];
        ...
    }
}
```

这里使用 `changed` 控制循环，直到 FIRST 集稳定。

### 3.5 buildFollow

位置：[parser.cpp](../src/parser/parser.cpp#L318)

FOLLOW 集表示某个非终结符后面可能跟哪些终结符。

当前实现也是迭代算法：

```text
1. FOLLOW(Program) 加入 EOF
2. 对每条产生式 A -> α B β
3. 把 FIRST(β) 中非 epsilon 的符号加入 FOLLOW(B)
4. 如果 β 可空，把 FOLLOW(A) 加入 FOLLOW(B)
5. 反复扫描直到不再变化
```

代码里使用 `firstOfSequence(production.rhs, j + 1)` 计算 β 的 FIRST 集。

FOLLOW 集在 SLR 中非常关键：当一个项目已经归约完成时，不是对所有终结符都规约，而是只在 FOLLOW(左部) 中的终结符上规约。

## 4. SLRTable 类

定义位置：[parser.cpp](../src/parser/parser.cpp#L354)

`SLRTable` 负责构造 LR(0) 项目集规范族，并生成 ACTION / GOTO 表。

主要成员：

```cpp
const Grammar& grammar;
std::vector<std::set<Item> > states;
std::map<std::pair<int, std::string>, int> transitions;
std::map<std::pair<int, std::string>, Action> actions;
std::map<std::pair<int, std::string>, int> gotos;
```

含义：

- `states`：LR(0) 项目集规范族，每个状态是一个项目集合。
- `transitions`：项目集之间的转换边。
- `actions`：SLR ACTION 表。
- `gotos`：SLR GOTO 表。

构造函数：

```cpp
explicit SLRTable(const Grammar& g) : grammar(g) {
    buildStates();
    buildTables();
}
```

### 4.1 closure

位置：[parser.cpp](../src/parser/parser.cpp#L368)

`closure` 计算项目集闭包。

算法：

```text
输入一个项目集 I
如果项目中有 A -> α . B β，且 B 是非终结符
则把 B 的所有产生式 B -> γ 加入项目 B -> . γ
重复这个过程，直到没有新项目加入
```

代码核心：

```cpp
if (current[i].dot >= (int)production.rhs.size()) {
    continue;
}
const std::string& next = production.rhs[current[i].dot];
if (grammar.nonterminals.find(next) == grammar.nonterminals.end()) {
    continue;
}
```

如果点后面是非终结符，就把该非终结符的所有产生式加进闭包。

### 4.2 goTo

位置：[parser.cpp](../src/parser/parser.cpp#L400)

`goTo` 表示从某个项目集读入一个符号后到达的新项目集。

算法：

```text
对状态 I 中每个项目 A -> α . X β
如果点后符号是 X
则移动点，得到 A -> α X . β
把所有移动后的项目组成集合
再对这个集合求 closure
```

代码：

```cpp
if (it->dot < (int)production.rhs.size() && production.rhs[it->dot] == symbol) {
    Item next;
    next.production = it->production;
    next.dot = it->dot + 1;
    moved.insert(next);
}
return closure(moved);
```

### 4.3 buildStates

位置：[parser.cpp](../src/parser/parser.cpp#L423)

`buildStates` 构造 LR(0) 项目集规范族。

流程：

```text
1. 从增广文法 Program' -> Program 开始
2. 初始状态 I0 = closure({ Program' -> . Program })
3. 对每个状态，找出所有点后符号
4. 对每个点后符号 X，计算 goto(I, X)
5. 如果新项目集不存在，就加入 states
6. 记录 transition
7. 用队列继续处理新状态
```

代码使用 `std::queue<int>` 做广度遍历：

```cpp
states.push_back(closure(startItems));
std::queue<int> queue;
queue.push(0);

while (!queue.empty()) {
    int current = queue.front();
    queue.pop();
    ...
}
```

### 4.4 buildTables

位置：[parser.cpp](../src/parser/parser.cpp#L488)

`buildTables` 根据 LR(0) 状态构造 SLR 分析表。

规则：

```text
如果项目 A -> α . a β，a 是终结符
  ACTION[state, a] = shift goto(state, a)

如果项目 A -> α . B β，B 是非终结符
  GOTO[state, B] = goto(state, B)

如果项目 A -> α . 已完成
  对 FOLLOW(A) 中每个终结符 a
  ACTION[state, a] = reduce A -> α

如果项目 Program' -> Program .
  ACTION[state, EOF] = accept
```

代码中区分终结符和非终结符：

```cpp
if (grammar.terminals.find(symbol) != grammar.terminals.end()) {
    Action action;
    action.type = ActionType::Shift;
    action.value = trans->second;
    setAction((int)i, symbol, action);
} else {
    gotos[std::make_pair((int)i, symbol)] = trans->second;
}
```

完成项目用 FOLLOW 集决定规约位置：

```cpp
const std::set<std::string>& followSet = grammar.follow.find(production.lhs)->second;
for (...) {
    Action action;
    action.type = ActionType::Reduce;
    action.value = item->production;
    setAction((int)i, *terminal, action);
}
```

### 4.5 setAction 和冲突处理

位置：[parser.cpp](../src/parser/parser.cpp#L467)

`setAction` 负责往 ACTION 表里写动作，并处理冲突。

目前特殊处理了经典的 dangling else 问题：

```cpp
if (old->second.type == ActionType::Shift && action.type == ActionType::Reduce && terminal == "else") {
    return;
}
if (old->second.type == ActionType::Reduce && action.type == ActionType::Shift && terminal == "else") {
    old->second = action;
    return;
}
```

含义：

```text
if (cond) if (...) stmt else stmt
```

这里的 `else` 默认和最近的 `if` 匹配，所以遇到 `else` 的 shift/reduce 冲突时优先 shift。

其他冲突直接报错：

```cpp
throw std::runtime_error(error.str());
```

这样做的好处是：如果后续改文法导致 SLR 表出现新冲突，不会静默生成错误分析表。

## 5. Parser::parse 分析流程

入口位置：[parser.cpp](../src/parser/parser.cpp#L856)

`Parser::parse` 是真正执行语法分析的函数。

### 5.1 初始化

```cpp
Grammar grammar;
SLRTable table(grammar);

std::vector<Token> input = tokens;
if (input.empty() || input.back().grammar != EOF_SYMBOL) {
    input.push_back(makeEOFLikeToken(input));
}

std::vector<int> stateStack;
std::vector<SemanticValue> valueStack;
stateStack.push_back(0);
```

这里有两个栈：

- `stateStack`：状态栈，保存 SLR 自动机状态。
- `valueStack`：语义值栈，保存 token 文本、AST 节点、节点列表等。

为什么需要两个栈？

```text
状态栈决定下一步 shift / reduce / accept
语义值栈负责在规约时构造 AST
```

### 5.2 查 ACTION 表

每轮循环先看当前状态和当前输入符号：

```cpp
int state = stateStack.back();
std::string lookahead = input[index].grammar.empty() ? input[index].lexeme : input[index].grammar;
actionIt = table.actions.find(std::make_pair(state, lookahead));
```

注意这里使用的是 `token.grammar`，不是直接使用 `token.lexeme`。

例如：

```text
变量名 a 的 lexeme 是 a，但 grammar 是 Ident
整数 10 的 lexeme 是 10，但 grammar 是 IntConst
关键字 int 的 grammar 是 int
```

这就是词法分析中 `grammar` 字段存在的原因。

### 5.3 错误处理

如果 ACTION 表查不到动作，说明当前 token 在当前状态下不合法：

```cpp
if (actionIt == table.actions.end()) {
    error << "Syntax error at line " << input[index].line
          << ", column " << input[index].column
          << ": unexpected '" << input[index].lexeme << "'"
          << ", expected: " << expectedTokens(table, state);
    result.success = false;
    result.errorMessage = error.str();
    return result;
}
```

错误信息包含：

- 出错行号。
- 出错列号。
- 遇到的 token。
- 当前状态下期望的 token 集合。

例如：

```text
Syntax error at line 3, column 1: unexpected '}', expected: ...
```

IDE 中左侧红色下划线就是根据这个行列号标出来的。

### 5.4 Shift 移进

移进动作：

```cpp
if (action.type == ActionType::Shift) {
    SemanticValue value;
    value.symbol = lookahead;
    value.text = input[index].lexeme;
    value.line = input[index].line;
    value.column = input[index].column;
    valueStack.push_back(value);
    stateStack.push_back(action.value);
    index++;
}
```

移进时做三件事：

```text
1. 把当前 token 转成 SemanticValue
2. 把 SemanticValue 压入语义值栈
3. 把新状态压入状态栈
4. 输入指针 index 后移
```

同时会记录日志：

```cpp
log << logNo++ << "\t" << stackTopSymbol(valueStack) << "#" << lookahead << "\tmove";
```

日志格式类似：

```text
1    $#int    move
```

### 5.5 Reduce 规约

规约动作：

```cpp
const Production& production = grammar.productions[action.value];
std::vector<SemanticValue> rhs(production.rhs.size());
for (int i = (int)production.rhs.size() - 1; i >= 0; i--) {
    rhs[i] = valueStack.back();
    valueStack.pop_back();
    stateStack.pop_back();
}
```

规约时做这些事：

```text
1. 找到要使用的产生式
2. 按产生式右部长度，从两个栈中弹出对应数量的元素
3. 把弹出的语义值按原顺序放到 rhs
4. 调用 buildSemantic 构造规约后的语义值
5. 查 GOTO 表
6. 压入规约后的语义值和新状态
```

调用语义动作：

```cpp
SemanticValue reduced = buildSemantic(production, rhs);
reduced.symbol = production.lhs;
```

查 GOTO 表：

```cpp
int gotoFrom = stateStack.back();
gotoIt = table.gotos.find(std::make_pair(gotoFrom, production.lhs));
```

记录日志：

```cpp
log << logNo++ << "\t" << production.lhs << "#" << lookahead
    << "\treduction " << productionToString(production);
```

日志例子：

```text
23    Stmt#}    reduction Stmt -> return ReturnExpOpt ;
```

### 5.6 Accept 接受

接受动作：

```cpp
if (action.type == ActionType::Accept) {
    result.success = true;
    if (!valueStack.empty()) {
        result.root.reset(valueStack.back().ast);
        valueStack.back().ast = NULL;
    }
    return result;
}
```

接受时，语义值栈顶应该保存完整程序的 AST，也就是 `CompUnit` 节点。

## 6. AST 构造设计

AST 节点定义在 [AST.h](../include/c--/parser/AST.h)。

核心结构：

```cpp
struct ASTNode {
    std::string name;
    std::string value;
    int line = -1;
    int column = -1;
    std::vector<ASTNode*> children;
};
```

字段含义：

- `name`：节点类型，例如 `FuncDef`、`VarDecl`、`BinaryExpr`。
- `value`：节点值，例如函数名 `main`、变量名 `a`、操作符 `+`。
- `line` / `column`：源码位置。
- `children`：子节点列表。

### 6.1 buildSemantic

位置：[parser.cpp](../src/parser/parser.cpp#L544)

`buildSemantic` 是 AST 构造的核心函数。

输入：

```cpp
SemanticValue buildSemantic(const Production& production, std::vector<SemanticValue>& rhs)
```

参数含义：

- `production`：当前用于规约的产生式。
- `rhs`：刚从语义值栈弹出来的右部语义值。

输出：

- 一个新的 `SemanticValue`，代表规约后的左部符号。
- 这个值可能带有 `ast`，也可能带有 `list`，也可能只带 `text`。

它通过 `production.tag` 分派不同语义动作：

```cpp
const std::string& tag = production.tag;

if (tag == "return_stmt") { ... }
if (tag == "binary_expr") { ... }
if (tag == "func_def") { ... }
```

### 6.2 pass 类型语义动作

很多非终结符只是包装一层，不需要产生新 AST 节点。

例如：

```text
Exp -> LOrExp
PrimaryExp -> Number
InitVal -> Exp
```

这类产生式使用 `pass`：

```cpp
if (tag == "pass" || tag == "program" || tag == "paren") {
    result.ast = rhs[index].ast;
    result.list = rhs[index].list;
    result.text = rhs[index].text;
    ...
}
```

含义是：直接把子节点传上去，不额外生成一层 `Exp`、`PrimaryExp` 之类的节点。

这样 AST 会更简洁。

### 6.3 函数定义节点

文法：

```text
FuncDef -> FuncType Name ( FuncFParamsOpt ) Block
```

语义动作：

```cpp
if (tag == "func_def") {
    result.ast = makeNode(ASTName::FuncDef, rhs[1].text, rhs[1].line, rhs[1].column);
    result.ast->addChild(rhs[0].ast);
    result.ast->addChild(rhs[3].ast);
    result.ast->addChild(rhs[5].ast);
    return result;
}
```

生成 AST：

```text
FuncDef value=函数名
  Type value=返回类型
  ParamList
  Block
```

例子：

```c
int main() {
  return 0;
}
```

生成：

```text
CompUnit
  FuncDef value=main
    Type value=int
    ParamList
    Block
      ReturnStmt
        IntLiteral value=0
```

### 6.4 变量声明节点

文法：

```text
VarDecl -> Type VarDef VarDefList ;
VarDef -> Name
VarDef -> Name = InitVal
```

语义动作：

```cpp
if (tag == "var_decl") {
    result.ast = makeNode(ASTName::VarDecl, "", rhs[0].line, rhs[0].column);
    result.ast->addChild(rhs[0].ast);
    result.ast->addChild(rhs[1].ast);
    appendList(result.ast->children, rhs[2].list);
    return result;
}
```

生成 AST：

```text
VarDecl
  Type value=int
  VarDef value=a
    IntLiteral value=3
```

如果是多个变量：

```c
int a = 1, b = 2;
```

生成：

```text
VarDecl
  Type value=int
  VarDef value=a
    IntLiteral value=1
  VarDef value=b
    IntLiteral value=2
```

### 6.5 赋值语句和返回语句

赋值：

```cpp
if (tag == "assign_stmt") {
    result.ast = makeNode(ASTName::AssignStmt, "", rhs[0].line, rhs[0].column);
    result.ast->addChild(rhs[0].ast);
    result.ast->addChild(rhs[2].ast);
    return result;
}
```

生成：

```text
AssignStmt
  LVal value=a
  BinaryExpr value=+
    LVal value=b
    IntLiteral value=1
```

返回：

```cpp
if (tag == "return_stmt") {
    result.ast = makeNode(ASTName::ReturnStmt, "", rhs[0].line, rhs[0].column);
    if (rhs[1].ast) {
        result.ast->addChild(rhs[1].ast);
    }
    return result;
}
```

生成：

```text
ReturnStmt
  IntLiteral value=0
```

如果是 `return;`，则 `ReturnStmt` 没有子节点。

### 6.6 表达式节点

表达式按优先级分层：

```text
UnaryExp
MulExp       * / %
AddExp       + -
RelExp       < > <= >=
EqExp        == !=
LAndExp      &&
LOrExp       ||
```

这种写法保证优先级正确：

```c
a + b * c
```

会生成：

```text
BinaryExpr value=+
  LVal value=a
  BinaryExpr value=*
    LVal value=b
    LVal value=c
```

二元表达式构造函数：

```cpp
ASTNode* makeBinary(
    const std::string& op,
    ASTNode* left,
    ASTNode* right
)
```

语义动作：

```cpp
if (tag == "binary_expr") {
    result.ast = makeBinary(rhs[1].text, rhs[0].ast, rhs[2].ast);
    return result;
}
```

一元表达式：

```cpp
if (tag == "unary_expr") {
    result.ast = makeNode(ASTName::UnaryExpr, rhs[0].text, rhs[0].line, rhs[0].column);
    result.ast->addChild(rhs[1].ast);
    return result;
}
```

例如：

```c
!0
```

生成：

```text
UnaryExpr value=!
  IntLiteral value=0
```

### 6.7 if/else 节点

文法：

```text
Stmt -> if ( Cond ) Stmt ElseOpt
ElseOpt -> else Stmt
ElseOpt -> $
```

语义动作：

```cpp
if (tag == "if_stmt") {
    result.ast = makeNode(ASTName::IfStmt, "", rhs[0].line, rhs[0].column);
    result.ast->addChild(rhs[2].ast);
    result.ast->addChild(rhs[4].ast);
    if (rhs[5].ast) {
        result.ast->addChild(rhs[5].ast);
    }
    return result;
}
```

生成：

```text
IfStmt
  条件表达式
  then 语句
  else 语句
```

如果没有 `else`，则只有两个子节点。

### 6.8 函数调用节点

文法：

```text
UnaryExp -> Name ( FuncRParamsOpt )
FuncRParams -> Exp FuncRParamsTail
```

语义动作：

```cpp
if (tag == "call_expr") {
    result.ast = makeNode(ASTName::CallExpr, rhs[0].text, rhs[0].line, rhs[0].column);
    appendList(result.ast->children, rhs[2].list);
    return result;
}
```

例子：

```c
add(1, 2)
```

生成：

```text
CallExpr value=add
  IntLiteral value=1
  IntLiteral value=2
```

## 7. 为什么 AST 不保留所有文法节点

当前 AST 是抽象语法树，不是完整语法树。

例如表达式文法中有：

```text
Exp -> LOrExp
LOrExp -> LAndExp
LAndExp -> EqExp
...
```

如果全部保留，AST 会很深，而且很多节点没有实际语义。

所以当前实现会省略这些中间层，只保留中间代码生成真正需要的信息：

- 函数定义。
- 声明。
- 语句。
- 左值。
- 字面量。
- 一元/二元表达式。
- 函数调用。
- 代码块。

这样中间代码生成组员遍历 AST 时会更轻松。

## 8. 输出规约日志

规约日志保存在 `ParseResult::reduceLogs` 中。

Shift 日志格式：

```text
序号    栈顶符号#面临输入符号    move
```

Reduce 日志格式：

```text
序号    产生式左部#面临输入符号    reduction 产生式
```

例子：

```text
1    $#int    move
2    Type#main    reduction Type -> int
23   Stmt#}    reduction Stmt -> return ReturnExpOpt ;
```

日志的作用：

- 满足作业要求中的“输出规约过程”。
- 调试语法错误。
- 检查表达式优先级和语句结构是否按预期规约。

测试 driver 会把日志写入 `reduce.txt`。

## 9. 当前支持的语法范围

当前 Parser 主要支持大作业文档要求的 C-- 子集：

- 全局变量声明和局部变量声明。
- `const int/float` 常量声明。
- `int`、`float` 类型。
- `void`、`int` 返回类型函数。
- 函数参数。
- 代码块。
- 赋值语句。
- 表达式语句。
- `if/else`。
- `return`。
- 整数和浮点数字面量。
- 一元运算 `+ - !`。
- 二元运算 `+ - * / % < > <= >= == != && ||`。
- 函数调用。

当前没有重点支持：

- 数组。
- 指针。
- 结构体。
- `while` / `for` 循环。
- `break` / `continue`。
- 字符串。
- 标准输入输出语句。

这些不在当前大作业文档的必做范围内，后续可以作为拓展。

## 10. 和中间代码生成的关系

语法分析器输出 AST，后续中间代码生成器遍历 AST。

关系如下：

```text
Parser
  -> ParseResult.root
  -> ASTNode
  -> IRGenerator 遍历 AST
  -> 调用 compiler_ir 中端类
  -> 输出 LLVM IR
```

例如：

```text
BinaryExpr value=+
  LVal value=a
  IntLiteral value=1
```

IR 生成器看到 `BinaryExpr value=+` 后，可以：

```text
1. 递归生成左操作数 a 的 Value
2. 递归生成右操作数 1 的 Value
3. 调用中端接口生成 add 指令
4. 返回 add 指令产生的 Value
```

因此 AST 的节点名、`value` 字段和 `children` 顺序必须稳定。具体约定见 [AST_SPEC.md](AST_SPEC.md)。

## 11. 设计总结

当前语法分析器的设计可以概括为：

```text
Grammar 负责文法、FIRST、FOLLOW
SLRTable 负责 LR(0) 项目集和 ACTION/GOTO 表
Parser::parse 负责分析栈执行
buildSemantic 负责规约时构造 AST
ParseResult 负责把 AST、规约日志和错误信息返回给外部
```

这种设计的好处：

- 算法结构清晰，和课程讲的 SLR 流程对应。
- 分析表是代码自动构造的，不需要手写大表。
- AST 在规约过程中同步生成，不需要再额外遍历语法树。
- 错误信息带行列号，方便测试和 IDE 高亮。
- `tag` 把“文法产生式”和“AST 构造动作”连接起来，方便后续维护。

需要注意的地方：

- 如果修改文法，必须同步修改 `buildSemantic` 中对应的 `tag` 处理。
- 如果新增 AST 节点，也要同步更新 `AST.h`、`AST_SPEC.md` 和 IR 生成器。
- 如果新增关键字或终结符，需要确认词法分析器的 `grammar` 字段和 Parser 文法符号一致。
