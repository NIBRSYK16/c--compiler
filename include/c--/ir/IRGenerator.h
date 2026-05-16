#pragma once

#include "c--/common/Common.h"
#include "c--/parser/AST.h"

namespace cminus
{

    class IRGenerator
    {
    public:
        IRResult generate(const ASTNode *root);
    };

} // namespace cminus
