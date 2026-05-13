#include "c--/ir/IRGenerator.h"
#include "third_part/compiler_ir/include/Module.h"
#include "third_part/compiler_ir/include/Function.h"
#include "third_part/compiler_ir/include/BasicBlock.h"
#include "third_part/compiler_ir/include/Constant.h"
#include "third_part/compiler_ir/include/IRbuilder.h"
#include "third_part/compiler_ir/include/IRprinter.h"
#include "third_part/compiler_ir/include/Type.h"
#include "third_part/compiler_ir/include/GlobalVariable.h"
#include <stdexcept>
#include <map>
#include <vector>
#include <string>
#include <cassert>

namespace cminus
{
    namespace
    {
        class IRVisitor
        {
        private:
            Module *module = nullptr;
            Function *currentFunc = nullptr;
            BasicBlock *currentBB = nullptr;
            IRBuilder *builder = nullptr;
            std::vector<std::map<std::string, Value *>> symbolTableStack; // 符号表栈
            std::vector<Value *> valueStack;                              // 表达式值栈
            std::map<std::string, GlobalVariable *> globalSymbolTable;    // 全局符号表
            bool isGlobalScope = false;                                   // 当前是否在全局作用域

        public:
            IRVisitor() = default;
            Value *popValue() // 取出表达式值栈栈顶
            {
                if (valueStack.empty())
                {
                    return nullptr;
                }
                Value *val = valueStack.back();
                valueStack.pop_back();
                return val;
            }

