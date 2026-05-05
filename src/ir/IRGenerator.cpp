#include "c--/ir/IRGenerator.h"
#include "third_part/compiler_ir/include/Module.h"
#include "third_part/compiler_ir/include/Function.h"
#include "third_part/compiler_ir/include/BasicBlock.h"
#include "third_part/compiler_ir/include/Constant.h"
#include "third_part/compiler_ir/include/IRbuilder.h"
#include "third_part/compiler_ir/include/IRprinter.h"
#include "third_part/compiler_ir/include/Type.h"
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
                if (!root)
                {
                    throw std::runtime_error("AST root is null");
                }
                reset(); // 重置状态支持多次调用
                module = new Module("main");
                builder = new IRBuilder(nullptr, module);
                visit(root);
                if (!module)
                {
                    throw std::runtime_error("No module created during IR generation");
                }
                std::string irText = module->print();
                return irText;
            }
            void visit(const ASTNode *node)
            {
                if (!node)
                {
                    return;
                }
                if (node->name == "CompUnit")
                {
                    visitCompUnit(node);
                }
                else if (node->name == "FuncDef")
                {
                    visitFuncDef(node);
                }
                else if (node->name == "FuncType")
                {
                    visitFuncType(node);
                }
                else if (node->name == "Block")
                {
                    visitBlock(node);
                }
                else if (node->name == "ReturnStmt")
                {
                    visitReturnStmt(node);
                }
                else if (node->name == "IntLiteral")
                {
                    visitIntLiteral(node);
                }
                else if (node->name == "BinaryExpr")
                {
                    visitBinaryExpr(node);
                }
                else if (node->name == "Decl")
                {
                    visitDecl(node);
                }
                else if (node->name == "AssignStmt")
                {
                    visitAssignStmt(node);
                }
                else if (node->name == "Ident")
                {
                    visitIdent(node);
                }
                else
                {
                    // 对于未知节点类型，递归访问子节点
                    for (auto &child : node->children)
                    {
                        visit(child.get());
                    }
                }
            }
            void visitCompUnit(const ASTNode *node)
            {
                // 创建Module程序根节点
                for (auto &child : node->children)
                {
                    visit(child.get());
                }
            }
            void visitFuncDef(const ASTNode *node)
            {
                // 创建Function
                std::string funcName;
                Type *returnType = nullptr;

                // 从子节点提取函数信息
                for (auto &child : node->children)
                {
                    if (child->name == "FuncType")
                    {
                        // 处理函数返回类型
                        if (!child->children.empty())
                        {
                            std::string typeName = child->children[0]->value;
                            if (typeName == "int")
                            {
                                returnType = Type::get_int32_type(module);
                            }
                            else if (typeName == "void")
                            {
                                returnType = Type::get_void_type(module);
                            }
                            else
                            {
                                throw std::runtime_error("Unsupported return type: " + typeName);
                            }
                        }
                    }
                    else if (child->name == "Ident")
                    {
                        funcName = child->value;
                    }
                }

                if (funcName.empty())
                {
                    throw std::runtime_error("Function name not found");
                }

                if (!returnType)
                {
                    throw std::runtime_error("Function return type not found");
                }

                // 创建函数类型
                std::vector<Type *> paramTypes; // 空参数列表
                FunctionType *funcType = FunctionType::get(returnType, paramTypes);

                // 创建函数
                Function *func = Function::create(funcType, funcName, module);
                module->add_function(func); // 将函数添加到模块
                currentFunc = func;

                // 创建入口基本块
                BasicBlock *entryBB = BasicBlock::create(module, "entry", func);
                func->add_basic_block(entryBB);
                currentBB = entryBB;
                builder->set_insert_point(entryBB);

                // 进入函数作用域
                enterScope();

                // 处理函数体（Block）
                for (auto &child : node->children)
                {
                    if (child->name == "Block")
                    {
                        visitBlock(child.get());
                    }
                }

                // 如果基本块没有终止指令 添加默认返回
                // 对于返回int的函数，需要返回0
                if (!currentBB->get_terminator() && returnType->is_int32_type())
                {
                    ConstantInt *zero = ConstantInt::get(0, module);
                    builder->create_ret(zero);
                }

                // 退出函数作用域
                exitScope();

                // 重置当前函数和基本块
                currentFunc = nullptr;
                currentBB = nullptr;
            }
            void visitFuncType(const ASTNode *node)
            {
                // 确定函数返回类型 直接在visitFuncDef中处理
            }
            void visitBlock(const ASTNode *node)
            {
                // 创建BasicBlock
                // 进入新的作用域
                enterScope();

                // 处理块内的所有语句
                for (auto &child : node->children)
                {
                    visit(child.get());
                }

                // 退出作用域
                exitScope();
            }
            void visitReturnStmt(const ASTNode *node)
            {
                // 创建返回指令
                if (!node->children.empty())
                {
                    visit(node->children[0].get());
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
            }
            void visitDecl(const ASTNode *node)
            {
                // 处理变量声明
            }
            void visitAssignStmt(const ASTNode *node)
            {
                // 处理赋值语句
            }
            Type *getTypeFromName(const std::string &typeName)
            {
                if (typeName == "int")
                {
                    return Type::get_int32_type(module);
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
                for (auto it = symbolTableStack.rbegin(); it != symbolTableStack.rend(); ++it)
                {
                    auto found = it->find(name);
                    if (found != it->end())
                    {
                        return found->second;
                    }
                }
                return nullptr;
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
                Value *varValue = lookupSymbol(varName);

                if (!varValue)
                {
                    throw std::runtime_error("Undefined variable: " + varName);
                }

                // 局部变量需要加载值
                if (varValue->get_type()->is_pointer_type())
                {
                    // 创建load指令
                    Value *loadedValue = builder->create_load(varValue);
                    pushValue(loadedValue);
                }
                else
                {
                    // 直接使用
                    pushValue(varValue);
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
