# AST 约定

Parser 生成什么样的 AST，IRGenerator 怎么读取。

## ASTNode 怎么看

每个节点都是一个 `ASTNode`：

```cpp
struct ASTNode {
    std::string name;     // 节点类型，比如 FuncDef、BinaryExpr
    std::string value;    // 节点自己的值，比如 main、+、123
    int line;
    int column;
    std::vector<ASTNode*> children;
};
```

简单理解：

- `name`：这个节点是什么。
- `value`：这个节点额外带的值，没有就留空。
- `children`：这个节点下面接了哪些子节点，顺序必须按下面表格来。

创建节点时尽量这样写，不要手写字符串：

```cpp
ASTNode* node = createNode(ASTName::FuncDef, "main");
```

添加子节点：

```cpp
node->addChild(createNode(ASTName::Type, "int"));
```

如果子节点已经提前存在，例如 `child`，就这样加：

```cpp
node->addChild(child);
```

## 节点表

| 节点名 | value 放什么 | children 顺序 |
|---|---|---|
| `CompUnit` | 空 | 程序里的全局声明和函数，按源码顺序 |
| `ConstDecl` | 空 | 第 0 个是 `Type`，后面是多个 `ConstDef` |
| `ConstDef` | 常量名 | 第 0 个是初始化表达式 |
| `VarDecl` | 空 | 第 0 个是 `Type`，后面是多个 `VarDef` |
| `VarDef` | 变量名 | 没初始化就没有子节点；有初始化时第 0 个是初始化表达式 |
| `FuncDef` | 函数名 | 第 0 个是返回 `Type`，第 1 个是 `ParamList`，第 2 个是 `Block` |
| `Type` | `int` / `float` / `void` | 无 |
| `ParamList` | 空 | 多个 `Param`，按参数顺序；无参数时为空 |
| `Param` | 参数名 | 第 0 个是 `Type` |
| `Block` | 空 | 代码块里的声明和语句，按源码顺序 |
| `AssignStmt` | 空 | 第 0 个是 `LVal`，第 1 个是右侧表达式 |
| `ExprStmt` | 空 | 空语句没有子节点；表达式语句第 0 个是表达式 |
| `IfStmt` | 空 | 第 0 个是条件，第 1 个是 then 语句，有 else 时第 2 个是 else 语句 |
| `ReturnStmt` | 空 | `return;` 没有子节点；`return exp;` 第 0 个是表达式 |
| `LVal` | 变量名 | 无 |
| `IntLiteral` | 整数文本，比如 `123` | 无 |
| `FloatLiteral` | 浮点数文本，比如 `1.5` | 无 |
| `UnaryExpr` | 一元运算符：`+` / `-` / `!` | 第 0 个是操作数 |
| `BinaryExpr` | 二元运算符：`+ - * / % < > <= >= == != && \|\|` | 第 0 个是左表达式，第 1 个是右表达式 |
| `CallExpr` | 函数名 | 函数实参，按调用顺序 |

## 例子 1：函数

源码：

```c
int add(int a, int b) {
    return a + b;
}
```

AST：

```text
CompUnit
  FuncDef value=add
    Type value=int
    ParamList
      Param value=a
        Type value=int
      Param value=b
        Type value=int
    Block
      ReturnStmt
        BinaryExpr value=+
          LVal value=a
          LVal value=b
```

## 例子 2：变量和表达式

源码：

```c
int x = (a + 1) * -b;
```

AST：

```text
CompUnit
  VarDecl
    Type value=int
    VarDef value=x
      BinaryExpr value=*
        BinaryExpr value=+
          LVal value=a
          IntLiteral value=1
        UnaryExpr value=-
          LVal value=b
```

## 最重要的约定

- AST 根节点必须是 `CompUnit`。
- 不要生成 `AddExp`、`MulExp`、`PrimaryExp` 这类中间语法节点。
- 变量使用统一生成 `LVal`。
- 函数调用统一生成 `CallExpr`。
- `FuncDef` 必须永远有 3 个子节点：`Type`、`ParamList`、`Block`。
- `ParamList` 即使没有参数，也要保留这个节点。
