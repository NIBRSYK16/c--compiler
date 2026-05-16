#pragma once

#include <vector>

#include "c--/common/Common.h"

namespace cminus {

class Parser {
public:
    ParseResult parse(const std::vector<Token>& tokens);
};

} // namespace cminus
