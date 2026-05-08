# Parser 诊断增强说明

本文档记录语法分析模块在正式 SLR 核心之外增加的 token 流预检能力。该部分不替代 FIRST/FOLLOW、LR(0) 项目集、SLR 表和规约过程，而是在进入 SLR 分析前先做一层轻量但实用的诊断。

## 1. 设计目标

成熟编译器前端通常不会只在分析表报错后给出一句“syntax error”，而会尽量提前发现结构性问题，并给出位置、错误类型和修复建议。本项目新增的预检层主要解决这些问题：

- token 流是否包含且只包含一个 EOF。
- 括号和大括号是否成对匹配。
- `else` 是否存在明显缺失的 `if`。
- 二元运算符右侧是否缺少操作数。
- 两个表达式 token 是否异常相邻。
- 统计括号/块嵌套深度，辅助后续调试 SLR 状态栈。

## 2. 相关接口

定义位置：

- `include/c--/parser/parser.h`
- `src/parser/parser.cpp`

核心结构：

```cpp
struct TokenStreamSummary {
    bool success;
    int tokenCount;
    int eofCount;
    int maxParenDepth;
    int maxBraceDepth;
    std::vector<Diagnostic> diagnostics;
};
```

Parser 提供独立接口：

```cpp
TokenStreamSummary Parser::precheckTokenStream(const std::vector<Token>& tokens) const;
```

这样后续 SLR Parser 完成后也可以继续复用这层检查：先做 token 质量门禁，再进入正式移进规约。

## 3. 诊断格式

诊断统一使用 `include/c--/common/Diagnostic.h` 中的结构：

```cpp
struct Diagnostic {
    DiagnosticSeverity severity;
    std::string code;
    std::string message;
    SourceRange range;
    std::string hint;
};
```

示例输出：

```text
error[SYN_UNCLOSED_DELIMITER] at line 5, column 12: unclosed delimiter '('
  hint: add a matching ')' later in the same syntactic region
```

## 4. Driver 开关

临时 parser driver 支持通过环境变量输出预检摘要：

```bash
PARSER_PREFLIGHT=1 make run-parser
```

输出包括：

- token 总数。
- EOF 数量。
- 最大括号嵌套深度。
- 最大代码块嵌套深度。
- 诊断条数和诊断详情。

如果需要机器可读输出，可以打开 JSON Lines 诊断：

```bash
DIAGNOSTICS_JSON=1 PARSER_PREFLIGHT=1 make run-parser
```

每条诊断是一行 JSON，方便后续接入自动化测试或生成报告。

## 5. 和 SLR 主线的关系

该功能只做 token 流级别的结构检查，不构造文法、不计算 FIRST/FOLLOW、不生成 LR 项目集，也不输出正式规约序列，因此不会和 SLR 主线开发冲突。

后续 SLR 完成后，可以把 `precheckTokenStream` 放在 `parse` 的入口处：

1. 若预检发现严重错误，直接返回结构化诊断。
2. 若预检只有 warning 或完全通过，再进入 SLR 分析表驱动过程。
3. SLR 报错时复用 `Diagnostic`，把期望终结符、当前 token、状态栈摘要一起输出。
