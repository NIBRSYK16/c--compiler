#pragma once

#include "c--/common/Common.h"

namespace cminus {

class SemanticAnalyzer {
public:
    SemanticResult analyze(const ASTNode* root);
};

} // namespace cminus
