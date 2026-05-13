#pragma once

#include <string>

#include "c--/common/Common.h"

namespace cminus {

class Lexer {
public:
    LexResult tokenize(const std::string& source);
};

} // namespace cminus
