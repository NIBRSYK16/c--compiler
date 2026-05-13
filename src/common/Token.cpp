#include "c--/common/Common.h"

namespace cminus {

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::Keyword:
            return "KW";
        case TokenType::Identifier:
            return "IDN";
        case TokenType::Integer:
            return "INT";
        case TokenType::Float:
            return "FLOAT";
        case TokenType::Operator:
            return "OP";
        case TokenType::Separator:
            return "SE";
        case TokenType::EndOfFile:
            return "EOF";
        case TokenType::Unknown:
            return "UNKNOWN";
    }
    return "UNKNOWN";
}

} // namespace cminus
