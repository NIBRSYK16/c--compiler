#pragma once

#include <string>
#include <vector>
#include <memory>

#include "c--/common/Diagnostic.h"
#include "c--/common/Token.h"
#include "c--/parser/AST.h"

namespace cminus {

struct LexResult {
    bool success = false;
    std::vector<Token> tokens;
    std::string errorMessage;
    std::vector<Diagnostic> diagnostics;
};

struct ParseResult {
    bool success = false;
    std::unique_ptr<ASTNode> root;
    std::vector<std::string> reduceLogs;
    std::string errorMessage;
    std::vector<Diagnostic> diagnostics;
};

struct IRResult {
    bool success = false;
    std::string irText;
    std::string errorMessage;
};

} // namespace cminus
