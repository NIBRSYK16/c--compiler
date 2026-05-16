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
- [x] 调用third-part已有函数得到的是label_entry而非entry输出偏差待解决
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

### 2026-05-08
检查调用第三方库的细节并根据编译原理大作业.docx进行中间代码生成部分的修正
- [x] create_iand和create_ior第三方库内部调用create_sdiv有问题 不知能否修改第三方库
- [ ] 调用third-part已有函数得到的是label_entry而非entry输出偏差是因为第三方库内部自动加入"label_"前缀 不知能否修改第三方库
- [x] 增加全局变量支持 区分全局和局部作用域 使用全局数据区而非栈分配进行存储
- [x] cline给出的所有单独AST节点到中间代码生成的样例均运行成功没有看出什么问题

### 2026-05-16
对于visitBinaryExpr中&&和||的处理逻辑进行修改使用比较指令条件跳转和基本块实现短路求值
- [x] 添加辅助函数handleLogicalAnd和handleLogicalOr
- [x] 改变visitBinaryExpr结构不针对逻辑表达式计算右值 &&和||调用新的辅助函数不使用IRBuilder
- [x] 针对ir_invalid_311_logic_and_or.sy样例进行生成测试
- [x] 完善visitIfStmt使得中间代码生成使用唯一的基本块名称