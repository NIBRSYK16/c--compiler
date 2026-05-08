#pragma once

#include <string>

namespace cminus {

enum class TokenType {
    Keyword,
    Identifier,
    Integer,
    Float,
    Operator,
    Separator,
    EndOfFile,
    Unknown
};

struct Token {
    std::string lexeme;    // 原始文本，例如 int、a、10、+
    TokenType type;        // Token 类型
    std::string grammar;   // Parser 使用的文法符号，例如 int、Ident、IntConst
    std::string attr;      // 属性，例如关键字编号、变量名、数值
    int line = 1;
    int column = 1;
};

inline std::string tokenTypeToString(TokenType type) {
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
