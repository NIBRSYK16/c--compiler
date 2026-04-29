#include "c--/ir/IRGenerator.h"
#include "third_part/compiler_ir/include/Module.h"
#include "third_part/compiler_ir/include/Function.h"
#include "third_part/compiler_ir/include/BasicBlock.h"
#include "third_part/compiler_ir/include/Constant.h"
#include "third_part/compiler_ir/include/IRbuilder.h"
#include "third_part/compiler_ir/include/IRprinter.h"
#include "third_part/compiler_ir/include/Type.h"

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
            IRprinter printer;

        public:
            std::string generate(ASTNode *root)
            {
                if (!root)
                {
                    throw std::runtime_error("AST root is null");
                }
                visit(root);
                if (!module)
                {
                    throw std::runtime_error("No module created during IR generation");
                }
                return printer.print(module);
            }
            void visit(ASTNode *node)
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
                else
                {
                    // 对于未知节点类型，递归访问子节点
                    for (auto &child : node->children)
                    {
                        visit(child.get());
                    }
                }
            }
            void visitCompUnit(ASTNode *node)
            {
                // 创建Module
            }
            void visitFuncDef(ASTNode *node)
            {
                // 创建Function
            }
            void visitFuncType(ASTNode *node)
            {
                // 确定函数返回类型
            }
            void visitBlock(ASTNode *node)
            {
                // 创建BasicBlock
            }
            void visitReturnStmt(ASTNode *node)
            {
                // 创建返回指令
            }
            void visitIntLiteral(ASTNode *node)
            {
                // 创建整数常量
            }
            void visitBinaryExpr(ASTNode *node)
            {
                // 处理二元表达式
            }
            void visitDecl(ASTNode *node)
            {
                // 处理变量声明
            }
            void visitAssignStmt(ASTNode *node)
            {
                // 处理赋值语句
            }
            Type *getTypeFromName(const std::string &typeName)
            {
                if (typeName == "int")
                {
                    return IntegerType::get(32, module);
                }
                throw std::runtime_error("Unsupported type: " + typeName);
            }
            ~IRVisitor()
            {
                delete builder;
            }
        };
    }

    IRResult IRGenerator::generate(const ASTNode *root)
    {
        IRResult result;
        if (root == nullptr)
        {
            result.success = false;
            result.errorMessage = "IRGenerator received null AST root.";
            return result;
        }

        result.success = false;
        result.errorMessage = "IR generation is not implemented yet.";
        return result;
    }

} // namespace cminus
