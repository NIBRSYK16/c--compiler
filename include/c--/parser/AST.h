// include/c--/parser/AST.h
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <ostream>
#include <utility>

namespace cminus {

namespace ASTName {

static const char* CompUnit = "CompUnit";
static const char* ConstDecl = "ConstDecl";
static const char* ConstDef = "ConstDef";
static const char* VarDecl = "VarDecl";
static const char* VarDef = "VarDef";
static const char* FuncDef = "FuncDef";
static const char* Type = "Type";
static const char* ParamList = "ParamList";
static const char* Param = "Param";
static const char* Block = "Block";
static const char* AssignStmt = "AssignStmt";
static const char* ExprStmt = "ExprStmt";
static const char* IfStmt = "IfStmt";
static const char* ReturnStmt = "ReturnStmt";
static const char* LVal = "LVal";
static const char* IntLiteral = "IntLiteral";
static const char* FloatLiteral = "FloatLiteral";
static const char* UnaryExpr = "UnaryExpr";
static const char* BinaryExpr = "BinaryExpr";
static const char* CallExpr = "CallExpr";

} // namespace ASTName

struct ASTNode {
    std::string name;    // 节点类型，例如 FuncDef、ReturnStmt、BinaryExpr
    std::string value;   // 节点值，例如 main、0、+
    int line = -1;
    int column = -1;

    std::vector<std::unique_ptr<ASTNode>> children;

    ASTNode() = default;

    ASTNode(const std::string& nodeName, const std::string& nodeValue = "", int nodeLine = -1, int nodeColumn = -1)
        : name(nodeName), value(nodeValue), line(nodeLine), column(nodeColumn) {}

    ASTNode* addChild(std::unique_ptr<ASTNode> child) {
        ASTNode* raw = child.get();
        // unique_ptr 不能复制，这里用 move 表示把子节点所有权交给 children。
        children.push_back(std::move(child));
        return raw;
    }
};

inline std::unique_ptr<ASTNode> createNode(
    const std::string& name,
    const std::string& value = "",
    int line = -1,
    int column = -1
) {
    return std::unique_ptr<ASTNode>(new ASTNode(name, value, line, column));
}

void printAST(const ASTNode* node, std::ostream& os, int depth = 0);

} // namespace cminus
