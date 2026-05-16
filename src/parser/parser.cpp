#include "c--/parser/parser.h"

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace cminus {

namespace {

const std::string EPSILON_SYMBOL = "$";
const std::string EOF_SYMBOL = "EOF";
const std::string START_SYMBOL = "Program'";

struct Production {
    std::string lhs;
    std::vector<std::string> rhs;
    std::string tag;
};

struct Item {
    int production = 0;
    int dot = 0;

    bool operator<(const Item& other) const {
        if (production != other.production) {
            return production < other.production;
        }
        return dot < other.dot;
    }

    bool operator==(const Item& other) const {
        return production == other.production && dot == other.dot;
    }
};

enum class ActionType {
    Shift,
    Reduce,
    Accept
};

struct Action {
    ActionType type = ActionType::Shift;
    int value = -1;
};

struct SemanticValue {
    std::string symbol;
    std::string text;
    int line = -1;
    int column = -1;
    ASTNode* ast = NULL;
    std::vector<ASTNode* > list;
};

ASTNode* makeNode(const std::string& name, const std::string& value = "", int line = -1, int column = -1) {
    return createNode(name, value, line, column);
}

std::string productionToString(const Production& production) {
    std::ostringstream out;
    out << production.lhs << " ->";
    if (production.rhs.empty()) {
        out << " " << EPSILON_SYMBOL;
    } else {
        for (size_t i = 0; i < production.rhs.size(); i++) {
            out << " " << production.rhs[i];
        }
    }
    return out.str();
}

class Grammar {
public:
    std::vector<Production> productions;
    std::map<std::string, std::vector<int> > byLhs;
    std::set<std::string> nonterminals;
    std::set<std::string> terminals;
    std::map<std::string, std::set<std::string> > first;
    std::map<std::string, std::set<std::string> > follow;

    Grammar() {
        buildProductions();
        buildSymbols();
        buildFirst();
        buildFollow();
    }

private:
    void add(const std::string& lhs, const std::vector<std::string>& rhs, const std::string& tag) {
        Production production;
        production.lhs = lhs;
        production.rhs = rhs;
        production.tag = tag;
        byLhs[lhs].push_back((int)productions.size());
        productions.push_back(production);
        nonterminals.insert(lhs);
    }

    void buildProductions() {
        add(START_SYMBOL, {"Program"}, "start");
        add("Program", {"CompUnit"}, "program");
        add("CompUnit", {"TopList"}, "comp_unit");
        add("TopList", {"TopItem", "TopList"}, "top_list_cons");
        add("TopList", {}, "top_list_empty");
        add("TopItem", {"ConstDecl"}, "pass");
        add("TopItem", {"Type", "Name", "TopAfterTypeName"}, "top_type_item");
        add("TopItem", {"void", "Name", "(", "FuncFParamsOpt", ")", "Block"}, "top_void_func");

        add("Decl", {"ConstDecl"}, "pass");
        add("Decl", {"VarDecl"}, "pass");
        add("ConstDecl", {"const", "Type", "ConstDef", "ConstDefList", ";"}, "const_decl");
        add("ConstDefList", {",", "ConstDef", "ConstDefList"}, "list_const_def_cons");
        add("ConstDefList", {}, "list_empty");
        add("ConstDef", {"Name", "=", "ConstInitVal"}, "const_def");
        add("ConstInitVal", {"ConstExp"}, "pass");
        add("VarDecl", {"Type", "VarDef", "VarDefList", ";"}, "var_decl");
        add("VarDefList", {",", "VarDef", "VarDefList"}, "list_var_def_cons");
        add("VarDefList", {}, "list_empty");
        add("VarDef", {"Name"}, "var_def_no_init");
        add("VarDef", {"Name", "=", "InitVal"}, "var_def_init");
        add("InitVal", {"Exp"}, "pass");

        add("TopAfterTypeName", {"(", "FuncFParamsOpt", ")", "Block"}, "top_func_tail");
        add("TopAfterTypeName", {"TopVarDefRest", "VarDefList", ";"}, "top_var_tail");
        add("TopVarDefRest", {}, "var_def_rest_no_init");
        add("TopVarDefRest", {"=", "InitVal"}, "var_def_rest_init");
        add("FuncDef", {"FuncType", "Name", "(", "FuncFParamsOpt", ")", "Block"}, "func_def");
        add("FuncType", {"void"}, "type");
        add("FuncType", {"int"}, "type");
        add("Type", {"int"}, "type");
        add("Type", {"float"}, "type");
        add("FuncFParamsOpt", {"FuncFParams"}, "pass");
        add("FuncFParamsOpt", {}, "param_list_empty");
        add("FuncFParams", {"Param", "ParamListTail"}, "param_list");
        add("ParamListTail", {",", "Param", "ParamListTail"}, "list_param_cons");
        add("ParamListTail", {}, "list_empty");
        add("Param", {"Type", "Name"}, "param");

        add("Block", {"{", "BlockItems", "}"}, "block");
        add("BlockItems", {"BlockItem", "BlockItems"}, "block_items_cons");
        add("BlockItems", {}, "list_empty");
        add("BlockItem", {"Decl"}, "pass");
        add("BlockItem", {"Stmt"}, "pass");

        add("Stmt", {"LVal", "=", "Exp", ";"}, "assign_stmt");
        add("Stmt", {"ExpOpt", ";"}, "expr_stmt");
        add("Stmt", {"Block"}, "pass");
        add("Stmt", {"if", "(", "Cond", ")", "Stmt", "ElseOpt"}, "if_stmt");
        add("ElseOpt", {"else", "Stmt"}, "else_stmt");
        add("ElseOpt", {}, "else_empty");
        add("Stmt", {"return", "ReturnExpOpt", ";"}, "return_stmt");
        add("ExpOpt", {"Exp"}, "pass");
        add("ExpOpt", {}, "empty");
        add("ReturnExpOpt", {"Exp"}, "pass");
        add("ReturnExpOpt", {}, "empty");

        add("Exp", {"LOrExp"}, "pass");
        add("Cond", {"LOrExp"}, "pass");
        add("LVal", {"Name"}, "lval");
        add("PrimaryExp", {"(", "Exp", ")"}, "paren");
        add("PrimaryExp", {"LVal"}, "pass");
        add("PrimaryExp", {"Number"}, "pass");
        add("Number", {"IntConst"}, "int_literal");
        add("Number", {"floatConst"}, "float_literal");

        add("UnaryExp", {"PrimaryExp"}, "pass");
        add("UnaryExp", {"Name", "(", "FuncRParamsOpt", ")"}, "call_expr");
        add("UnaryExp", {"UnaryOp", "UnaryExp"}, "unary_expr");
        add("UnaryOp", {"+"}, "op");
        add("UnaryOp", {"-"}, "op");
        add("UnaryOp", {"!"}, "op");
        add("FuncRParamsOpt", {"FuncRParams"}, "pass");
        add("FuncRParamsOpt", {}, "list_empty");
        add("FuncRParams", {"Exp", "FuncRParamsTail"}, "args");
        add("FuncRParamsTail", {",", "Exp", "FuncRParamsTail"}, "list_exp_cons");
        add("FuncRParamsTail", {}, "list_empty");

        add("MulExp", {"UnaryExp"}, "pass");
        add("MulExp", {"MulExp", "MulOp", "UnaryExp"}, "binary_expr");
        add("MulOp", {"*"}, "op");
        add("MulOp", {"/"}, "op");
        add("MulOp", {"%"}, "op");
        add("AddExp", {"MulExp"}, "pass");
        add("AddExp", {"AddExp", "AddOp", "MulExp"}, "binary_expr");
        add("AddOp", {"+"}, "op");
        add("AddOp", {"-"}, "op");
        add("RelExp", {"AddExp"}, "pass");
        add("RelExp", {"RelExp", "RelOp", "AddExp"}, "binary_expr");
        add("RelOp", {"<"}, "op");
        add("RelOp", {">"}, "op");
        add("RelOp", {"<="}, "op");
        add("RelOp", {">="}, "op");
        add("EqExp", {"RelExp"}, "pass");
        add("EqExp", {"EqExp", "EqOp", "RelExp"}, "binary_expr");
        add("EqOp", {"=="}, "op");
        add("EqOp", {"!="}, "op");
        add("LAndExp", {"EqExp"}, "pass");
        add("LAndExp", {"LAndExp", "&&", "EqExp"}, "binary_expr_direct");
        add("LOrExp", {"LAndExp"}, "pass");
        add("LOrExp", {"LOrExp", "||", "LAndExp"}, "binary_expr_direct");
        add("ConstExp", {"AddExp"}, "pass");

        add("Name", {"Ident"}, "name");
        add("Name", {"main"}, "name");
    }

    void buildSymbols() {
        for (size_t i = 0; i < productions.size(); i++) {
            for (size_t j = 0; j < productions[i].rhs.size(); j++) {
                const std::string& symbol = productions[i].rhs[j];
                if (nonterminals.find(symbol) == nonterminals.end()) {
                    terminals.insert(symbol);
                }
            }
        }
        terminals.insert(EOF_SYMBOL);
    }

    bool addFirst(const std::string& symbol, const std::string& terminal) {
        size_t oldSize = first[symbol].size();
        first[symbol].insert(terminal);
        return first[symbol].size() != oldSize;
    }

    bool rhsNullable(const std::vector<std::string>& rhs) const {
        if (rhs.empty()) {
            return true;
        }
        for (size_t i = 0; i < rhs.size(); i++) {
            std::map<std::string, std::set<std::string> >::const_iterator it = first.find(rhs[i]);
            if (it == first.end() || it->second.find(EPSILON_SYMBOL) == it->second.end()) {
                return false;
            }
        }
        return true;
    }

    std::set<std::string> firstOfSequence(const std::vector<std::string>& symbols, size_t begin) const {
        std::set<std::string> result;
        if (begin >= symbols.size()) {
            result.insert(EPSILON_SYMBOL);
            return result;
        }

        bool allNullable = true;
        for (size_t i = begin; i < symbols.size(); i++) {
            std::map<std::string, std::set<std::string> >::const_iterator it = first.find(symbols[i]);
            if (it == first.end()) {
                allNullable = false;
                break;
            }

            for (std::set<std::string>::const_iterator term = it->second.begin(); term != it->second.end(); ++term) {
                if (*term != EPSILON_SYMBOL) {
                    result.insert(*term);
                }
            }

            if (it->second.find(EPSILON_SYMBOL) == it->second.end()) {
                allNullable = false;
                break;
            }
        }

        if (allNullable) {
            result.insert(EPSILON_SYMBOL);
        }
        return result;
    }

    void buildFirst() {
        for (std::set<std::string>::const_iterator it = terminals.begin(); it != terminals.end(); ++it) {
            first[*it].insert(*it);
        }
        for (std::set<std::string>::const_iterator it = nonterminals.begin(); it != nonterminals.end(); ++it) {
            first[*it];
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t i = 0; i < productions.size(); i++) {
                const Production& production = productions[i];
                if (production.rhs.empty()) {
                    changed = addFirst(production.lhs, EPSILON_SYMBOL) || changed;
                    continue;
                }

                for (size_t j = 0; j < production.rhs.size(); j++) {
                    const std::set<std::string>& rhsFirst = first[production.rhs[j]];
                    for (std::set<std::string>::const_iterator term = rhsFirst.begin(); term != rhsFirst.end(); ++term) {
                        if (*term != EPSILON_SYMBOL) {
                            changed = addFirst(production.lhs, *term) || changed;
                        }
                    }
                    if (rhsFirst.find(EPSILON_SYMBOL) == rhsFirst.end()) {
                        break;
                    }
                    if (j + 1 == production.rhs.size()) {
                        changed = addFirst(production.lhs, EPSILON_SYMBOL) || changed;
                    }
                }

                if (rhsNullable(production.rhs)) {
                    changed = addFirst(production.lhs, EPSILON_SYMBOL) || changed;
                }
            }
        }
    }

    void buildFollow() {
        for (std::set<std::string>::const_iterator it = nonterminals.begin(); it != nonterminals.end(); ++it) {
            follow[*it];
        }
        follow["Program"].insert(EOF_SYMBOL);

        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t i = 0; i < productions.size(); i++) {
                const Production& production = productions[i];
                for (size_t j = 0; j < production.rhs.size(); j++) {
                    const std::string& symbol = production.rhs[j];
                    if (nonterminals.find(symbol) == nonterminals.end()) {
                        continue;
                    }

                    std::set<std::string> betaFirst = firstOfSequence(production.rhs, j + 1);
                    size_t oldSize = follow[symbol].size();
                    for (std::set<std::string>::const_iterator term = betaFirst.begin(); term != betaFirst.end(); ++term) {
                        if (*term != EPSILON_SYMBOL) {
                            follow[symbol].insert(*term);
                        }
                    }
                    if (betaFirst.find(EPSILON_SYMBOL) != betaFirst.end()) {
                        follow[symbol].insert(follow[production.lhs].begin(), follow[production.lhs].end());
                    }
                    if (follow[symbol].size() != oldSize) {
                        changed = true;
                    }
                }
            }
        }
    }
};