            void pushValue(Value *val) // 将值压入表达式值栈栈顶
            {
                valueStack.push_back(val);
            }
            std::string generate(const ASTNode *root)
            {
                // 调试部分
                //  std::cerr << "=== DEBUG: Starting IR generation ===" << std::endl;
                //  static int generateCount = 0;
                //  generateCount++;
                //  std::cerr << "DEBUG generate call #" << generateCount << std::endl;

                if (!root)
                {
                    std::cerr << "ERROR: AST root is null" << std::endl;
                    throw std::runtime_error("AST root is null");
                }
                // 调试部分
                // std::cerr << "DEBUG: Root node: name=" << root->name
                //           << ", value=" << root->value
                //           << ", children=" << root->children.size() << std::endl;

                // 只调用一次reset()
                reset(); // 重置状态支持多次调用

                if (module)
                {
                    // std::cerr << "WARNING: module already exists, deleting..." << std::endl;
                    delete module;
                    module = nullptr;
                }

                module = new Module("main");
                // std::cerr << "DEBUG: Created module at " << module << std::endl;
                builder = new IRBuilder(nullptr, module);

                try
                {
                    visit(root);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "ERROR in visit: " << e.what() << std::endl;
                    throw;
                }

                std::string irText = module->print();
                // std::cerr << "DEBUG: Raw IR text length: " << irText.length() << std::endl;

                return irText;
            }
            void visit(const ASTNode *node)
            {
                // 调试使用
                //  static int callCount = 0;
                //  callCount++;
                //  std::cerr << "DEBUG visit #" << callCount << ": " << node->name
                //            << " value=" << node->value << std::endl;
                if (!node)
                    return;
                if (node->name == "CompUnit")
                {
                    // std::cerr << "DEBUG: Visiting CompUnit with " << node->children.size()
                    //           << " children" << std::endl;
                    visitCompUnit(node);
                    return;
                }
                else if (node->name == "FuncDef")
                {
                    visitFuncDef(node);
                    return;
                }
                else if (node->name == "Type")
                {
                    visitType(node);
                    return;
                }
                else if (node->name == "Block")
                {
                    visitBlock(node);
                    return;
                }
                else if (node->name == "ReturnStmt")
                {
                    visitReturnStmt(node);
                    return;
                }
                else if (node->name == "IntLiteral")
                {
                    visitIntLiteral(node);
                    return;
                }
                else if (node->name == "BinaryExpr")
                {
                    visitBinaryExpr(node);
                    return;
                }
                else if (node->name == "VarDecl")
                {
                    visitVarDecl(node);
                    return;
                }
                else if (node->name == "VarDef")
                {
                    // 为了完整性添加默认处理
                    // 在visitVarDecl中处理
                    return;
                }
                else if (node->name == "ConstDecl")
                {
                    visitConstDecl(node);
                    return;
                }
                else if (node->name == "ConstDef")
                {
                    // 为了完整性添加默认处理
                    // 在visitConstDecl中处理
                    return;
                }
                else if (node->name == "AssignStmt")
                {
                    visitAssignStmt(node);
                    return;
                }
                else if (node->name == "Ident")
                {
                    visitIdent(node);
                    return;
                }
                else if (node->name == "LVal")
                {
                    visitLVal(node);
                    return;
                }
                else if (node->name == "FloatLiteral")
                {
                    visitFloatLiteral(node);
                    return;
                }
                else if (node->name == "UnaryExpr")
                {
                    visitUnaryExpr(node);
                    return;
                }
                else if (node->name == "CallExpr")
                {
                    visitCallExpr(node);
                    return;
                }
                else if (node->name == "ExprStmt")
                {
                    visitExprStmt(node);
                    return;
                }
                else if (node->name == "IfStmt")
                {
                    visitIfStmt(node);
                    return;
                }
                else if (node->name == "ParamList")
                {
                    visitParamList(node);
                    return;
                }
                else if (node->name == "Param")
                {
                    visitParam(node);
                    return;
                }
                else
                {
                    // 对于未知节点类型，递归访问子节点
                    for (auto &child : node->children)
                    {
                        visit(child);
                    }
                }
            }
            void visitCompUnit(const ASTNode *node)
            {
                // 进入全局作用域
                isGlobalScope = true;
                // 创建Module程序根节点
                for (auto &child : node->children)
                {
                    visit(child);
                }
                // 离开全局作用域
                isGlobalScope = false;
            }
            void visitFuncDef(const ASTNode *node)
            {
                // 进入函数作用域，设置非全局作用域
                isGlobalScope = false;
                // 调试使用
                // static int funcDefCount = 0;
                // funcDefCount++;
                // std::cerr << "DEBUG visitFuncDef #" << funcDefCount
                //           << ": " << node->value << std::endl;
                // 提取函数名
                std::string funcName = node->value; // 直接从节点value获取

                // 检查子节点结构
                if (node->children.size() < 3)
                {
                    throw std::runtime_error("FuncDef must have at least 3 children: Type, ParamList, Block");
                }

                // 第0个子节点是返回类型
                const ASTNode *returnTypeNode = node->children[0];
                if (returnTypeNode->name != "Type")
                {
                    throw std::runtime_error("First child of FuncDef must be Type");
                }
                std::string returnTypeName = returnTypeNode->value;
                if (returnTypeName == "float") // 函数类型不支持float
                {
                    throw std::runtime_error("Function '" + funcName + "' cannot return float type at line " +
                                             std::to_string(node->line) +
                                             " (C-- grammar only allows 'int' or 'void' as return types)");
                }
                Type *returnType = getTypeFromName(returnTypeName);

                // 第1个子节点是参数列表
                const ASTNode *paramListNode = node->children[1];
                if (paramListNode->name != "ParamList")
                {
                    throw std::runtime_error("Second child of FuncDef must be ParamList");
                }

                // 处理参数
                std::vector<Type *> paramTypes;
                std::vector<std::string> paramNames;

                for (auto &paramChild : paramListNode->children)
                {
                    if (paramChild->name == "Param")
                    {
                        // Param节点：value=参数名，第0个子节点是Type
                        if (!paramChild->children.empty())
                        {
                            const ASTNode *paramTypeNode = paramChild->children[0];
                            if (paramTypeNode->name == "Type")
                            {
                                Type *paramType = getTypeFromName(paramTypeNode->value);
                                paramTypes.push_back(paramType);
                                paramNames.push_back(paramChild->value);
                            }
                        }
                    }
                }

                // 创建函数类型
                FunctionType *funcType = FunctionType::get(returnType, paramTypes);
                Function *func = Function::create(funcType, funcName, module);
                // module->add_function(func);
                currentFunc = func;

                // 创建入口基本块
                BasicBlock *entryBB = BasicBlock::create(module, "entry", func); // 输出是label_entry
                // BasicBlock *entryBB = BasicBlock::create(module, "", func);// 输出是label
                // func->add_basic_block(entryBB);
                currentBB = entryBB;
                builder->set_insert_point(entryBB);

                // 进入函数作用域
                enterScope();

                // 为参数创建alloca指令并加入符号表
                auto argIt = func->get_args().begin();
                for (size_t i = 0; i < paramNames.size() && argIt != func->get_args().end(); i++, ++argIt)
                {
                    AllocaInst *paramAlloca = builder->create_alloca(paramTypes[i]);

                    // Argument* 继承自 Value* 可以直接使用
                    Value *argValue = *argIt; // 这已经是Value*了

                    // 存储参数值到alloca空间
                    builder->create_store(argValue, paramAlloca);

                    // 将alloca指令加入符号表，而不是参数值
                    addSymbol(paramNames[i], paramAlloca);
                }

                // 处理函数体（第2个子节点是Block）
                if (node->children.size() > 2)
                {
                    const ASTNode *blockNode = node->children[2];
                    if (blockNode->name == "Block")
                    {
                        visitBlock(blockNode);
                    }
                }

                // 如果基本块没有终止指令，添加默认返回
                // if (!currentBB->get_terminator())
                // {
                //     if (returnType->is_int32_type())
                //     {
                //         ConstantInt *zero = ConstantInt::get(0, module);
                //         builder->create_ret(zero);
                //     }
                //     else if (returnType->is_void_type())
                //     {
                //         builder->create_void_ret();
                //     }
                // }

                // 退出函数作用域
                exitScope();

                currentFunc = nullptr;
                currentBB = nullptr;
            }
            void visitType(const ASTNode *node)
            {
                // 确定函数返回类型
            }
            void visitBlock(const ASTNode *node)
            {
                // 创建BasicBlock
                // 进入新的作用域
                enterScope();

                // 处理块内的所有语句
                for (auto &child : node->children)
                {
                    visit(child);
                }

                // 退出作用域
                exitScope();
            }
            void visitReturnStmt(const ASTNode *node)
            {
                // 创建返回指令
                if (!node->children.empty())
                {
                    visit(node->children[0]);
                    Value *retValue = popValue(); // 从值栈弹出返回值
                    if (!retValue)
                    {
                        throw std::runtime_error("Return value not found in value stack");
                    }
                    builder->create_ret(retValue);
                }
                else
                {
                    builder->create_void_ret(); // void返回
                }
            }
            void visitIntLiteral(const ASTNode *node)
            {
                // 创建整数常量
                int intValue = std::stoi(node->value); // AST节点获得整数值
                ConstantInt *constInt = ConstantInt::get(intValue, module);
                pushValue(constInt); // 将常量值压入值栈
            }
            void visitBinaryExpr(const ASTNode *node)
            {
                // 处理二元表达式
                std::string op = node->value;

                // 检查子节点数量
                if (node->children.size() < 2)
                {
                    throw std::runtime_error("BinaryExpr must have 2 children");
                }

                // 计算左表达式
                visit(node->children[0]);
                Value *left = popValue();

                // 计算右表达式
                visit(node->children[1]);
                Value *right = popValue();

                if (!left || !right)
                {
                    throw std::runtime_error("Missing operand in binary expression");
                }

                // 检查类型：不支持float运算
                if (left->get_type()->is_float_type() || right->get_type()->is_float_type())
                {
                    std::string leftType = left->get_type()->is_float_type() ? "float" : "int";
                    std::string rightType = right->get_type()->is_float_type() ? "float" : "int";
                    throw std::runtime_error("Binary operation '" + op + "' with " + leftType +
                                             " and " + rightType + " operands is not supported (float operations are not implemented)");
                }

                // 只支持整数类型
                if (!left->get_type()->is_integer_type() || !right->get_type()->is_integer_type())
                {
                    throw std::runtime_error("Binary expression only supports integer types");
                }

                Value *result = nullptr;
                Type *type = left->get_type();

                // 整数类型运算
                if (type->is_integer_type())
                {
                    if (op == "+")
                    {
                        result = builder->create_iadd(left, right);
                    }
                    else if (op == "-")
                    {
                        result = builder->create_isub(left, right);
                    }
                    else if (op == "*")
                    {
                        result = builder->create_imul(left, right);
                    }
                    else if (op == "/")
                    {
                        result = builder->create_isdiv(left, right);
                    }
                    else if (op == "%")
                    {
                        result = builder->create_irem(left, right);
                    }
                    // 比较运算返回i1类型（布尔）
                    else if (op == "==")
                    {
                        result = builder->create_icmp_eq(left, right);
                    }
                    else if (op == "!=")
                    {
                        result = builder->create_icmp_ne(left, right);
                    }
                    else if (op == "<")
                    {
                        result = builder->create_icmp_lt(left, right);
                    }
                    else if (op == "<=")
                    {
                        result = builder->create_icmp_le(left, right);
                    }
                    else if (op == ">")
                    {
                        result = builder->create_icmp_gt(left, right);
                    }
                    else if (op == ">=")
                    {
                        result = builder->create_icmp_ge(left, right);
                    }
                    // 逻辑运算（需要转换为布尔值）
                    else if (op == "&&" || op == "||")
                    {
                        // 先将整数转换为布尔值
                        Value *leftBool = builder->create_icmp_ne(left, ConstantInt::get(0, module));
                        Value *rightBool = builder->create_icmp_ne(right, ConstantInt::get(0, module));

                        if (op == "&&")
                        {
                            result = builder->create_iand(leftBool, rightBool);
                        }
                        else
                        { // "||"
                            result = builder->create_ior(leftBool, rightBool);
                        }
                    }
                    else
                    {
                        throw std::runtime_error("Unsupported binary operator for integer: " + op);
                    }
                }
                // 浮点数类型运算
                else if (type->is_float_type())
                {
                    // 暂时不支持浮点数
                    throw std::runtime_error("Float binary operations not implemented yet");
                }
                else
                {
                    throw std::runtime_error("Unsupported type for binary expression");
                }

                pushValue(result);
            }
            void visitVarDecl(const ASTNode *node)
            {
                // 处理变量声明
                // 第0个子节点是Type
                if (node->children.empty())
                    return;
                const ASTNode *typeNode = node->children[0];
                Type *varType = getTypeFromName(typeNode->value);

                // 处理后续的VarDef节点
                for (size_t i = 1; i < node->children.size(); i++)
                {
                    const ASTNode *varDefNode = node->children[i];
                    if (varDefNode->name == "VarDef")
                    {
                        visitVarDef(varDefNode, varType); // 调用visitVarDef函数
                    }
                }
            }
            void visitVarDef(const ASTNode *node, Type *varType)
            {
                // 变量定义函数
                std::string varName = node->value;

                if (isGlobalScope)
                {
                    // 处理全局变量
                    // 检查是否已存在同名全局变量
                    if (globalSymbolTable.find(varName) != globalSymbolTable.end())
                    {
                        throw std::runtime_error("Duplicate global variable definition: " + varName);
                    }

                    // 处理初始化表达式
                    Constant *initValue = nullptr;
                    if (!node->children.empty())
                    {
                        // 计算初始化表达式
                        const ASTNode *initExprNode = node->children[0];
                        visit(initExprNode);
                        Value *exprValue = popValue();

                        if (exprValue)
                        {
                            // 将Value*转换为Constant*
                            if (auto *constInt = dynamic_cast<ConstantInt *>(exprValue))
                            {
                                initValue = constInt;
                            }
                            else if (exprValue->get_type()->is_float_type())
                            {
                                // 第三方库没有ConstantFloat但是float全局变量初始化需要常量 进行报错
                                throw std::runtime_error("Global float variable initialization is not supported: " + varName);
                            }
                            else
                            {
                                throw std::runtime_error("Global variable initialization must be constant expression");
                            }
                        }
                    }

                    // 创建全局变量
                    GlobalVariable *gv = GlobalVariable::create(varName, module, varType, false, initValue);

                    // 添加到全局符号表
                    addGlobalSymbol(varName, gv);
                }
                else
                {
                    // 处理局部变量
                    // 创建alloca指令
                    AllocaInst *alloca = builder->create_alloca(varType);
                    addSymbol(varName, alloca);

                    // 如果有初始化表达式
                    if (!node->children.empty())
                    {
                        visit(node->children[0]); // 计算初始化表达式
                        Value *initValue = popValue();
                        if (initValue)
                        {
                            // 检查float变量初始化
                            if (varType->is_float_type() || initValue->get_type()->is_float_type())
                            {
                                if (varType->is_float_type() && initValue->get_type()->is_float_type())
                                {
                                    throw std::runtime_error("Float variable '" + varName + "' initialization with float value at line " +
                                                             std::to_string(node->line) +
                                                             " is not supported (float operations are not implemented)");
                                }
                            }
                            // 类型检查
                            if (initValue->get_type() != varType)
                            {
                                throw std::runtime_error("Type mismatch in variable initialization");
                            }
                            builder->create_store(initValue, alloca);
                        }
                    }
                }
            }
            void visitConstDecl(const ASTNode *node)
            {
                // 处理常量声明
                // 第0个子节点是Type
                if (node->children.empty())
                    return;

                const ASTNode *typeNode = node->children[0];
                if (typeNode->name != "Type")
                {
                    throw std::runtime_error("First child of ConstDecl must be Type");
                }

                Type *constType = getTypeFromName(typeNode->value);

                // 处理后续的ConstDef节点
                for (size_t i = 1; i < node->children.size(); i++)
                {
                    const ASTNode *constDefNode = node->children[i];
                    if (constDefNode->name == "ConstDef")
                    {
                        // 调用visitConstDef函数
                        visitConstDef(constDefNode, constType);
                    }
                }
            }
            void visitConstDef(const ASTNode *node, Type *constType)
            {
                // 处理常量定义
                std::string constName = node->value;
                // 常量类型不可以是float
                if (constType->is_float_type())
                {
                    throw std::runtime_error("Constant '" + constName + "' cannot be of float type at line " +
                                             std::to_string(node->line) +
                                             " (float constants are not supported in this implementation)");
                }
                // 检查是否有初始化表达式
                if (node->children.empty())
                {
                    throw std::runtime_error("ConstDef must have initialization expression");
                }

                // 计算初始化表达式
                const ASTNode *initExprNode = node->children[0];
                visit(initExprNode);
                Value *initValue = popValue();

                if (!initValue)
                {
                    throw std::runtime_error("Missing initialization value for constant: " + constName);
                }

                // 类型检查
                if (initValue->get_type() != constType)
                {
                    throw std::runtime_error("Type mismatch in constant initialization: " + constName);
                }

                // 将Value*转换为Constant*
                Constant *constInit = nullptr;
                if (auto *constInt = dynamic_cast<ConstantInt *>(initValue))
                {
                    constInit = constInt;
                }
                else
                {
                    throw std::runtime_error("Constant initialization must be constant expression");
                }

                if (isGlobalScope)
                {
                    // 处理全局常量
                    if (globalSymbolTable.find(constName) != globalSymbolTable.end())
                    {
                        throw std::runtime_error("Duplicate global constant definition: " + constName);
                    }

                    // 创建全局常量
                    GlobalVariable *gv = GlobalVariable::create(constName, module, constType, true, constInit);

                    // 添加到全局符号表
                    addGlobalSymbol(constName, gv);
                }
                else
                {
                    // 处理局部常量
                    // 创建alloca指令分配常量空间
                    AllocaInst *alloca = builder->create_alloca(constType);

                    // 存储初始值
                    builder->create_store(initValue, alloca);

                    // 将常量加入符号表
                    addSymbol(constName, alloca);
                }
            }
            void visitAssignStmt(const ASTNode *node)
            {
                // 处理赋值语句
                if (node->children.size() < 2)
                {
                    throw std::runtime_error("AssignStmt must have 2 children");
                }

                // 先处理右表达式
                const ASTNode *exprNode = node->children[1];
                visit(exprNode);
                Value *exprValue = popValue();

                if (!exprValue)
                {
                    throw std::runtime_error("No value from expression in assignment");
                }

                // 处理左值
                const ASTNode *lvalNode = node->children[0];
                if (lvalNode->name != "LVal")
                {
                    throw std::runtime_error("First child of AssignStmt must be LVal");
                }

                std::string varName = lvalNode->value;
                Value *varAddr = lookupSymbol(varName);
                if (!varAddr)
                {
                    throw std::runtime_error("Undefined variable in assignment: " + varName);
                }
                // 类型检查
                if (varAddr->get_type()->is_pointer_type())
                {
                    PointerType *ptrType = static_cast<PointerType *>(varAddr->get_type());
                    Type *pointedType = ptrType->get_element_type();

                    // 检查赋值类型是否匹配
                    if (exprValue->get_type() != pointedType)
                    {
                        // 如果是float类型赋值给出更具体的错误
                        if (pointedType->is_float_type() || exprValue->get_type()->is_float_type())
                        {
                            throw std::runtime_error("Assignment to/from float variable '" + varName +
                                                     "' at line " + std::to_string(node->line) +
                                                     " is not supported (float operations are not implemented)");
                        }
                        throw std::runtime_error("Type mismatch in assignment to variable '" + varName +
                                                 "' at line " + std::to_string(node->line));
                    }
                }
                // 存储值到变量
                builder->create_store(exprValue, varAddr);
            }
            void visitLVal(const ASTNode *node)
            {
                // 处理变量名
                std::string varName = node->value;
                Value *varAddr = lookupSymbol(varName);

                if (!varAddr)
                {
                    throw std::runtime_error("Undefined variable: " + varName);
                }
                // 加载变量的值
                Value *loadedValue = builder->create_load(varAddr);
                pushValue(loadedValue);
            }
            void visitFloatLiteral(const ASTNode *node)
            {
                // 处理浮点数常量 不支持浮点数常量
                throw std::runtime_error("Float literal '" + node->value + "' at line " +
                                         std::to_string(node->line) + " is not supported (float constants cannot be used)");
            }
            void visitUnaryExpr(const ASTNode *node)
            {
                // 处理一元运算符
                std::string op = node->value; // '+', '-', '!'

                if (node->children.empty())
                {
                    throw std::runtime_error("UnaryExpr must have 1 child");
                }

                // 计算操作数
                visit(node->children[0]);
                Value *operand = popValue();

                if (!operand)
                {
                    throw std::runtime_error("Missing operand in unary expression");
                }

                // 检查类型：不支持float
                if (operand->get_type()->is_float_type())
                {
                    throw std::runtime_error("Unary operation '" + op + "' on float operand is not supported at line " +
                                             std::to_string(node->line));
                }
                // 只支持整数类型
                if (!operand->get_type()->is_integer_type())
                {
                    throw std::runtime_error("Unary expression only supports integer type at line " +
                                             std::to_string(node->line));
                }
                Value *result = nullptr;

                if (op == "+")
                {
                    // 正号，直接返回操作数
                    result = operand;
                }
                else if (op == "-")
                {
                    // 负号：0 - operand
                    if (operand->get_type()->is_integer_type())
                    {
                        Value *zero = ConstantInt::get(0, module);
                        result = builder->create_isub(zero, operand);
                    }
                    else
                    {
                        throw std::runtime_error("Unary minus only supports integer type");
                    }
                }
                else if (op == "!")
                {
                    // 逻辑非：operand == 0 ? 1 : 0
                    if (operand->get_type()->is_integer_type())
                    {
                        Value *zero = ConstantInt::get(0, module);
                        Value *isZero = builder->create_icmp_eq(operand, zero);
                        // 将i1转换为i32
                        result = builder->create_zext(isZero, Type::get_int32_type(module));
                    }
                    else
                    {
                        throw std::runtime_error("Logical not only supports integer type");
                    }
                }
                else
                {
                    throw std::runtime_error("Unsupported unary operator: " + op);
                }

                pushValue(result);
            }
            void visitCallExpr(const ASTNode *node)
            {
                // 处理函数调用
                std::string funcName = node->value;

                // 在模块中查找函数
                Function *func = nullptr;
                for (auto &f : module->get_functions())
                {
                    if (f->get_name() == funcName)
                    {
                        func = f;
                        break;
                    }
                }

                if (!func)
                {
                    throw std::runtime_error("Undefined function: " + funcName);
                }

                // 收集参数值
                std::vector<Value *> args;
                for (auto &child : node->children)
                {
                    visit(child); // 访问每个参数表达式
                    Value *argValue = popValue();
                    if (!argValue)
                    {
                        throw std::runtime_error("Missing argument value in function call");
                    }
                    args.push_back(argValue);
                }
                // 检查参数数量是否匹配
                if (args.size() != func->get_num_of_args())
                {
                    throw std::runtime_error("Argument count mismatch for function: " + funcName);
                }
                // 创建调用指令
                CallInst *call = builder->create_call(func, args);
                // 只有非void返回类型的函数才将结果压入值栈
                Type *returnType = func->get_return_type();
                if (!returnType->is_void_type())
                {
                    pushValue(call); // 将函数调用结果压入值栈
                }
                // void函数没有返回值，不压入值栈
            }
            void visitExprStmt(const ASTNode *node)
            {
                // 处理表达式语句(可能为空或者有表达式)
                if (!node->children.empty())
                {
                    // 有表达式，计算表达式值
                    visit(node->children[0]);
                    Value *exprValue = popValue();
                }
                // 空语句什么也不做
            }
            void visitIfStmt(const ASTNode *node)
            {
                // 处理条件语句
                // 子节点：0=条件，1=then块，2=else块

                if (node->children.size() < 2)
                {
                    throw std::runtime_error("IfStmt must have at least 2 children (condition and then-block)");
                }

                // 访问条件表达式
                const ASTNode *condNode = node->children[0];
                visit(condNode);
                Value *condValue = popValue();

                if (!condValue)
                {
                    throw std::runtime_error("Missing condition value in if statement");
                }

                // 将条件值转换为布尔值（i1类型）
                Value *boolCond = nullptr;
                if (condValue->get_type()->is_integer_type())
                {
                    // 整数转布尔：cond != 0
                    Value *zero = ConstantInt::get(0, module);
                    boolCond = builder->create_icmp_ne(condValue, zero);
                }
                else if (condValue->get_type()->is_int1_type())
                {
                    // 已经是布尔类型
                    boolCond = condValue;
                }
                else
                {
                    throw std::runtime_error("Condition must be integer or boolean type");
                }

                // 获取当前函数
                Function *func = currentFunc;
                if (!func)
                {
                    throw std::runtime_error("If statement not inside a function");
                }

                // 创建基本块
                BasicBlock *thenBB = BasicBlock::create(module, "if_then", func);
                BasicBlock *elseBB = nullptr;
                BasicBlock *mergeBB = BasicBlock::create(module, "if_merge", func);

                // 如果有else块
                bool hasElse = (node->children.size() > 2);
                if (hasElse)
                {
                    elseBB = BasicBlock::create(module, "if_else", func);
                    builder->create_cond_br(boolCond, thenBB, elseBB);
                }
                else
                {
                    builder->create_cond_br(boolCond, thenBB, mergeBB);
                }

                // 处理then块
                builder->set_insert_point(thenBB);
                const ASTNode *thenNode = node->children[1];
                visit(thenNode);

                // then块结束后跳转到合并块
                if (!thenBB->get_terminator())
                {
                    builder->create_br(mergeBB);
                }

                // 处理else块
                if (hasElse && elseBB)
                {
                    builder->set_insert_point(elseBB);
                    const ASTNode *elseNode = node->children[2];
                    visit(elseNode);

                    // else块结束后跳转到合并块
                    if (!elseBB->get_terminator())
                    {
                        builder->create_br(mergeBB);
                    }
                }

                // 设置插入点到合并块继续执行
                builder->set_insert_point(mergeBB);
            }
            void visitParamList(const ASTNode *node)
            {
                // 处理参数序列
                for (auto &child : node->children)
                {
                    visit(child);
                }
            }
            void visitParam(const ASTNode *node)
            {
                // 处理单个参数
                // Param节点：value=参数名，第0个子节点是Type

                std::string paramName = node->value;

                // 获取参数类型
                if (node->children.empty())
                {
                    throw std::runtime_error("Param node must have a Type child");
                }

                const ASTNode *typeNode = node->children[0];
                if (typeNode->name != "Type")
                {
                    throw std::runtime_error("First child of Param must be Type");
                }

                Type *paramType = getTypeFromName(typeNode->value);
                // 为参数创建alloca指令分配空间
                AllocaInst *alloca = builder->create_alloca(paramType);
                // 将参数加入符号表
                addSymbol(paramName, alloca);
            }
            Type *getTypeFromName(const std::string &typeName)
            {
                if (typeName == "int")
                {
                    return Type::get_int32_type(module);
                }
                else if (typeName == "float")
                {
                    return Type::get_float_type(module);
                }
                else if (typeName == "void")
                {
                    return Type::get_void_type(module);
                }
                throw std::runtime_error("Unsupported type: " + typeName);
            }

