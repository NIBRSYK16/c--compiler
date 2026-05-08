# C-- 实时命令行编辑器

`cminus_editor` 是一个面向 C-- 源码的轻量命令行文本编辑器。它不是完整 IDE，但把编辑、语法高亮、词法分析和 Parser 预检串在了一起，适合演示编译器前端的实时反馈能力。

## 1. 编译和启动

编译：

```bash
make editor
```

打开默认输入文件：

```bash
make run-editor
```

直接指定文件：

```bash
.tmp_build/cminus_editor tests/case1_ok/input.sy
```

如果终端不支持 ANSI 颜色，可以关闭颜色：

```bash
NO_COLOR=1 .tmp_build/cminus_editor tests/case1_ok/input.sy
```

## 2. 核心能力

- 命令行内编辑 `.sy` 源码。
- 每次插入、替换、删除后自动运行词法分析和语法预检。
- 用 ANSI 颜色做语法高亮：
  - 关键字：紫色。
  - 标识符：青色。
  - 数字：黄色。
  - 运算符：蓝色。
  - 界符：绿色。
- 输出 token 数量、词法状态、语法预检状态、错误数量、warning 数量和括号/代码块嵌套深度。
- 支持查看 token 表，方便调试 Parser 输入。
- 支持保存修改后的源码。

## 3. 命令

```text
:open <file>             打开文件
:save [file]             保存到当前文件或指定文件
:show [from] [to]        显示带行号和高亮的源码
:insert <line> <text>    在指定行前插入一行
:append <text>           在末尾追加一行
:replace <line> <text>   替换指定行
:delete <line>           删除指定行
:analyze                 手动运行词法分析和语法预检，并输出完整诊断
:tokens                  输出 token 表
:live on|off             开关实时分析
:color on|off            开关 ANSI 高亮
:help                    显示帮助
:quit                    退出
```

直接输入非命令文本时，会把这一行追加到当前缓冲区，并触发实时分析。

## 4. 示例

启动后输入：

```text
:show
:replace 2     return 1 +;
:analyze
```

可以看到 Parser 预检会提示二元运算符缺少右操作数。再输入：

```text
:replace 2     return 1 + 2;
:analyze
```

诊断会恢复为预检通过。

## 5. 和 Parser 的关系

这个工具调用的是：

- `Lexer::tokenize(...)`
- `Parser::precheckTokenStream(...)`

因此它不依赖正式 SLR Parser 已经完成，也不会替代 SLR 规约过程。等后续 SLR 核心实现后，可以继续把真正的 Parser 错误接入编辑器，实现更完整的实时语法反馈。
