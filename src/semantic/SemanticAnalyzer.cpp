#include "c--/semantic/SemanticAnalyzer.h"

#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace cminus {

namespace {

struct SymbolInfo {
    std::string type;
    bool isConst = false;
};

struct FunctionInfo {
    std::string returnType;
    std::vector<std::string> paramTypes;
};

class SemanticVisitor {
public:
    SemanticResult result;

    SemanticVisitor() {
        enterScope();
    }

    void analyze(const ASTNode* root) {
        if (root == NULL) {
            fail("empty AST root");
        }
        visitCompUnit(root);
        result.logs.push_back("semantic analysis finished");
        result.success = true;
    }

private:
    std::vector<std::map<std::string, SymbolInfo> > scopes;
    std::map<std::string, FunctionInfo> functions;
    std::string currentReturnType;

    void fail(const std::string& message) {
        throw std::runtime_error(message);
    }

    std::string where(const ASTNode* node) {
        std::ostringstream out;
        if (node != NULL && node->line > 0) {
            out << " at line " << node->line << ", column " << node->column;
        }
        return out.str();
    }

    void enterScope() {
        scopes.push_back(std::map<std::string, SymbolInfo>());
    }

    void exitScope() {
        if (!scopes.empty()) {
            scopes.pop_back();
        }
    }

    bool currentScopeHas(const std::string& name) {
        if (scopes.empty()) {
            return false;
        }
        return scopes.back().find(name) != scopes.back().end();
    }

    SymbolInfo* findSymbol(const std::string& name) {
        for (int i = (int)scopes.size() - 1; i >= 0; i--) {
            std::map<std::string, SymbolInfo>::iterator found = scopes[i].find(name);
            if (found != scopes[i].end()) {
                return &found->second;
            }
        }
        return NULL;
    }

    void addSymbol(const std::string& name, const std::string& type, bool isConst, const ASTNode* node) {
        if (name.empty()) {
            fail("empty symbol name" + where(node));
        }
        if (currentScopeHas(name)) {
            fail("duplicate definition of '" + name + "'" + where(node));
        }
        if (scopes.size() == 1 && functions.find(name) != functions.end()) {
            fail("global symbol name conflicts with function '" + name + "'" + where(node));
        }
        SymbolInfo info;
        info.type = type;
        info.isConst = isConst;
        scopes.back()[name] = info;

        std::string kind = isConst ? "const " : "var ";
        result.logs.push_back("define " + kind + name + " : " + type);
    }

    void addFunction(const ASTNode* node) {
        std::string name = node->value;
        if (functions.find(name) != functions.end()) {
            fail("duplicate function definition '" + name + "'" + where(node));
        }
        if (currentScopeHas(name)) {
            fail("function name conflicts with global symbol '" + name + "'" + where(node));
        }
        if (node->children.size() < 3) {
            fail("bad function node '" + name + "'" + where(node));
        }

        FunctionInfo info;
        info.returnType = node->children[0]->value;

        const ASTNode* params = node->children[1];
        for (size_t i = 0; i < params->children.size(); i++) {
            const ASTNode* param = params->children[i];
            if (!param->children.empty()) {
                info.paramTypes.push_back(param->children[0]->value);
            }
        }

        functions[name] = info;
        result.logs.push_back("define function " + name + " -> " + info.returnType);
    }

    bool canAssign(const std::string& left, const std::string& right) {
        if (left == right) {
            return true;
        }
        return false;
    }

    void visitCompUnit(const ASTNode* node) {
        if (node->name != ASTName::CompUnit) {
            fail("root node must be CompUnit");
        }

        for (size_t i = 0; i < node->children.size(); i++) {
            const ASTNode* child = node->children[i];
            if (child->name == ASTName::FuncDef) {
                addFunction(child);
            } else if (child->name == ASTName::VarDecl) {
                visitVarDecl(child);
            } else if (child->name == ASTName::ConstDecl) {
                visitConstDecl(child);
            }
        }

        for (size_t i = 0; i < node->children.size(); i++) {
            const ASTNode* child = node->children[i];
            if (child->name == ASTName::FuncDef) {
                visitFuncDef(child);
            }
        }

        if (functions.find("main") == functions.end()) {
            fail("missing main function");
        }
    }

    void visitFuncDef(const ASTNode* node) {
        std::string oldReturnType = currentReturnType;
        currentReturnType = node->children[0]->value;

        enterScope();

        const ASTNode* params = node->children[1];
        for (size_t i = 0; i < params->children.size(); i++) {
            const ASTNode* param = params->children[i];
            std::string type = param->children.empty() ? "" : param->children[0]->value;
            addSymbol(param->value, type, false, param);
        }

        visitBlock(node->children[2], false);

        exitScope();
        currentReturnType = oldReturnType;
    }

    void visitBlock(const ASTNode* node, bool createScope) {
        if (createScope) {
            enterScope();
        }

        for (size_t i = 0; i < node->children.size(); i++) {
            visitStmtOrDecl(node->children[i]);
        }

        if (createScope) {
            exitScope();
        }
    }

    void visitStmtOrDecl(const ASTNode* node) {
        if (node->name == ASTName::VarDecl) {
            visitVarDecl(node);
        } else if (node->name == ASTName::ConstDecl) {
            visitConstDecl(node);
        } else if (node->name == ASTName::Block) {
            visitBlock(node, true);
        } else if (node->name == ASTName::AssignStmt) {
            visitAssignStmt(node);
        } else if (node->name == ASTName::ExprStmt) {
            if (!node->children.empty()) {
                exprType(node->children[0]);
            }
        } else if (node->name == ASTName::IfStmt) {
            visitIfStmt(node);
        } else if (node->name == ASTName::ReturnStmt) {
            visitReturnStmt(node);
        }
    }

