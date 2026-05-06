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

### 2026-05-01
对于已经实现过的visitor函数根据AST节点的规定进行修改和补充
- visit函数中的节点类型匹配按照AST_SPEC进行调整
- [x] 修改visit中if/else语句使用的名字
- [x] 完善visitFuncDef&visitCompUnit&visitBlock&visitReturnStmt&visitIntLiteral函数
- [ ] 确定visitIdent函数中关于符号表存储的细节


