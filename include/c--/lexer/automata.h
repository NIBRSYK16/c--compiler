#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace cminus {

const int EPSILON = -1;

struct TokenRule {
    std::string name;      // KW / OP / SE / IDN / INT / FLOAT
    std::string attr;      // KW/OP/SE use fixed code, IDN/INT/FLOAT use lexeme
    int priority;          // Smaller value means higher priority.
};

struct NFAEdge {
    int to;
    int ch;
};

struct NFAState {
    int id;
    std::vector<NFAEdge> edges;
    int acceptRule;
};

struct NFAFragment {
    int start;
    int accept;
};

struct NFA {
    int start;
    std::vector<NFAState> states;
};

struct DFAState {
    int id;
    std::set<int> nfaStates;
    std::map<int, int> trans;
    int acceptRule;
};

struct DFA {
    int start;
    std::vector<DFAState> states;
    std::vector<int> alphabet;
};

NFA buildLexerNFA(std::vector<TokenRule>& rules);
DFA nfaToDfa(const NFA& nfa, const std::vector<TokenRule>& rules);
DFA minimizeDfa(const DFA& dfa, const std::vector<TokenRule>& rules);

const DFA& getMinimizedDFA();
const std::vector<TokenRule>& getTokenRules();

} // namespace cminus
