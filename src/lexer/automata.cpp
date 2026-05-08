#include "c--/lexer/automata.h"

#include <algorithm>
#include <map>
#include <queue>

namespace cminus {

namespace {

class NFABuilder {
public:
    NFA nfa;

    NFABuilder() {
        nfa.start = newState();
    }

    int newState() {
        NFAState state;
        state.id = (int)nfa.states.size();
        state.acceptRule = -1;
        nfa.states.push_back(state);
        return state.id;
    }

    void addEdge(int from, int to, int ch) {
        NFAEdge edge;
        edge.to = to;
        edge.ch = ch;
        nfa.states[from].edges.push_back(edge);
    }

    NFAFragment charSet(const std::vector<int>& chars) {
        int start = newState();
        int accept = newState();

        for (size_t i = 0; i < chars.size(); i++) {
            addEdge(start, accept, chars[i]);
        }

        NFAFragment result;
        result.start = start;
        result.accept = accept;
        return result;
    }

    NFAFragment concat(NFAFragment left, NFAFragment right) {
        addEdge(left.accept, right.start, EPSILON);

        NFAFragment result;
        result.start = left.start;
        result.accept = right.accept;
        return result;
    }

    NFAFragment alternate(NFAFragment left, NFAFragment right) {
        int start = newState();
        int accept = newState();

        addEdge(start, left.start, EPSILON);
        addEdge(start, right.start, EPSILON);
        addEdge(left.accept, accept, EPSILON);
        addEdge(right.accept, accept, EPSILON);

        NFAFragment result;
        result.start = start;
        result.accept = accept;
        return result;
    }

    NFAFragment star(NFAFragment fragment) {
        int start = newState();
        int accept = newState();

        addEdge(start, fragment.start, EPSILON);
        addEdge(start, accept, EPSILON);
        addEdge(fragment.accept, fragment.start, EPSILON);
        addEdge(fragment.accept, accept, EPSILON);

        NFAFragment result;
        result.start = start;
        result.accept = accept;
        return result;
    }

    NFAFragment plus(NFAFragment fragment) {
        int start = newState();
        int accept = newState();

        addEdge(start, fragment.start, EPSILON);
        addEdge(fragment.accept, fragment.start, EPSILON);
        addEdge(fragment.accept, accept, EPSILON);

        NFAFragment result;
        result.start = start;
        result.accept = accept;
        return result;
    }

    NFAFragment literal(const std::string& word, bool ignoreCase) {
        NFAFragment result;
        result.start = -1;
        result.accept = -1;

        for (size_t i = 0; i < word.size(); i++) {
            std::vector<int> chars;
            unsigned char ch = (unsigned char)word[i];

            if (ignoreCase && ch >= 'a' && ch <= 'z') {
                chars.push_back(ch);
                chars.push_back(ch - 'a' + 'A');
            } else if (ignoreCase && ch >= 'A' && ch <= 'Z') {
                chars.push_back(ch);
                chars.push_back(ch - 'A' + 'a');
            } else {
                chars.push_back(ch);
            }

            NFAFragment oneChar = charSet(chars);
            if (result.start == -1) {
                result = oneChar;
            } else {
                result = concat(result, oneChar);
            }
        }

        return result;
    }