class SLRTable {
public:
    const Grammar& grammar;
    std::vector<std::set<Item> > states;
    std::map<std::pair<int, std::string>, int> transitions;
    std::map<std::pair<int, std::string>, Action> actions;
    std::map<std::pair<int, std::string>, int> gotos;

    explicit SLRTable(const Grammar& g) : grammar(g) {
        buildStates();
        buildTables();
    }

private:
    std::set<Item> closure(const std::set<Item>& input) const {
        std::set<Item> result = input;
        bool changed = true;
        while (changed) {
            changed = false;
            std::vector<Item> current(result.begin(), result.end());
            for (size_t i = 0; i < current.size(); i++) {
                const Production& production = grammar.productions[current[i].production];
                if (current[i].dot >= (int)production.rhs.size()) {
                    continue;
                }
                const std::string& next = production.rhs[current[i].dot];
                if (grammar.nonterminals.find(next) == grammar.nonterminals.end()) {
                    continue;
                }
                std::map<std::string, std::vector<int> >::const_iterator productions = grammar.byLhs.find(next);
                if (productions == grammar.byLhs.end()) {
                    continue;
                }
                for (size_t j = 0; j < productions->second.size(); j++) {
                    Item item;
                    item.production = productions->second[j];
                    item.dot = 0;
                    if (result.insert(item).second) {
                        changed = true;
                    }
                }
            }
        }
        return result;
    }

