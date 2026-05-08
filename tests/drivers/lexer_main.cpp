#include <exception>
#include <cstdlib>
#include <iostream>
#include <string>

#include "c--/lexer/automata.h"
#include "c--/lexer/lexer.h"
#include "tests/drivers/driver_utils.h"

int main(int argc, char** argv) {
    if (argc < 2 || argc > 3) {
        std::cerr << "Usage: " << argv[0] << " <input.sy> [token.tsv]\n";
        return 1;
    }

    try {
        std::string source = test_driver::readTextFile(argv[1]);

        cminus::Lexer lexer;
        cminus::LexResult result = lexer.tokenize(source);

        if (argc == 3) {
            std::ofstream output(argv[2]);
            if (!output) {
                throw std::runtime_error("cannot write token file: " + std::string(argv[2]));
            }
            test_driver::writeTokenTable(result.tokens, output);
        } else {
            test_driver::writeTokenTable(result.tokens, std::cout);
        }

        if (!result.success) {
            if (std::getenv("DIAGNOSTICS_JSON") != NULL) {
                std::cerr << cminus::renderDiagnosticsJsonLines(result.diagnostics);
            }
            std::cerr << result.errorMessage;
            return 1;
        }

        if (std::getenv("LEXER_STATS") != NULL) {
            cminus::AutomataStats stats = cminus::getAutomataStats();
            std::cerr << "lexer automata stats: "
                      << "rules=" << stats.rules
                      << ", alphabet=" << stats.alphabetSize
                      << ", nfa_states=" << stats.nfaStates
                      << ", dfa_states=" << stats.dfaStates
                      << ", minimized_dfa_states=" << stats.minimizedDfaStates
                      << '\n';
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[lexer_test] " << error.what() << '\n';
        return 1;
    }
}
