#include <exception>
#include <iostream>
#include <string>

#include "c--/ir/IRGenerator.h"
#include "tests/drivers/driver_utils.h"

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <ast.txt> [output.ll]\n";
        return 1;
    }

    try {
        std::unique_ptr<cminus::ASTNode> root = test_driver::readSimpleAST(argv[1]);

        cminus::IRGenerator generator;
        cminus::IRResult result = generator.generate(root.get());

        if (argc == 3) {
            test_driver::writeTextFile(argv[2], result.irText);
        } else {
            std::cout << result.irText;
        }

        if (!result.success) {
            std::cerr << result.errorMessage << '\n';
            return 1;
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[ir_test] " << error.what() << '\n';
        return 1;
    }
}