    std::set<Item> goTo(const std::set<Item>& state, const std::string& symbol) const {
        std::set<Item> moved;
        for (std::set<Item>::const_iterator it = state.begin(); it != state.end(); ++it) {
            const Production& production = grammar.productions[it->production];
            if (it->dot < (int)production.rhs.size() && production.rhs[it->dot] == symbol) {
                Item next;
                next.production = it->production;
                next.dot = it->dot + 1;
                moved.insert(next);
            }
        }
        return closure(moved);
    }

    int stateId(const std::set<Item>& state) const {
        for (size_t i = 0; i < states.size(); i++) {
            if (states[i] == state) {
                return (int)i;
            }
        }
        return -1;
    }

    void buildStates() {
        std::set<Item> startItems;
        Item start;
        start.production = 0;
        start.dot = 0;
        startItems.insert(start);

        states.push_back(closure(startItems));
        std::queue<int> queue;
        queue.push(0);

        while (!queue.empty()) {
            int current = queue.front();
            queue.pop();

            std::set<std::string> symbols;
            for (std::set<Item>::const_iterator it = states[current].begin(); it != states[current].end(); ++it) {
                const Production& production = grammar.productions[it->production];
                if (it->dot < (int)production.rhs.size()) {
                    symbols.insert(production.rhs[it->dot]);
                }
            }

            for (std::set<std::string>::const_iterator symbol = symbols.begin(); symbol != symbols.end(); ++symbol) {
                std::set<Item> nextState = goTo(states[current], *symbol);
                if (nextState.empty()) {
                    continue;
                }

                int id = stateId(nextState);
                if (id < 0) {
                    id = (int)states.size();
                    states.push_back(nextState);
                    queue.push(id);
                }
                transitions[std::make_pair(current, *symbol)] = id;
            }
        }
    }

