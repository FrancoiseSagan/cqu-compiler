#include"front/lexical.h"
#include"front/token.h"

#include<map>
#include<cassert>
#include<string>
#include<unordered_map>

#define TODO assert(0 && "todo")

// #define DEBUG_DFA
// #define DEBUG_SCANNER

std::string frontend::toString(State s) {
    switch (s) {
    case State::Empty: return "Empty";
    case State::Ident: return "Ident";
    case State::IntLiteral: return "IntLiteral";
    case State::FloatLiteral: return "FloatLiteral";
    case State::op: return "op";
    default:
        assert(0 && "invalid State");
    }
    return "";
}

bool isop(char input) {
    if(frontend::op_chars.find(input) == frontend::op_chars.end()) {
        return false;
    }
    return true;
}

frontend::DFA::DFA(): cur_state(frontend::State::Empty), cur_str() {}

frontend::DFA::~DFA() {}

bool frontend::DFA::next(char input, Token& buf) {

    switch (cur_state) {
    case State::Empty: {
        if (isspace(input)) {
            return false;
        }
        if (isalpha(input) || input == '_') {
            cur_str = input;
            cur_state = State::Ident;
            return false;
        }
        if (isdigit(input)) {
            cur_str = input;
            cur_state = State::IntLiteral;
            return false;
        }
        if (input == '.') {
            cur_str = input;
            cur_state = State::FloatLiteral;
            return false;
        }
        if (isop(input)) {
            cur_str = input;
            cur_state = State::op;
            return false;
        }
        assert(0 && "invalid input character");
    }

    case State::Ident: {
        if (isalnum(input) || input == '_') {
            cur_str += input;
            return false;
        }
        auto it = keywords.find(cur_str);
        if (it == keywords.end()) {
            buf = Token{TokenType::IDENFR, cur_str};
            cur_str.clear();
            cur_state = State::Empty;
            return true;
        }
        buf = Token{it->second, cur_str};
        cur_str.clear();
        cur_state = State::Empty;
        return true;
    }

    case State::IntLiteral: {
        if (isdigit(input) || isalpha(input)) {
            cur_str += input;
            return false;
        }
        if (input == '.') {
            cur_str += input;
            cur_state = State::FloatLiteral;
            return false;
        }
        buf = Token{TokenType::INTLTR, cur_str};
        cur_str.clear();
        cur_state = State::Empty;
        return true;
    }

    case State::FloatLiteral: {
        if (isdigit(input)) {
            cur_str += input;
            return false;
        }
        if (input == '.') {
            assert(0 && "invalid float literal with multiple dots");
        }
        buf = Token{TokenType::FLOATLTR, cur_str};
        cur_str.clear();
        cur_state = State::Empty;
        return true;
    }

    case State::op: {
        if (isop(input)) {
            auto double_str = cur_str + input;
            if(double_ops.find(double_str) != double_ops.end()) {
                // this is double op
                cur_str = double_str;
                return false;
            }
        }
        auto it = operators.find(cur_str);
        if (it == operators.end()) {
            std::cout << "FUCK" << std::endl;
        }
        assert(it != operators.end() && "not valid op");
        buf = Token{it->second, cur_str};
        cur_str.clear();
        cur_state = State::Empty;
        return true;
    }

    default:
        assert(0 && "invalid state");
    }

#ifdef DEBUG_DFA
#include<iostream>
    std::cout << "in state [" << toString(cur_state) << "], input = \'" << input << "\', str = " << cur_str << "\t";
#endif
    TODO;
#ifdef DEBUG_DFA
    std::cout << "next state is [" << toString(cur_state) << "], next str = " << cur_str << "\t, ret = " << ret << std::endl;
#endif

}

void frontend::DFA::reset() {
    cur_state = State::Empty;
    cur_str = "";
}

frontend::Scanner::Scanner(std::string filename): fin(filename) {
    if(!fin.is_open()) {
        assert(0 && "in Scanner constructor, input file cannot open");
    }
}

frontend::Scanner::~Scanner() {
    fin.close();
}

std::vector<frontend::Token> frontend::Scanner::run() {
    DFA dfa;
    dfa.reset();
    std::vector<Token> res;
    Token tk;
    char ch;
    bool in_single_line_comment = false; // 单行注释
    bool in_multi_line_comment = false;  // 多行注释

    while (fin.get(ch)) {
        if (in_single_line_comment) {
            if (ch == '\n') {
                in_single_line_comment = false;
            }
            continue;
        }
        else if (in_multi_line_comment) {
            // 多行注释直到遇到 */ 结束
            if (ch == '*') {
                char next_ch = fin.peek();
                if (next_ch == '/') {
                    in_multi_line_comment = false;
                    fin.get(next_ch); // 消耗 '/'
                }
            }
            continue;
        }

        if (ch == '/') {
            char next_ch = fin.peek();
            if (next_ch == '/') {
                // 单行注释
                in_single_line_comment = true;
                fin.get(next_ch);
                continue;
            }
            else if (next_ch == '*') {
                // 多行注释
                in_multi_line_comment = true;
                fin.get(next_ch);
                continue;
            }
        }

        if (dfa.next(ch, tk)) {
            res.push_back(tk);
            dfa.next(ch, tk);
        }
    }

    if (dfa.next(' ', tk)) {
        res.push_back(tk);
    }

    if (in_multi_line_comment) {
        throw std::runtime_error("Unclosed multi-line comment");
    }

#ifdef DEBUG_SCANNER
    for (const auto& token : res) {
        std::cout << "token: " << toString(token.type) << "\t" << token.value << std::endl;
    }
#endif

    return res;
}