            void reset() // 清理状态
            {
                currentFunc = nullptr;
                currentBB = nullptr;
                symbolTableStack.clear();
                valueStack.clear();
                globalSymbolTable.clear();
                isGlobalScope = false;
            }
            void enterScope()
            {
                symbolTableStack.emplace_back();
            }

            void exitScope()
            {
                if (!symbolTableStack.empty())
                {
                    symbolTableStack.pop_back();
                }
            }

            Value *lookupSymbol(const std::string &name)
            {
                // 先查局部符号表栈
                for (auto it = symbolTableStack.rbegin(); it != symbolTableStack.rend(); ++it)
                {
                    auto found = it->find(name);
                    if (found != it->end())
                    {
                        return found->second;
                    }
                }
                // 再查全局符号表
                auto globalFound = globalSymbolTable.find(name);
                if (globalFound != globalSymbolTable.end())
                {
                    return globalFound->second;
                }
                return nullptr;
            }
            void addGlobalSymbol(const std::string &name, GlobalVariable *gv)
            // 添加全局符号
            {
                globalSymbolTable[name] = gv;
            }
            void addSymbol(const std::string &name, Value *value)
            {
                if (symbolTableStack.empty())
                {
                    symbolTableStack.emplace_back();
                }
                symbolTableStack.back()[name] = value;
            }
            void visitIdent(const ASTNode *node)
            {
                // 从符号表中查找变量
                std::string varName = node->value;
                Value *varAddr = lookupSymbol(varName);

                if (!varAddr)
                {
                    throw std::runtime_error("Undefined variable: " + varName);
                }

                // 确保varAddr是AllocaInst*指针类型或GlobalVariable*指针类型
                if (varAddr->get_type()->is_pointer_type())
                {
                    // 创建load指令加载变量值
                    Value *loadedValue = builder->create_load(varAddr);
                    pushValue(loadedValue);
                }
                else
                {
                    // 如果不是指针则直接使用
                    pushValue(varAddr);
                }
            }
            ~IRVisitor()
            {
                delete builder;
                delete module;
            }
        };
    }

    IRResult
    IRGenerator::generate(const ASTNode *root)
    {
        IRResult result;

        try
        {
            if (root == nullptr)
            {
                result.success = false;
                result.errorMessage = "IRGenerator received null AST root.";
                return result;
            }

            IRVisitor visitor;
            std::string irText = visitor.generate(root);

            result.success = true;
            result.irText = irText;
            result.errorMessage = "";
        }
        catch (const std::exception &e)
        {
            result.success = false;
            result.errorMessage = "IR generation failed: " + std::string(e.what());
        }

        return result;
    }

} // namespace cminus
