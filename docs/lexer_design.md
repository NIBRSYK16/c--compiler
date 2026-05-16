# 词法分析器实现说明

本文档说明当前项目词法分析器的设计和实现细节。代码主要分成两部分：

- 自动机模块：根据词法规则构造 NFA，再转成 DFA，最后做 DFA 最小化。
- 扫描器模块：使用最小化 DFA 对源程序做最长匹配，输出 token 序列和错误信息。

相关源码：

- [Common.h](../include/c--/common/Common.h)：token 数据结构和 `LexResult` 返回结构。
- [automata.h](../include/c--/lexer/automata.h)：NFA/DFA 数据结构和接口。
- [automata.cpp](../src/lexer/automata.cpp)：自动机构造、确定化、最小化。
- [lexer.h](../include/c--/lexer/lexer.h)：`Lexer` 类接口。
- [lexer.cpp](../src/lexer/lexer.cpp)：扫描器主逻辑。

## 1. 总体流程

词法分析整体流程如下：

```text
C-- 源代码字符串
  -> 跳过空白字符，并维护行号列号
  -> 使用最小化 DFA 做最长匹配
  -> 生成 Token
  -> 遇到非法字符时记录错误
  -> 返回 LexResult
```

对应代码入口是 [Lexer::tokenize](../src/lexer/lexer.cpp#L122)：

```cpp
LexResult Lexer::tokenize(const std::string& source);
```

输入是完整源程序字符串，输出是：

```cpp
struct LexResult {
    bool success;
    std::vector<Token> tokens;
    std::string errorMessage;
};
```

## 2. Token 数据结构

定义位置：[Common.h](../include/c--/common/Common.h#L10)

```cpp
enum class TokenType {
    Keyword,      // 关键字
    Identifier,   // 标识符
    Integer,      // 整数
    Float,        // 浮点数
    Operator,     // 运算符
    Separator,    // 界符
    EndOfFile,    // 文件结束
    Unknown       // 未知类型
};
```

每个 token 使用 `Token` 表示：

```cpp
struct Token {
    std::string lexeme;    // 源代码里的原始文本，例如 int、a、10、+
    TokenType type;        // token 大类，例如 Keyword、Identifier
    std::string grammar;   // Parser 使用的文法符号，例如 int、Ident、IntConst
    std::string attr;      // 输出属性，例如关键字编号、变量名、数值
    int line = 1;          // token 起始行号
    int column = 1;        // token 起始列号
};
```

几个字段的区别：

- `lexeme` 保存源码原文。
- `type` 给程序内部判断 token 大类使用。
- `grammar` 给语法分析器使用。
- `attr` 用来输出题目要求的二元属性。

例子：

```text
源码      type        grammar     attr
int       Keyword     int         1
a         Identifier  Ident       a
=         Operator    =           11
10        Integer     IntConst    10
;         Separator   ;           24
```

## 3. LexResult 返回结构

定义位置：[Common.h](../include/c--/common/Common.h#L40)

```cpp
struct LexResult {
    bool success = false;
    std::vector<Token> tokens;
    std::string errorMessage;
};
```

含义：

- `success`：是否没有词法错误。
- `tokens`：扫描得到的 token 序列，末尾会补一个 `EOF`。
- `errorMessage`：词法错误信息。若无错误，则为空字符串。

这样做的好处是：词法分析阶段不直接退出程序，而是把结果交给主流程继续判断。

## 4. 自动机数据结构

定义位置：[automata.h](../include/c--/lexer/automata.h#L12)

### 4.1 TokenRule

```cpp
struct TokenRule {
    std::string name;      // KW / OP / SE / IDN / INT / FLOAT
    std::string attr;      // KW/OP/SE 使用固定编号，IDN/INT/FLOAT 使用词素本身
    int priority;          // 优先级，数值越小优先级越高
};
```

`TokenRule` 描述一条词法规则。

例如：

```text
name=KW, attr=1, priority=1      表示关键字 int
name=OP, attr=11, priority=...   表示运算符 =
name=IDN, attr="", priority=200  表示标识符
```

优先级用于处理冲突。例如 `int` 既符合关键字规则，也符合标识符规则，此时关键字优先。

### 4.2 NFA 相关结构

```cpp
struct NFAEdge {
    int to;       // 目标状态
    int ch;       // 转移字符，EPSILON 表示 epsilon 边
};
```

```cpp
struct NFAState {
    int id;                       // 状态编号
    std::vector<NFAEdge> edges;   // 从该状态出发的边
    int acceptRule;               // 接受规则编号，-1 表示非接受态
};
```

```cpp
struct NFAFragment {
    int start;     // 片段起点
    int accept;    // 片段终点
};
```

`NFAFragment` 是 Thompson 构造中使用的小片段。例如构造一个字符集合、一个字符串、一个闭包时，都会返回一个片段。

完整 NFA：

```cpp
struct NFA {
    int start;
    std::vector<NFAState> states;
};
```

### 4.3 DFA 相关结构

```cpp
struct DFAState {
    int id;                         // DFA 状态编号
    std::set<int> nfaStates;        // 该 DFA 状态对应的 NFA 状态集合
    std::map<int, int> trans;       // 输入字符 -> 下一个 DFA 状态
    int acceptRule;                 // 接受规则编号，-1 表示非接受态
};
```

```cpp
struct DFA {
    int start;
    std::vector<DFAState> states;
    std::vector<int> alphabet;
};
```

DFA 状态本质上是 NFA 状态集合。`nfaToDfa` 使用子集构造法完成这个转换。

## 5. NFABuilder 类

实现位置：[automata.cpp](../src/lexer/automata.cpp#L11)

`NFABuilder` 是自动机模块内部使用的构造器。它只有一个成员：

```cpp
class NFABuilder {
public:
    NFA nfa;
    ...
};
```

构造函数：

```cpp
NFABuilder() {
    nfa.start = newState();
}
```

创建一个全局起始状态。后面每条词法规则都会通过 epsilon 边连接到这个全局起点。

### 5.1 newState

位置：[automata.cpp](../src/lexer/automata.cpp#L19)

```cpp
int newState() {
    NFAState state;
    state.id = (int)nfa.states.size();
    state.acceptRule = -1;
    nfa.states.push_back(state);
    return state.id;
}
```

作用：

- 创建一个新的 NFA 状态。
- 默认不是接受态，所以 `acceptRule = -1`。
- 返回新状态编号。

### 5.2 addEdge

位置：[automata.cpp](../src/lexer/automata.cpp#L27)

```cpp
void addEdge(int from, int to, int ch) {
    NFAEdge edge;
    edge.to = to;
    edge.ch = ch;
    nfa.states[from].edges.push_back(edge);
}
```

作用：添加一条 NFA 边。

如果 `ch == EPSILON`，表示 epsilon 转移，不消耗输入字符。

### 5.3 charSet

位置：[automata.cpp](../src/lexer/automata.cpp#L34)

```cpp
NFAFragment charSet(const std::vector<int>& chars) {
    int start = newState();
    int accept = newState();

    for (size_t i = 0; i < chars.size(); i++) {
        addEdge(start, accept, chars[i]);
    }

    NFAFragment result;
    result.start = start;
    result.accept = accept;
    return result;
}
```

作用：构造一个字符集合片段。

例如 `digit = 0..9`，会得到：

```text
start --0--> accept
start --1--> accept
...
start --9--> accept
```

### 5.4 concat

位置：[automata.cpp](../src/lexer/automata.cpp#L48)

```cpp
NFAFragment concat(NFAFragment left, NFAFragment right) {
    addEdge(left.accept, right.start, EPSILON);

    NFAFragment result;
    result.start = left.start;
    result.accept = right.accept;
    return result;
}
```

作用：连接两个 NFA 片段。

例如构造 `digit+ '.' digit+` 时，会把数字部分、小数点部分、数字部分依次连接。

### 5.5 alternate

位置：[automata.cpp](../src/lexer/automata.cpp#L57)

```cpp
NFAFragment alternate(NFAFragment left, NFAFragment right) {
    int start = newState();
    int accept = newState();

    addEdge(start, left.start, EPSILON);
    addEdge(start, right.start, EPSILON);
    addEdge(left.accept, accept, EPSILON);
    addEdge(right.accept, accept, EPSILON);

    NFAFragment result;
    result.start = start;
    result.accept = accept;
    return result;
}
```

作用：构造选择结构，也就是正则里的 `A | B`。

### 5.6 star

位置：[automata.cpp](../src/lexer/automata.cpp#L72)

```cpp
NFAFragment star(NFAFragment fragment) {
    int start = newState();
    int accept = newState();

    addEdge(start, fragment.start, EPSILON);
    addEdge(start, accept, EPSILON);
    addEdge(fragment.accept, fragment.start, EPSILON);
    addEdge(fragment.accept, accept, EPSILON);

    NFAFragment result;
    result.start = start;
    result.accept = accept;
    return result;
}
```

作用：构造 `fragment*`，表示出现 0 次或多次。

标识符后续部分：

```text
(letter | digit | _)*
```

就用到了 `star`。

### 5.7 plus

位置：[automata.cpp](../src/lexer/automata.cpp#L87)

```cpp
NFAFragment plus(NFAFragment fragment) {
    int start = newState();
    int accept = newState();

    addEdge(start, fragment.start, EPSILON);
    addEdge(fragment.accept, fragment.start, EPSILON);
    addEdge(fragment.accept, accept, EPSILON);

    NFAFragment result;
    result.start = start;
    result.accept = accept;
    return result;
}
```

作用：构造 `fragment+`，表示出现 1 次或多次。

整数规则：

```text
digit+
```

浮点数里的整数部分也用到 `plus`。

### 5.8 literal

位置：[automata.cpp](../src/lexer/automata.cpp#L101)

```cpp
NFAFragment literal(const std::string& word, bool ignoreCase) {
    NFAFragment result;
    result.start = -1;
    result.accept = -1;

    for (size_t i = 0; i < word.size(); i++) {
        std::vector<int> chars;
        unsigned char ch = (unsigned char)word[i];

        if (ignoreCase && ch >= 'a' && ch <= 'z') {
            chars.push_back(ch);
            chars.push_back(ch - 'a' + 'A');
        } else if (ignoreCase && ch >= 'A' && ch <= 'Z') {
            chars.push_back(ch);
            chars.push_back(ch - 'A' + 'a');
        } else {
            chars.push_back(ch);
        }

        NFAFragment oneChar = charSet(chars);
        if (result.start == -1) {
            result = oneChar;
        } else {
            result = concat(result, oneChar);
        }
    }

    return result;
}
```

作用：构造固定字符串的 NFA。

例如：

```text
"return"
"=="
"&&"
```

如果 `ignoreCase = true`，则关键字大小写都能识别。例如 `int`、`INT`、`Int` 都会被识别成关键字。

### 5.9 addRule

位置：[automata.cpp](../src/lexer/automata.cpp#L131)

```cpp
void addRule(NFAFragment fragment, int ruleIndex) {
    nfa.states[fragment.accept].acceptRule = ruleIndex;
    addEdge(nfa.start, fragment.start, EPSILON);
}
```

作用：

- 给某个 NFA 片段的接受态绑定 token 规则。
- 把该片段接到总 NFA 起点上。

最终所有词法规则共享一个 NFA 起点。

## 6. 词法规则构造

实现位置：[buildLexerNFA](../src/lexer/automata.cpp#L242)

`buildLexerNFA` 负责把 C-- 的所有词法规则登记到 NFA 中。

### 6.1 关键字

```cpp
WordCode keywords[] = {
    {"int", "1"},
    {"void", "2"},
    {"return", "3"},
    {"const", "4"},
    {"main", "5"},
    {"float", "6"},
    {"if", "7"},
    {"else", "8"},
    {NULL, NULL}
};
```

构造时使用：

```cpp
builder.literal(keywords[i].word, true)
```

第二个参数是 `true`，表示关键字不区分大小写。

### 6.2 运算符

```cpp
WordCode operators[] = {
    {"+", "6"}, {"-", "7"}, {"*", "8"}, {"/", "9"},
    {"%", "10"}, {"=", "11"}, {">", "12"}, {"<", "13"},
    {"==", "14"}, {"<=", "15"}, {">=", "16"}, {"!=", "17"},
    {"&&", "18"}, {"||", "19"}, {"!", "!"},
    {NULL, NULL}
};
```

说明：

- 编号基本按照大作业文档。
- 单独的 `!` 是额外支持的，因为语法中要求一元表达式 `!0`，但文档运算符编号表没有给单独 `!` 编号。

### 6.3 界符

```cpp
WordCode separators[] = {
    {"(", "20"},
    {")", "21"},
    {"{", "22"},
    {"}", "23"},
    {";", "24"},
    {",", "25"},
    {NULL, NULL}
};
```

### 6.4 字符类

```cpp
std::vector<int> digit = makeRange('0', '9');

std::vector<int> letter = makeRange('a', 'z');
appendVector(letter, makeRange('A', 'Z'));

std::vector<int> letterOrUnder = letter;
letterOrUnder.push_back('_');

std::vector<int> letterDigitUnder = letterOrUnder;
appendVector(letterDigitUnder, digit);
```

这些字符类用于构造整数、浮点数和标识符。

### 6.5 浮点数

```cpp
// FLOAT = digit+ '.' digit+
int floatRule = addRuleInfo(rules, "FLOAT", "", 100);
NFAFragment floatPrefix = builder.concat(
    builder.plus(builder.charSet(digit)),
    builder.literal(".", false)
);
NFAFragment floatNumber = builder.concat(
    floatPrefix,
    builder.plus(builder.charSet(digit))
);
builder.addRule(floatNumber, floatRule);
```

对应文法：

```text
[0-9]+'.'[0-9]+
```

所以 `1.5` 合法，`1.` 和 `.5` 不按浮点数识别。

### 6.6 整数

```cpp
int intRule = addRuleInfo(rules, "INT", "", 101);
builder.addRule(builder.plus(builder.charSet(digit)), intRule);
```

对应：

```text
[0-9]+
```

### 6.7 标识符

```cpp
int idRule = addRuleInfo(rules, "IDN", "", 200);
NFAFragment idFirst = builder.charSet(letterOrUnder);
NFAFragment idRest = builder.star(builder.charSet(letterDigitUnder));
builder.addRule(builder.concat(idFirst, idRest), idRule);
```

对应：

```text
[a-zA-Z_][a-zA-Z_0-9]*
```

## 7. NFA 到 DFA

实现位置：[nfaToDfa](../src/lexer/automata.cpp#L334)

### 7.1 epsilonClosure

位置：[automata.cpp](../src/lexer/automata.cpp#L171)

```cpp
std::set<int> epsilonClosure(const NFA& nfa, const std::set<int>& input) {
    std::set<int> result = input;
    std::queue<int> queue;

    for (std::set<int>::const_iterator it = input.begin(); it != input.end(); ++it) {
        queue.push(*it);
    }

    while (!queue.empty()) {
        int state = queue.front();
        queue.pop();

        const std::vector<NFAEdge>& edges = nfa.states[state].edges;
        for (size_t i = 0; i < edges.size(); i++) {
            if (edges[i].ch == EPSILON && result.find(edges[i].to) == result.end()) {
                result.insert(edges[i].to);
                queue.push(edges[i].to);
            }
        }
    }

    return result;
}
```

作用：从一组 NFA 状态出发，沿 epsilon 边能到达的所有状态都加入集合。

### 7.2 moveSet

位置：[automata.cpp](../src/lexer/automata.cpp#L195)

```cpp
std::set<int> moveSet(const NFA& nfa, const std::set<int>& input, int ch) {
    std::set<int> result;

    for (std::set<int>::const_iterator it = input.begin(); it != input.end(); ++it) {
        const std::vector<NFAEdge>& edges = nfa.states[*it].edges;
        for (size_t i = 0; i < edges.size(); i++) {
            if (edges[i].ch == ch) {
                result.insert(edges[i].to);
            }
        }
    }

    return result;
}
```

作用：从一组 NFA 状态出发，读入字符 `ch` 后能一步到达哪些状态。

### 7.3 chooseAcceptRule

位置：[automata.cpp](../src/lexer/automata.cpp#L210)

```cpp
int chooseAcceptRule(const NFA& nfa, const std::set<int>& states, const std::vector<TokenRule>& rules) {
    int best = -1;

    for (std::set<int>::const_iterator it = states.begin(); it != states.end(); ++it) {
        int ruleIndex = nfa.states[*it].acceptRule;
        if (ruleIndex >= 0) {
            if (best < 0 || rules[ruleIndex].priority < rules[best].priority) {
                best = ruleIndex;
            }
        }
    }

    return best;
}
```

作用：一个 DFA 状态可能包含多个 NFA 接受态。此时选择优先级最高的规则。

例如 `int` 同时符合关键字和标识符规则，最终选择关键字。

### 7.4 子集构造核心

```cpp
std::set<int> startSet;
startSet.insert(nfa.start);
startSet = epsilonClosure(nfa, startSet);
```

从 NFA 起点的 epsilon 闭包开始构造 DFA 初态。

核心公式：

```cpp
std::set<int> moved = moveSet(nfa, currentSet, ch);
std::set<int> nextSet = epsilonClosure(nfa, moved);
```

对应理论公式：

```text
DFA_next = epsilon-closure(move(NFA_state_set, ch))
```

如果 `nextSet` 之前没有出现过，就创建新的 DFA 状态；否则直接复用已有状态。

## 8. DFA 最小化

实现位置：[minimizeDfa](../src/lexer/automata.cpp#L400)

代码采用分组细化思想：

1. 先按 `acceptRule` 分组。
2. 对每个状态计算签名。
3. 签名包括：当前状态接受规则 + 每个输入字符转移到的目标组。
4. 如果签名不同，则拆分到不同组。
5. 重复直到分组不再变化。
6. 每组选择一个代表状态，生成最小 DFA。

关键代码：

```cpp
std::vector<int> signature;
signature.push_back(dfa.states[i].acceptRule);

for (size_t j = 0; j < dfa.alphabet.size(); j++) {
    int ch = dfa.alphabet[j];
    std::map<int, int>::const_iterator trans = dfa.states[i].trans.find(ch);
    if (trans == dfa.states[i].trans.end()) {
        signature.push_back(-1);
    } else {
        signature.push_back(group[trans->second]);
    }
}
```

解释：

- 如果两个状态的接受规则不同，不能合并。
- 如果两个状态在某个字符上转移到不同分组，也不能合并。

最终得到的 `minimized` 就是扫描器使用的最小 DFA。

## 9. 自动机缓存

实现位置：[automata.cpp](../src/lexer/automata.cpp#L225)

```cpp
bool built = false;
std::vector<TokenRule> globalRules;
DFA globalMinDFA;
NFA globalNFA;
DFA globalDFA;
```

`ensureBuilt` 负责懒加载：

```cpp
void ensureBuilt() {
    if (!built) {
        globalNFA = buildLexerNFA(globalRules);
        globalDFA = nfaToDfa(globalNFA, globalRules);
        globalMinDFA = minimizeDfa(globalDFA, globalRules);
        built = true;
    }
}
```

扫描器通过下面两个函数获取自动机和规则：

```cpp
const DFA& getMinimizedDFA() {
    ensureBuilt();
    return globalMinDFA;
}

const std::vector<TokenRule>& getTokenRules() {
    ensureBuilt();
    return globalRules;
}
```

好处：第一次扫描时构造自动机，之后重复使用，不需要每次重新构造。

## 10. 扫描器辅助函数

实现位置：[lexer.cpp](../src/lexer/lexer.cpp#L14)

### 10.1 isBlank

```cpp
bool isBlank(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n';
}
```

判断空白字符。空白字符不生成 token。

### 10.2 updatePosition

```cpp
void updatePosition(const std::string& source, int begin, int end, int& line, int& column) {
    for (int i = begin; i < end; i++) {
        if (source[i] == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
    }
}
```

维护行号和列号。

例如读到换行符，行号加 1，列号回到 1。读到普通字符，列号加 1。

### 10.3 grammarName

```cpp
std::string grammarName(const std::string& lexeme, const TokenRule& rule) {
    if (rule.name == "IDN") {
        return "Ident";
    }
    if (rule.name == "INT") {
        return "IntConst";
    }
    if (rule.name == "FLOAT") {
        return "floatConst";
    }
    if (rule.name == "KW") {
        return toLower(lexeme);
    }
    return lexeme;
}
```

作用：把词法规则转换成语法分析器需要的终结符。

例子：

```text
IDN   -> Ident
INT   -> IntConst
FLOAT -> floatConst
KW    -> 关键字文本的小写形式
OP/SE -> 原始符号
```

### 10.4 tokenAttr

```cpp
std::string tokenAttr(const std::string& lexeme, const TokenRule& rule) {
    if (rule.name == "IDN" || rule.name == "INT" || rule.name == "FLOAT") {
        return lexeme;
    }
    return rule.attr;
}
```

作用：生成 token 属性。

- 标识符、整数、浮点数：属性就是词素本身。
- 关键字、运算符、界符：属性是文档要求的编号。

### 10.5 makeToken

```cpp
Token makeToken(const std::string& lexeme, const TokenRule& rule, int line, int column) {
    Token token;
    token.lexeme = lexeme;
    token.type = tokenTypeFromRule(rule.name);
    token.grammar = grammarName(lexeme, rule);
    token.attr = tokenAttr(lexeme, rule);
    token.line = line;
    token.column = column;
    return token;
}
```

作用：把一次 DFA 匹配结果封装为 `Token`。

### 10.6 makeEOFToken

```cpp
Token makeEOFToken(int line, int column) {
    Token token;
    token.lexeme = "EOF";
    token.type = TokenType::EndOfFile;
    token.grammar = "EOF";
    token.attr = "EOF";
    token.line = line;
    token.column = column;
    return token;
}
```

作用：源码扫描结束后，补一个 EOF token，方便 Parser 判断输入结束。

## 11. Lexer::tokenize 详解

实现位置：[lexer.cpp](../src/lexer/lexer.cpp#L122)

完整代码如下，并加上说明性注释：

```cpp
LexResult Lexer::tokenize(const std::string& source) {
    // 最终返回结果，里面保存 token 序列、错误信息和成功状态。
    LexResult result;

    // 取出已经构造好的最小化 DFA 和 token 规则表。
    // 第一次调用时会触发 NFA -> DFA -> 最小 DFA 的构造。
    const DFA& dfa = getMinimizedDFA();
    const std::vector<TokenRule>& rules = getTokenRules();

    // 当前扫描位置的行号、列号和字符串下标。
    int line = 1;
    int column = 1;
    int pos = 0;
    int length = (int)source.size();

    // 词法错误统一收集在 errors 中。
    std::ostringstream errors;
    bool hasError = false;

    // 外层循环：每次识别一个 token。
    while (pos < length) {
        char current = source[pos];

        // 空白字符不生成 token，只更新行列号。
        if (isBlank(current)) {
            updatePosition(source, pos, pos + 1, line, column);
            pos++;
            continue;
        }

        // 从 DFA 初态开始尝试识别一个 token。
        int state = dfa.start;

        // cursor 是向前试探的位置，pos 是当前 token 的起点。
        int cursor = pos;

        // lastAcceptRule 和 lastAcceptPos 用于实现最长匹配。
        // 如果扫描过程中多次到达接受态，保留最后一次接受态。
        int lastAcceptRule = -1;
        int lastAcceptPos = -1;

        // 内层循环：沿 DFA 转移尽量往后读。
        while (cursor < length) {
            unsigned char ch = (unsigned char)source[cursor];

            // 本项目 C-- token 都是 ASCII 字符。
            if (ch >= 128) {
                break;
            }

            // 查当前 DFA 状态在字符 ch 上有没有转移。
            std::map<int, int>::const_iterator next = dfa.states[state].trans.find(ch);
            if (next == dfa.states[state].trans.end()) {
                break;
            }

            // 有转移则进入下一个状态，并消耗一个字符。
            state = next->second;
            cursor++;

            // 如果新状态是接受态，记录当前最长可接受位置。
            if (dfa.states[state].acceptRule >= 0) {
                lastAcceptRule = dfa.states[state].acceptRule;
                lastAcceptPos = cursor;
            }
        }

        // 如果至少到达过一次接受态，说明识别出了一个 token。
        if (lastAcceptRule >= 0) {
            // 截取 token 原始文本。
            std::string lexeme = source.substr(pos, lastAcceptPos - pos);

            // 构造 Token，并加入结果列表。
            result.tokens.push_back(makeToken(lexeme, rules[lastAcceptRule], line, column));

            // 更新行列号和扫描位置，准备识别下一个 token。
            updatePosition(source, pos, lastAcceptPos, line, column);
            pos = lastAcceptPos;
            continue;
        }

        // 如果没有任何接受态，说明当前字符无法组成合法 token。
        hasError = true;
        errors << "Lexical error at line " << line
               << ", column " << column
               << ": unexpected character '" << describeChar(source[pos]) << "'.\n";

        // 错误恢复：跳过当前非法字符，继续扫描后面的输入。
        updatePosition(source, pos, pos + 1, line, column);
        pos++;
    }

    // 扫描结束，补 EOF token。
    result.tokens.push_back(makeEOFToken(line, column));

    // 如果出现过词法错误，则 success 为 false。
    result.success = !hasError;
    result.errorMessage = errors.str();
    return result;
}
```

## 12. 最长匹配机制

最长匹配依赖两个变量：

```cpp
int lastAcceptRule = -1;
int lastAcceptPos = -1;
```

扫描时不会一到接受态就立刻输出 token，而是继续往后走。如果后面还能形成更长的 token，就更新 `lastAcceptPos`。

例子：

```c
a >= b
```

扫描到 `>` 时，`>` 是一个合法 token；继续读 `=` 后，`>=` 也是合法 token。最终输出 `>=`。

例子：

```c
intx
```

虽然前缀 `int` 是关键字，但继续读到 `x` 后，整个 `intx` 符合标识符规则。最终输出 `IDN intx`，而不是 `KW int` 加 `IDN x`。

## 13. 错误处理

如果从当前位置开始，DFA 没有到达过任何接受态：

```cpp
if (lastAcceptRule < 0) {
    hasError = true;
    errors << "...";
    pos++;
}
```

当前实现采用简单错误恢复策略：

- 记录错误行列号。
- 跳过当前非法字符。
- 继续扫描后续内容。

这样一个文件中出现多个非法字符时，可以尽量一次性报出多个错误。

## 14. 示例

输入：

```c
int main() {
  return 0;
}
```

扫描结果大致为：

```text
lexeme  type        grammar   attr
int     Keyword     int       1
main    Keyword     main      5
(       Separator   (         20
)       Separator   )         21
{       Separator   {         22
return  Keyword     return    3
0       Integer     IntConst  0
;       Separator   ;         24
}       Separator   }         23
EOF     EndOfFile   EOF       EOF
```

可以用临时测试命令运行词法分析：

```bash
make run-lexer INPUT=tests/ok_001_minimal_return.sy
```

输出文件：

```text
.tmp_build/token.tsv
```

## 15. 当前实现的边界

- 没有实现物理双缓冲区，而是一次性读入 `std::string`，用 `pos` 和 `cursor` 模拟扫描指针。
- 没有处理注释，因为大作业文档没有明确要求注释规则。
- 单独 `!` 的属性目前是 `"!"`，因为文档运算符编号表没有给它编号，但语法要求支持一元 `!`。
- 浮点数严格按 `[0-9]+'.'[0-9]+` 识别，`1.` 和 `.5` 不作为合法浮点数字面量。

## 16. 和课件结构的对应关系

课件中的词法分析器结构可以对应到当前实现：

| 课件概念 | 当前实现 |
|---|---|
| 源程序文本 | `source` 字符串 |
| 输入缓冲区 | `source` + 下标 `pos` |
| 扫描缓冲区 | `source` + 下标 `cursor` |
| 预处理 | `isBlank`、`updatePosition` |
| 扫描器 | `Lexer::tokenize` 中的 DFA 转移循环 |
| 单词符号输出 | `result.tokens` |
| 错误信息列表 | `result.errorMessage` |
| 状态转换图 | `automata.cpp` 中构造出的最小化 DFA |
