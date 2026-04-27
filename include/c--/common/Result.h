#pragma once

#include <string>
#include <vector>
#include <memory>

#include "cminus/common/Token.h"
#include "cminus/parser/AST.h"

namespace cminus {

struct LexResult {
    bool success = false;
    std::vector<Token> tokens;
    std::string errorMessage;
};

struct ParseResult {
    bool success = false;
    std::unique_ptr<ASTNode> root;
    std::vector<std::string> reduceLogs;
    std::string errorMessage;
};

struct IRResult {
    bool success = false;
    std::string irText;
    std::string errorMessage;
};

} // namespace cminus
