#include <exception>
#include <fstream>
#include <iostream>
#include <string>

#include "c--/parser/parser.h"
#include "tests/drivers/driver_utils.h"

int main(int argc, char** argv) {
    if (argc < 2 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " <token.tsv> [ast.txt] [reduce.txt]\n";
        return 1;
    }

    try {
        std::vector<cminus::Token> tokens = test_driver::readTokenTable(argv[1]);

        cminus::Parser parser;
        cminus::ParseResult result = parser.parse(tokens);

        std::ostream* astOutput = &std::cout;
        std::ofstream astFile;
        if (argc >= 3) {
            astFile.open(argv[2]);
            if (!astFile) {
                throw std::runtime_error("cannot write AST file: " + std::string(argv[2]));
            }
            astOutput = &astFile;
        }

        if (result.root) {
            test_driver::writeASTText(result.root.get(), *astOutput);
        }

        if (argc >= 4) {
            std::ofstream reduceOutput(argv[3]);
            if (!reduceOutput) {
                throw std::runtime_error("cannot write reduce file: " + std::string(argv[3]));
            }

            for (size_t i = 0; i < result.reduceLogs.size(); i++) {
                reduceOutput << result.reduceLogs[i] << '\n';
            }
        }

        if (!result.success) {
            std::cerr << result.errorMessage << '\n';
            return 1;
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[parser_test] " << error.what() << '\n';
        return 1;
    }
}
