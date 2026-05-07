## 开发日志（kmr）

### 2026-04-29
- 先尝试使用一下fork后仓库的拉取和提交


### 2026-04-30
- 实现实验指导书中要求的visitor框架
- 为中间代码生成阶段加上符号表处理和值栈处理
- 对于正确样例ast.txt的所有节点实现了visit处理
- 实现visitIntLiteral 创建整数常量并压入值栈
- 实现visitReturnStmt 创建返回指令
- 实现visitIdent 变量查找和加载
- 实现visitCompUnit visitFuncDef visitBlock框架

### 2026-05-01
整理可用third-part部分
- 类别	方法名	参数	返回值	状态
- 内存分配	create_alloca	Type*	AllocaInst*	
- 算术运算	create_iadd	Value*, Value*	BinaryInst*	
- 算术运算	create_isub	Value*, Value*	BinaryInst*	
- 算术运算	create_imul	Value*, Value*	BinaryInst*	
- 算术运算	create_isdiv	Value*, Value*	BinaryInst*	
- 算术运算	create_irem	Value*, Value*	BinaryInst*	
- 比较运算	create_icmp_eq	Value*, Value*	CmpInst*	
- 比较运算	create_icmp_ne	Value*, Value*	CmpInst*	
- 比较运算	create_icmp_lt	Value*, Value*	CmpInst*	
- 比较运算	create_icmp_le	Value*, Value*	CmpInst*	
- 比较运算	create_icmp_gt	Value*, Value*	CmpInst*	
- 比较运算	create_icmp_ge	Value*, Value*	CmpInst*	
- 控制流	create_br	BasicBlock*	BranchInst*	
- 控制流	create_cond_br	Value*, BasicBlock*, BasicBlock*	BranchInst*	
- 内存访问	create_load	Value*	LoadInst*	
- 内存访问	create_store	Value*, Value*	StoreInst*	
- 返回	create_ret	Value*	ReturnInst*	
- 返回	create_void_ret	无	ReturnInst*	

### 2026-05-06
对于已经实现过的visitor函数根据AST节点的规定进行修改和补充
- visit函数中的节点类型匹配按照AST_SPEC进行调整
- [x] 修改visit中if/else语句使用的名字
- [x] 完善visitFuncDef visitCompUnit visitBlock visitReturnStmt visitIntLiteral visitIdent visitVarDecl visitType函数
- [x] 符号表栈管理
- [x] 对于正确样例case1_ok/ast.txt进行测试 除了label_entry有问题其它和样例一致
- [ ] 调用third-part已有函数得到的是label_entry而非entry输出偏差待解决
- [x] visitBinaryExpr 二元表达式实现
- [x] visitUnaryExpr 一元表达式实现
- [x] visitAssignStmt 赋值语句实现
- [x] visitLVal 左值空函数
- [x] 第三方库可以声明float类型变量FloatType但是没有找到支持float类型运算和float常量定义的相关函数

### 2026-05-07
继续实现其它仅占位的visitor函数
- [x] visitConstDecl 常量声明
- [x] visitConstDef 常量定义
- [x] visitFloatLiteral 浮点数常量(定义不支持)
- [x] visitCallExpr 函数调用
- [x] visitExprStmt 表达式语句
- [x] visitIfStmt 条件语句
- [x] visitParamList 参数列表
- [x] visitParam 参数
- [x] visitVarDef 变量定义(从visitVarDecl中分离)
- [x] 对于float相关类型定义/声明/运算处增加类型检查