    void setAction(int state, const std::string& terminal, const Action& action) {
        std::pair<int, std::string> key = std::make_pair(state, terminal);
        std::map<std::pair<int, std::string>, Action>::iterator old = actions.find(key);
        if (old == actions.end()) {
            actions[key] = action;
            return;
        }

        if (old->second.type == ActionType::Shift && action.type == ActionType::Reduce && terminal == "else") {
            return;
        }
        if (old->second.type == ActionType::Reduce && action.type == ActionType::Shift && terminal == "else") {
            old->second = action;
            return;
        }

        if (old->second.type == action.type && old->second.value == action.value) {
            return;
        }

        std::ostringstream error;
        error << "SLR conflict at state " << state << " on terminal " << terminal;
        throw std::runtime_error(error.str());
    }

    void buildTables() {
        for (size_t i = 0; i < states.size(); i++) {
            for (std::set<Item>::const_iterator item = states[i].begin(); item != states[i].end(); ++item) {
                const Production& production = grammar.productions[item->production];

                if (item->dot < (int)production.rhs.size()) {
                    const std::string& symbol = production.rhs[item->dot];
                    std::map<std::pair<int, std::string>, int>::const_iterator trans =
                        transitions.find(std::make_pair((int)i, symbol));
                    if (trans == transitions.end()) {
                        continue;
                    }

                    if (grammar.terminals.find(symbol) != grammar.terminals.end()) {
                        Action action;
                        action.type = ActionType::Shift;
                        action.value = trans->second;
                        setAction((int)i, symbol, action);
                    } else {
                        gotos[std::make_pair((int)i, symbol)] = trans->second;
                    }
                    continue;
                }

                if (production.lhs == START_SYMBOL) {
                    Action action;
                    action.type = ActionType::Accept;
                    setAction((int)i, EOF_SYMBOL, action);
                    continue;
                }

                const std::set<std::string>& followSet = grammar.follow.find(production.lhs)->second;
                for (std::set<std::string>::const_iterator terminal = followSet.begin(); terminal != followSet.end(); ++terminal) {
                    Action action;
                    action.type = ActionType::Reduce;
                    action.value = item->production;
                    setAction((int)i, *terminal, action);
                }
            }
        }
    }
};