    void addRule(NFAFragment fragment, int ruleIndex) {
        nfa.states[fragment.accept].acceptRule = ruleIndex;
        addEdge(nfa.start, fragment.start, EPSILON);
    }
};

struct WordCode {
    const char* word;
    const char* code;
};

std::vector<int> makeRange(char first, char last) {
    std::vector<int> result;
    for (int ch = (unsigned char)first; ch <= (unsigned char)last; ch++) {
        result.push_back(ch);
    }
    return result;
}

void appendVector(std::vector<int>& target, const std::vector<int>& source) {
    for (size_t i = 0; i < source.size(); i++) {
        target.push_back(source[i]);
    }
}

std::vector<int> uniqueChars(std::vector<int> chars) {
    std::sort(chars.begin(), chars.end());
    chars.erase(std::unique(chars.begin(), chars.end()), chars.end());
    return chars;
}

int addRuleInfo(std::vector<TokenRule>& rules, const char* name, const char* attr, int priority) {
    TokenRule rule;
    rule.name = name;
    rule.attr = attr;
    rule.priority = priority;
    rules.push_back(rule);
    return (int)rules.size() - 1;
}

std::set<int> epsilonClosure(const NFA& nfa, const std::set<int>& input) {
    std::set<int> result = input;
    std::queue<int> queue;

    for (std::set<int>::const_iterator it = input.begin(); it != input.end(); ++it) {
        queue.push(*it);
    }

    while (!queue.empty()) {
        int state = queue.front();
        queue.pop();

        const std::vector<NFAEdge>& edges = nfa.states[state].edges;
        for (size_t i = 0; i < edges.size(); i++) {
            if (edges[i].ch == EPSILON && result.find(edges[i].to) == result.end()) {
                result.insert(edges[i].to);
                queue.push(edges[i].to);
            }
        }
    }

    return result;
}

std::set<int> moveSet(const NFA& nfa, const std::set<int>& input, int ch) {
    std::set<int> result;

    for (std::set<int>::const_iterator it = input.begin(); it != input.end(); ++it) {
        const std::vector<NFAEdge>& edges = nfa.states[*it].edges;
        for (size_t i = 0; i < edges.size(); i++) {
            if (edges[i].ch == ch) {
                result.insert(edges[i].to);
            }
        }
    }

    return result;
}

int chooseAcceptRule(const NFA& nfa, const std::set<int>& states, const std::vector<TokenRule>& rules) {
    int best = -1;

    for (std::set<int>::const_iterator it = states.begin(); it != states.end(); ++it) {
        int ruleIndex = nfa.states[*it].acceptRule;
        if (ruleIndex >= 0) {
            if (best < 0 || rules[ruleIndex].priority < rules[best].priority) {
                best = ruleIndex;
            }
        }
    }

    return best;
}

bool built = false;
std::vector<TokenRule> globalRules;
DFA globalMinDFA;
NFA globalNFA;
DFA globalDFA;

void ensureBuilt() {
    if (!built) {
        globalNFA = buildLexerNFA(globalRules);
        globalDFA = nfaToDfa(globalNFA, globalRules);
        globalMinDFA = minimizeDfa(globalDFA, globalRules);
        built = true;
    }
}

} // namespace

NFA buildLexerNFA(std::vector<TokenRule>& rules) {
    rules.clear();
    NFABuilder builder;

    int priority = 1;

    WordCode keywords[] = {
        {"int", "1"},
        {"void", "2"},
        {"return", "3"},
        {"const", "4"},
        {"main", "5"},
        {"float", "6"},
        {"if", "7"},
        {"else", "8"},
        {NULL, NULL}
    };

    for (int i = 0; keywords[i].word != NULL; i++) {
        int ruleIndex = addRuleInfo(rules, "KW", keywords[i].code, priority++);
        builder.addRule(builder.literal(keywords[i].word, true), ruleIndex);
    }

    WordCode operators[] = {
        {"+", "6"},
        {"-", "7"},
        {"*", "8"},
        {"/", "9"},
        {"%", "10"},
        {"=", "11"},
        {">", "12"},
        {"<", "13"},
        {"==", "14"},
        {"<=", "15"},
        {">=", "16"},
        {"!=", "17"},
        {"&&", "18"},
        {"||", "19"},
        {"!", "!"},
        {NULL, NULL}
    };

    for (int i = 0; operators[i].word != NULL; i++) {
        int ruleIndex = addRuleInfo(rules, "OP", operators[i].code, priority++);
        builder.addRule(builder.literal(operators[i].word, false), ruleIndex);
    }

    WordCode separators[] = {
        {"(", "20"},
        {")", "21"},
        {"{", "22"},
        {"}", "23"},
        {";", "24"},
        {",", "25"},
        {NULL, NULL}
    };

    for (int i = 0; separators[i].word != NULL; i++) {
        int ruleIndex = addRuleInfo(rules, "SE", separators[i].code, priority++);
        builder.addRule(builder.literal(separators[i].word, false), ruleIndex);
    }

    std::vector<int> digit = makeRange('0', '9');

    std::vector<int> letter = makeRange('a', 'z');
    appendVector(letter, makeRange('A', 'Z'));

    std::vector<int> letterOrUnder = letter;
    letterOrUnder.push_back('_');
    letterOrUnder = uniqueChars(letterOrUnder);

    std::vector<int> letterDigitUnder = letterOrUnder;
    appendVector(letterDigitUnder, digit);
    letterDigitUnder = uniqueChars(letterDigitUnder);

    // FLOAT = digit+ '.' digit+
    int floatRule = addRuleInfo(rules, "FLOAT", "", 100);
    NFAFragment floatPrefix = builder.concat(builder.plus(builder.charSet(digit)), builder.literal(".", false));
    NFAFragment floatNumber = builder.concat(floatPrefix, builder.plus(builder.charSet(digit)));
    builder.addRule(floatNumber, floatRule);

    int intRule = addRuleInfo(rules, "INT", "", 101);
    builder.addRule(builder.plus(builder.charSet(digit)), intRule);

    int idRule = addRuleInfo(rules, "IDN", "", 200);
    NFAFragment idFirst = builder.charSet(letterOrUnder);
    NFAFragment idRest = builder.star(builder.charSet(letterDigitUnder));
    builder.addRule(builder.concat(idFirst, idRest), idRule);

    return builder.nfa;
}

DFA nfaToDfa(const NFA& nfa, const std::vector<TokenRule>& rules) {
    DFA dfa;
    dfa.start = 0;

    std::set<int> alphabetSet;
    for (size_t i = 0; i < nfa.states.size(); i++) {
        for (size_t j = 0; j < nfa.states[i].edges.size(); j++) {
            int ch = nfa.states[i].edges[j].ch;
            if (ch != EPSILON) {
                alphabetSet.insert(ch);
            }
        }
    }

    for (std::set<int>::iterator it = alphabetSet.begin(); it != alphabetSet.end(); ++it) {
        dfa.alphabet.push_back(*it);
    }

    std::map< std::set<int>, int > idOf;
    std::queue< std::set<int> > queue;

    std::set<int> startSet;
    startSet.insert(nfa.start);
    startSet = epsilonClosure(nfa, startSet);

    DFAState start;
    start.id = 0;
    start.nfaStates = startSet;
    start.acceptRule = chooseAcceptRule(nfa, startSet, rules);
    dfa.states.push_back(start);

    idOf[startSet] = 0;
    queue.push(startSet);

    while (!queue.empty()) {
        std::set<int> currentSet = queue.front();
        queue.pop();

        int currentId = idOf[currentSet];

        for (size_t i = 0; i < dfa.alphabet.size(); i++) {
            int ch = dfa.alphabet[i];
            std::set<int> moved = moveSet(nfa, currentSet, ch);
            if (moved.empty()) {
                continue;
            }

            std::set<int> nextSet = epsilonClosure(nfa, moved);
            if (idOf.find(nextSet) == idOf.end()) {
                DFAState nextState;
                nextState.id = (int)dfa.states.size();
                nextState.nfaStates = nextSet;
                nextState.acceptRule = chooseAcceptRule(nfa, nextSet, rules);

                idOf[nextSet] = nextState.id;
                dfa.states.push_back(nextState);
                queue.push(nextSet);
            }

            dfa.states[currentId].trans[ch] = idOf[nextSet];
        }
    }

    return dfa;
}

DFA minimizeDfa(const DFA& dfa, const std::vector<TokenRule>& rules) {
    (void)rules;

    int stateCount = (int)dfa.states.size();
    std::vector<int> group(stateCount, -1);
    std::map<int, int> acceptGroup;
    int groupCount = 0;

    for (int i = 0; i < stateCount; i++) {
        int ruleIndex = dfa.states[i].acceptRule;

        if (acceptGroup.find(ruleIndex) == acceptGroup.end()) {
            acceptGroup[ruleIndex] = groupCount++;
        }

        group[i] = acceptGroup[ruleIndex];
    }

    bool changed = true;
    while (changed) {
        changed = false;

        std::map< std::vector<int>, int > signatureToGroup;
        std::vector<int> newGroup(stateCount, -1);
        int newGroupCount = 0;

        for (int i = 0; i < stateCount; i++) {
            std::vector<int> signature;
            signature.push_back(dfa.states[i].acceptRule);

            for (size_t j = 0; j < dfa.alphabet.size(); j++) {
                int ch = dfa.alphabet[j];
                std::map<int, int>::const_iterator trans = dfa.states[i].trans.find(ch);
                if (trans == dfa.states[i].trans.end()) {
                    signature.push_back(-1);
                } else {
                    signature.push_back(group[trans->second]);
                }
            }

            if (signatureToGroup.find(signature) == signatureToGroup.end()) {
                signatureToGroup[signature] = newGroupCount++;
            }

            newGroup[i] = signatureToGroup[signature];
        }

        if (newGroup != group) {
            changed = true;
            group = newGroup;
            groupCount = newGroupCount;
        }
    }

    int maxGroup = -1;
    for (int i = 0; i < stateCount; i++) {
        if (group[i] > maxGroup) {
            maxGroup = group[i];
        }
    }

    int minimizedStateCount = maxGroup + 1;
    std::vector<int> representative(minimizedStateCount, -1);

    for (int i = 0; i < stateCount; i++) {
        if (representative[group[i]] == -1) {
            representative[group[i]] = i;
        }
    }

    DFA minimized;
    minimized.alphabet = dfa.alphabet;
    minimized.states.resize(minimizedStateCount);

    for (int groupId = 0; groupId < minimizedStateCount; groupId++) {
        int oldState = representative[groupId];
        minimized.states[groupId].id = groupId;
        minimized.states[groupId].nfaStates = dfa.states[oldState].nfaStates;
        minimized.states[groupId].acceptRule = dfa.states[oldState].acceptRule;
    }

    for (int groupId = 0; groupId < minimizedStateCount; groupId++) {
        int oldState = representative[groupId];
        for (std::map<int, int>::const_iterator it = dfa.states[oldState].trans.begin();
             it != dfa.states[oldState].trans.end();
             ++it) {
            minimized.states[groupId].trans[it->first] = group[it->second];
        }
    }

    minimized.start = group[dfa.start];
    return minimized;
}

const DFA& getMinimizedDFA() {
    ensureBuilt();
    return globalMinDFA;
}

const std::vector<TokenRule>& getTokenRules() {
    ensureBuilt();
    return globalRules;
}

AutomataStats getAutomataStats() {
    ensureBuilt();

    AutomataStats stats;
    stats.nfaStates = (int)globalNFA.states.size();
    stats.dfaStates = (int)globalDFA.states.size();
    stats.minimizedDfaStates = (int)globalMinDFA.states.size();
    stats.rules = (int)globalRules.size();
    stats.alphabetSize = (int)globalMinDFA.alphabet.size();
    return stats;
}

} // namespace cminus