    void visitVarDecl(const ASTNode* node) {
        if (node->children.empty()) {
            fail("bad variable declaration" + where(node));
        }
        std::string type = node->children[0]->value;
        for (size_t i = 1; i < node->children.size(); i++) {
            const ASTNode* varDef = node->children[i];
            addSymbol(varDef->value, type, false, varDef);
            if (!varDef->children.empty()) {
                std::string initType = exprType(varDef->children[0]);
                if (!canAssign(type, initType)) {
                    fail("type mismatch in initialization of '" + varDef->value + "'" + where(varDef));
                }
            }
        }
    }

    void visitConstDecl(const ASTNode* node) {
        if (node->children.empty()) {
            fail("bad constant declaration" + where(node));
        }
        std::string type = node->children[0]->value;
        for (size_t i = 1; i < node->children.size(); i++) {
            const ASTNode* constDef = node->children[i];
            addSymbol(constDef->value, type, true, constDef);
            if (constDef->children.empty()) {
                fail("const '" + constDef->value + "' must have initializer" + where(constDef));
            }
            std::string initType = exprType(constDef->children[0]);
            if (!canAssign(type, initType)) {
                fail("type mismatch in initialization of const '" + constDef->value + "'" + where(constDef));
            }
        }
    }

    void visitAssignStmt(const ASTNode* node) {
        const ASTNode* lval = node->children[0];
        SymbolInfo* symbol = findSymbol(lval->value);
        if (symbol == NULL) {
            fail("undefined variable '" + lval->value + "'" + where(lval));
        }
        if (symbol->isConst) {
            fail("cannot assign to const '" + lval->value + "'" + where(lval));
        }

        std::string rightType = exprType(node->children[1]);
        if (!canAssign(symbol->type, rightType)) {
            fail("type mismatch in assignment to '" + lval->value + "'" + where(lval));
        }
    }

    void visitIfStmt(const ASTNode* node) {
        std::string condType = exprType(node->children[0]);
        if (condType == "void") {
            fail("if condition cannot be void" + where(node));
        }
        visitStmtOrDecl(node->children[1]);
        if (node->children.size() > 2) {
            visitStmtOrDecl(node->children[2]);
        }
    }

    void visitReturnStmt(const ASTNode* node) {
        if (currentReturnType == "void") {
            if (!node->children.empty()) {
                fail("void function should not return a value" + where(node));
            }
            return;
        }

        if (node->children.empty()) {
            fail("non-void function must return a value" + where(node));
        }

        std::string type = exprType(node->children[0]);
        if (!canAssign(currentReturnType, type)) {
            fail("return type mismatch, expected " + currentReturnType + " but got " + type + where(node));
        }
    }

    std::string exprType(const ASTNode* node) {
        if (node->name == ASTName::IntLiteral) {
            return "int";
        }
        if (node->name == ASTName::FloatLiteral) {
            return "float";
        }
        if (node->name == ASTName::LVal) {
            SymbolInfo* symbol = findSymbol(node->value);
            if (symbol == NULL) {
                fail("undefined variable '" + node->value + "'" + where(node));
            }
            return symbol->type;
        }
        if (node->name == ASTName::UnaryExpr) {
            std::string type = exprType(node->children[0]);
            if (type == "void") {
                fail("unary operator cannot be applied to void" + where(node));
            }
            return type;
        }
        if (node->name == ASTName::BinaryExpr) {
            return binaryType(node);
        }
        if (node->name == ASTName::CallExpr) {
            return callType(node);
        }
        fail("unknown expression node '" + node->name + "'" + where(node));
        return "";
    }

    std::string binaryType(const ASTNode* node) {
        std::string op = node->value;
        std::string left = exprType(node->children[0]);
        std::string right = exprType(node->children[1]);

        if (left == "void" || right == "void") {
            fail("binary operator cannot use void operand" + where(node));
        }

        if (op == "&&" || op == "||") {
            return "int";
        }

        if (op == "<" || op == ">" || op == "<=" || op == ">=" || op == "==" || op == "!=") {
            if (left != right) {
                fail("comparison operands must have same type" + where(node));
            }
            return "int";
        }

        if (op == "%") {
            if (left != "int" || right != "int") {
                fail("operator % only supports int operands" + where(node));
            }
            return "int";
        }

        if (left != right) {
            fail("binary operands must have same type" + where(node));
        }
        return left;
    }

    std::string callType(const ASTNode* node) {
        std::map<std::string, FunctionInfo>::iterator found = functions.find(node->value);
        if (found == functions.end()) {
            fail("undefined function '" + node->value + "'" + where(node));
        }

        const FunctionInfo& info = found->second;
        if (info.paramTypes.size() != node->children.size()) {
            fail("argument count mismatch for function '" + node->value + "'" + where(node));
        }

        for (size_t i = 0; i < node->children.size(); i++) {
            std::string argType = exprType(node->children[i]);
            if (!canAssign(info.paramTypes[i], argType)) {
                fail("argument type mismatch for function '" + node->value + "'" + where(node));
            }
        }

        return info.returnType;
    }
};

} // namespace

SemanticResult SemanticAnalyzer::analyze(const ASTNode* root) {
    SemanticVisitor visitor;
    try {
        visitor.analyze(root);
        return visitor.result;
    } catch (const std::exception& e) {
        visitor.result.success = false;
        visitor.result.errorMessage = e.what();
        return visitor.result;
    }
}

} // namespace cminus
