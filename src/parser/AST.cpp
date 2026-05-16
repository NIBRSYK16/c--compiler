#include "c--/parser/AST.h"

#include <ostream>

namespace cminus {

void printAST(const ASTNode* node, std::ostream& os, int depth) {
    if (node == NULL) {
        return;
    }

    for (int i = 0; i < depth; i++) {
        os << "  ";
    }

    os << node->name;
    if (!node->value.empty()) {
        os << " value=" << node->value;
    }
    os << '\n';

    for (size_t i = 0; i < node->children.size(); i++) {
        printAST(node->children[i], os, depth + 1);
    }
}

} // namespace cminus