void appendList(std::vector<ASTNode* >& target, std::vector<ASTNode* >& source) {
    for (size_t i = 0; i < source.size(); i++) {
        target.push_back(source[i]);
    }
    source.clear();
}

ASTNode* makeBinary(const std::string& op, ASTNode* left, ASTNode* right) {
    ASTNode* node = makeNode(ASTName::BinaryExpr, op);
    node->addChild(left);
    node->addChild(right);
    return node;
}

SemanticValue buildSemantic(const Production& production, std::vector<SemanticValue>& rhs) {
    SemanticValue result;
    result.symbol = production.lhs;
    if (!rhs.empty()) {
        result.line = rhs[0].line;
        result.column = rhs[0].column;
    }

    const std::string& tag = production.tag;

    if (tag == "pass" || tag == "program" || tag == "paren") {
        if (!rhs.empty()) {
            result.ast = rhs[tag == "paren" ? 1 : 0].ast;
            result.list = rhs[tag == "paren" ? 1 : 0].list;
            result.text = rhs[tag == "paren" ? 1 : 0].text;
            result.line = rhs[tag == "paren" ? 1 : 0].line;
            result.column = rhs[tag == "paren" ? 1 : 0].column;
        }
        return result;
    }

    if (tag == "empty" || tag == "list_empty") {
        return result;
    }

    if (tag == "comp_unit") {
        result.ast = makeNode(ASTName::CompUnit);
        for (size_t i = 0; i < rhs[0].list.size(); i++) {
            result.ast->addChild(rhs[0].list[i]);
        }
        return result;
    }

    if (tag == "top_list_empty") {
        return result;
    }

    if (tag == "top_list_cons" || tag == "block_items_cons") {
        if (rhs[0].ast) {
            result.list.push_back(rhs[0].ast);
        }
        appendList(result.list, rhs[1].list);
        return result;
    }

    if (tag == "list_const_def_cons" || tag == "list_var_def_cons" ||
        tag == "list_param_cons" || tag == "list_exp_cons") {
        if (rhs[1].ast) {
            result.list.push_back(rhs[1].ast);
        }
        appendList(result.list, rhs[2].list);
        return result;
    }

    if (tag == "type") {
        result.text = rhs[0].text;
        result.ast = makeNode(ASTName::Type, rhs[0].text, rhs[0].line, rhs[0].column);
        return result;
    }

    if (tag == "name" || tag == "op") {
        result.text = rhs[0].text;
        result.line = rhs[0].line;
        result.column = rhs[0].column;
        return result;
    }

    if (tag == "const_decl") {
        result.ast = makeNode(ASTName::ConstDecl, "", rhs[0].line, rhs[0].column);
        result.ast->addChild(rhs[1].ast);
        result.ast->addChild(rhs[2].ast);
        appendList(result.ast->children, rhs[3].list);
        return result;
    }

    if (tag == "top_type_item") {
        if (rhs[2].text == "func") {
            result.ast = makeNode(ASTName::FuncDef, rhs[1].text, rhs[1].line, rhs[1].column);
            result.ast->addChild(rhs[0].ast);
            ASTNode* tail = rhs[2].ast;
            result.ast->addChild(tail->children[0]);
            result.ast->addChild(tail->children[1]);
            tail->children.clear();
            delete tail;
            rhs[2].ast = NULL;
            return result;
        }

        result.ast = makeNode(ASTName::VarDecl, "", rhs[0].line, rhs[0].column);
        result.ast->addChild(rhs[0].ast);
        ASTNode* firstVar = makeNode(ASTName::VarDef, rhs[1].text, rhs[1].line, rhs[1].column);
        if (rhs[2].ast) {
            firstVar->addChild(rhs[2].ast);
        }
        result.ast->addChild(firstVar);
        appendList(result.ast->children, rhs[2].list);
        return result;
    }

    if (tag == "top_void_func") {
        result.ast = makeNode(ASTName::FuncDef, rhs[1].text, rhs[1].line, rhs[1].column);
        result.ast->addChild(makeNode(ASTName::Type, "void", rhs[0].line, rhs[0].column));
        result.ast->addChild(rhs[3].ast);
        result.ast->addChild(rhs[5].ast);
        return result;
    }

    if (tag == "top_func_tail") {
        result.text = "func";
        result.ast = makeNode("FuncTail");
        result.ast->addChild(rhs[1].ast);
        result.ast->addChild(rhs[3].ast);
        return result;
    }

    if (tag == "top_var_tail") {
        result.text = "var";
        result.ast = rhs[0].ast;
        result.list = rhs[1].list;
        return result;
    }

    if (tag == "var_def_rest_no_init") {
        return result;
    }

    if (tag == "var_def_rest_init") {
        result.ast = rhs[1].ast;
        return result;
    }

    if (tag == "const_def") {
        result.ast = makeNode(ASTName::ConstDef, rhs[0].text, rhs[0].line, rhs[0].column);
        result.ast->addChild(rhs[2].ast);
        return result;
    }

    if (tag == "var_decl") {
        result.ast = makeNode(ASTName::VarDecl, "", rhs[0].line, rhs[0].column);
        result.ast->addChild(rhs[0].ast);
        result.ast->addChild(rhs[1].ast);
        appendList(result.ast->children, rhs[2].list);
        return result;
    }

    if (tag == "var_def_no_init") {
        result.ast = makeNode(ASTName::VarDef, rhs[0].text, rhs[0].line, rhs[0].column);
        return result;
    }

    if (tag == "var_def_init") {
        result.ast = makeNode(ASTName::VarDef, rhs[0].text, rhs[0].line, rhs[0].column);
        result.ast->addChild(rhs[2].ast);
        return result;
    }

    if (tag == "func_def") {
        result.ast = makeNode(ASTName::FuncDef, rhs[1].text, rhs[1].line, rhs[1].column);
        result.ast->addChild(rhs[0].ast);
        result.ast->addChild(rhs[3].ast);
        result.ast->addChild(rhs[5].ast);
        return result;
    }

    if (tag == "param_list_empty") {
        result.ast = makeNode(ASTName::ParamList);
        return result;
    }

    if (tag == "param_list") {
        result.ast = makeNode(ASTName::ParamList);
        result.ast->addChild(rhs[0].ast);
        appendList(result.ast->children, rhs[1].list);
        return result;
    }

    if (tag == "param") {
        result.ast = makeNode(ASTName::Param, rhs[1].text, rhs[1].line, rhs[1].column);
        result.ast->addChild(rhs[0].ast);
        return result;
    }

    if (tag == "block") {
        result.ast = makeNode(ASTName::Block, "", rhs[0].line, rhs[0].column);
        appendList(result.ast->children, rhs[1].list);
        return result;
    }

    if (tag == "assign_stmt") {
        result.ast = makeNode(ASTName::AssignStmt, "", rhs[0].line, rhs[0].column);
        result.ast->addChild(rhs[0].ast);
        result.ast->addChild(rhs[2].ast);
        return result;
    }

    if (tag == "expr_stmt") {
        result.ast = makeNode(ASTName::ExprStmt, "", rhs[0].line, rhs[0].column);
        if (rhs[0].ast) {
            result.ast->addChild(rhs[0].ast);
        }
        return result;
    }

    if (tag == "if_stmt") {
        result.ast = makeNode(ASTName::IfStmt, "", rhs[0].line, rhs[0].column);
        result.ast->addChild(rhs[2].ast);
        result.ast->addChild(rhs[4].ast);
        if (rhs[5].ast) {
            result.ast->addChild(rhs[5].ast);
        }
        return result;
    }

    if (tag == "else_stmt") {
        result.ast = rhs[1].ast;
        return result;
    }

    if (tag == "else_empty") {
        return result;
    }

    if (tag == "return_stmt") {
        result.ast = makeNode(ASTName::ReturnStmt, "", rhs[0].line, rhs[0].column);
        if (rhs[1].ast) {
            result.ast->addChild(rhs[1].ast);
        }
        return result;
    }

    if (tag == "lval") {
        result.ast = makeNode(ASTName::LVal, rhs[0].text, rhs[0].line, rhs[0].column);
        return result;
    }

    if (tag == "int_literal") {
        result.ast = makeNode(ASTName::IntLiteral, rhs[0].text, rhs[0].line, rhs[0].column);
        return result;
    }

    if (tag == "float_literal") {
        result.ast = makeNode(ASTName::FloatLiteral, rhs[0].text, rhs[0].line, rhs[0].column);
        return result;
    }

    if (tag == "call_expr") {
        result.ast = makeNode(ASTName::CallExpr, rhs[0].text, rhs[0].line, rhs[0].column);
        appendList(result.ast->children, rhs[2].list);
        return result;
    }

    if (tag == "unary_expr") {
        result.ast = makeNode(ASTName::UnaryExpr, rhs[0].text, rhs[0].line, rhs[0].column);
        result.ast->addChild(rhs[1].ast);
        return result;
    }

    if (tag == "args") {
        result.list.push_back(rhs[0].ast);
        appendList(result.list, rhs[1].list);
        return result;
    }

    if (tag == "binary_expr") {
        result.ast = makeBinary(rhs[1].text, rhs[0].ast, rhs[2].ast);
        return result;
    }

    if (tag == "binary_expr_direct") {
        result.ast = makeBinary(production.rhs[1], rhs[0].ast, rhs[2].ast);
        return result;
    }

    return result;
}

