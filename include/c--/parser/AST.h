// include/cminus/parser/AST.h
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <ostream>

namespace cminus {

struct ASTNode {
    std::string name;    // 节点类型，例如 FuncDef、ReturnStmt、BinaryExpr
    std::string value;   // 节点值，例如 main、0、+
    int line = -1;
    int column = -1;

    std::vector<std::unique_ptr<ASTNode>> children;

    ASTNode() = default;

    ASTNode(std::string name, std::string value = "", int line = -1, int column = -1)
        : name(std::move(name)), value(std::move(value)), line(line), column(column) {}

    ASTNode* addChild(std::unique_ptr<ASTNode> child) {
        ASTNode* raw = child.get();
        children.push_back(std::move(child));
        return raw;
    }
};

void printAST(const ASTNode* node, std::ostream& os, int depth = 0);

} // namespace cminus
