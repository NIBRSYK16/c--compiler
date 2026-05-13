#pragma once

#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstdlib>

#include "c--/common/Common.h"
#include "c--/parser/AST.h"

namespace test_driver {

inline std::string readTextFile(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file: " + path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

inline void writeTextFile(const std::string& path, const std::string& content) {
    std::ofstream output(path.c_str(), std::ios::out | std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot write file: " + path);
    }

    output << content;
}

inline std::string tokenTypeName(cminus::TokenType type) {
    switch (type) {
        case cminus::TokenType::Keyword:
            return "KW";
        case cminus::TokenType::Identifier:
            return "IDN";
        case cminus::TokenType::Integer:
            return "INT";
        case cminus::TokenType::Float:
            return "FLOAT";
        case cminus::TokenType::Operator:
            return "OP";
        case cminus::TokenType::Separator:
            return "SE";
        case cminus::TokenType::EndOfFile:
            return "EOF";
        default:
            return "UNKNOWN";
    }
}

inline cminus::TokenType tokenTypeFromName(const std::string& name) {
    if (name == "KW") return cminus::TokenType::Keyword;
    if (name == "IDN") return cminus::TokenType::Identifier;
    if (name == "INT") return cminus::TokenType::Integer;
    if (name == "FLOAT") return cminus::TokenType::Float;
    if (name == "OP") return cminus::TokenType::Operator;
    if (name == "SE") return cminus::TokenType::Separator;
    if (name == "EOF") return cminus::TokenType::EndOfFile;
    return cminus::TokenType::Unknown;
}

inline void writeTokenTable(const std::vector<cminus::Token>& tokens, std::ostream& output) {
    output << "# line\tcolumn\ttype\tgrammar\tattr\tlexeme\n";
    for (size_t i = 0; i < tokens.size(); i++) {
        const cminus::Token& token = tokens[i];
        output << token.line << '\t'
               << token.column << '\t'
               << tokenTypeName(token.type) << '\t'
               << token.grammar << '\t'
               << token.attr << '\t'
               << token.lexeme << '\n';
    }
}

inline std::vector<cminus::Token> readTokenTable(const std::string& path) {
    std::ifstream input(path.c_str());
    if (!input) {
        throw std::runtime_error("cannot open token file: " + path);
    }

    std::vector<cminus::Token> tokens;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream row(line);
        std::string lineText;
        std::string columnText;
        std::string typeText;
        cminus::Token token;

        if (!std::getline(row, lineText, '\t') ||
            !std::getline(row, columnText, '\t') ||
            !std::getline(row, typeText, '\t') ||
            !std::getline(row, token.grammar, '\t') ||
            !std::getline(row, token.attr, '\t') ||
            !std::getline(row, token.lexeme)) {
            throw std::runtime_error("bad token table row: " + line);
        }

        token.line = std::atoi(lineText.c_str());
        token.column = std::atoi(columnText.c_str());
        token.type = tokenTypeFromName(typeText);
        tokens.push_back(token);
    }

    return tokens;
}

inline void writeASTText(const cminus::ASTNode* node, std::ostream& output, int depth = 0) {
    if (node == NULL) {
        return;
    }

    for (int i = 0; i < depth; i++) {
        output << "  ";
    }

    output << node->name;
    if (!node->value.empty()) {
        output << " value=" << node->value;
    }
    output << '\n';

    for (size_t i = 0; i < node->children.size(); i++) {
        writeASTText(node->children[i], output, depth + 1);
    }
}

inline int countLeadingSpaces(const std::string& line) {
    int count = 0;
    while (count < (int)line.size() && line[count] == ' ') {
        count++;
    }
    return count;
}

inline cminus::ASTNode* parseASTLine(const std::string& text) {
    std::string name = text;
    std::string value;

    size_t valuePos = text.find(" value=");
    if (valuePos != std::string::npos) {
        name = text.substr(0, valuePos);
        value = text.substr(valuePos + 7);
    }

    return cminus::createNode(name, value);
}

inline std::unique_ptr<cminus::ASTNode> readSimpleAST(const std::string& path) {
    std::ifstream input(path.c_str());
    if (!input) {
        throw std::runtime_error("cannot open AST file: " + path);
    }

    std::unique_ptr<cminus::ASTNode> root;
    std::vector<cminus::ASTNode*> stack;
    std::string line;

    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        int spaces = countLeadingSpaces(line);
        if (spaces % 2 != 0) {
            throw std::runtime_error("bad AST indentation: " + line);
        }

        int depth = spaces / 2;
        std::string text = line.substr(spaces);
        cminus::ASTNode* node = parseASTLine(text);

        if (depth == 0) {
            root.reset(node);
            stack.clear();
            stack.push_back(node);
        } else {
            if (depth > (int)stack.size()) {
                throw std::runtime_error("bad AST parent depth: " + line);
            }

            stack.resize(depth);
            stack.back()->addChild(node);
            stack.push_back(node);
        }
    }

    if (!root) {
        throw std::runtime_error("empty AST file: " + path);
    }

    return root;
}

} // namespace test_driver