std::string stackTopSymbol(const std::vector<SemanticValue>& values) {
    if (values.empty()) {
        return "$";
    }
    return values.back().symbol;
}

std::string expectedTokens(const SLRTable& table, int state) {
    std::ostringstream out;
    bool first = true;
    for (std::map<std::pair<int, std::string>, Action>::const_iterator it = table.actions.begin();
         it != table.actions.end();
         ++it) {
        if (it->first.first != state) {
            continue;
        }
        if (!first) {
            out << ", ";
        }
        out << it->first.second;
        first = false;
    }
    return out.str();
}

Token makeEOFLikeToken(const std::vector<Token>& tokens) {
    Token token;
    token.lexeme = EOF_SYMBOL;
    token.grammar = EOF_SYMBOL;
    token.attr = EOF_SYMBOL;
    token.type = TokenType::EndOfFile;
    if (!tokens.empty()) {
        token.line = tokens.back().line;
        token.column = tokens.back().column + (int)tokens.back().lexeme.size();
    }
    return token;
}

} // namespace

ParseResult Parser::parse(const std::vector<Token>& tokens) {
    ParseResult result;

    Grammar grammar;
    SLRTable table(grammar);

    std::vector<Token> input = tokens;
    if (input.empty() || input.back().grammar != EOF_SYMBOL) {
        input.push_back(makeEOFLikeToken(input));
    }

    std::vector<int> stateStack;
    std::vector<SemanticValue> valueStack;
    stateStack.push_back(0);

    size_t index = 0;
    int logNo = 1;

    while (index < input.size()) {
        int state = stateStack.back();
        std::string lookahead = input[index].grammar.empty() ? input[index].lexeme : input[index].grammar;
        std::map<std::pair<int, std::string>, Action>::const_iterator actionIt =
            table.actions.find(std::make_pair(state, lookahead));

        if (actionIt == table.actions.end()) {
            std::ostringstream error;
            error << "Syntax error at line " << input[index].line
                  << ", column " << input[index].column
                  << ": unexpected '" << input[index].lexeme << "'"
                  << ", expected: " << expectedTokens(table, state);
            result.success = false;
            result.errorMessage = error.str();
            return result;
        }

        const Action& action = actionIt->second;

        if (action.type == ActionType::Shift) {
            std::ostringstream log;
            log << logNo++ << "\t" << stackTopSymbol(valueStack) << "#" << lookahead << "\tmove";
            result.reduceLogs.push_back(log.str());

            SemanticValue value;
            value.symbol = lookahead;
            value.text = input[index].lexeme;
            value.line = input[index].line;
            value.column = input[index].column;
            valueStack.push_back(value);
            stateStack.push_back(action.value);
            index++;
            continue;
        }

        if (action.type == ActionType::Reduce) {
            const Production& production = grammar.productions[action.value];
            std::vector<SemanticValue> rhs(production.rhs.size());
            for (int i = (int)production.rhs.size() - 1; i >= 0; i--) {
                rhs[i] = valueStack.back();
                valueStack.pop_back();
                stateStack.pop_back();
            }

            std::ostringstream log;
            log << logNo++ << "\t" << production.lhs << "#" << lookahead
                << "\treduction " << productionToString(production);
            result.reduceLogs.push_back(log.str());

            SemanticValue reduced = buildSemantic(production, rhs);
            reduced.symbol = production.lhs;

            int gotoFrom = stateStack.back();
            std::map<std::pair<int, std::string>, int>::const_iterator gotoIt =
                table.gotos.find(std::make_pair(gotoFrom, production.lhs));
            if (gotoIt == table.gotos.end()) {
                result.success = false;
                result.errorMessage = "Internal parser error: missing goto for " + production.lhs;
                return result;
            }

            valueStack.push_back(reduced);
            stateStack.push_back(gotoIt->second);
            continue;
        }

        if (action.type == ActionType::Accept) {
            result.success = true;
            if (!valueStack.empty()) {
                result.root.reset(valueStack.back().ast);
                valueStack.back().ast = NULL;
            }
            return result;
        }
    }

    result.success = false;
    result.errorMessage = "Syntax error: unexpected end of input.";
    return result;
}

} // namespace cminus
