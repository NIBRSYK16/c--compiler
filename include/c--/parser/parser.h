#pragma once

#include <vector>

#include "c--/common/Diagnostic.h"
#include "c--/common/Result.h"
#include "c--/common/Token.h"

namespace cminus {

struct TokenStreamSummary {
    bool success = true;
    int tokenCount = 0;
    int eofCount = 0;
    int maxParenDepth = 0;
    int maxBraceDepth = 0;
    std::vector<Diagnostic> diagnostics;
};

class Parser {
public:
    ParseResult parse(const std::vector<Token>& tokens);
    TokenStreamSummary precheckTokenStream(const std::vector<Token>& tokens) const;
};

} // namespace cminus
