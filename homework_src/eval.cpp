#include<map>
#include<cassert>
#include <cstdint>
#include<string>
#include<iostream>
#include<vector>
#include<set>
#include<queue>

#define TODO assert(0 && "TODO")
// #define DEBUG_DFA

// enumerate for Status
enum class State {
    Empty,              // space, \n, \r ...
    IntLiteral,         // int literal, like '1' '01900', '0xAB', '0b11001'
    op                  // operators and '(', ')'
};
std::string toString(State s) {
    switch (s) {
    case State::Empty: return "Empty";
    case State::IntLiteral: return "IntLiteral";
    case State::op: return "op";
    default:
        assert(0 && "invalid State");
    }
    return "";
}

// enumerate for Token type
enum class TokenType{
    INTLTR,        // int literal
    PLUS,          // +
    MINU,          // -
    MULT,          // *
    DIV,           // /
    LPARENT,       // (
    RPARENT,       // )
};
std::string toString(TokenType type) {
    switch (type) {
    case TokenType::INTLTR:  return "INTLTR";
    case TokenType::PLUS:    return "PLUS";
    case TokenType::MINU:    return "MINU";
    case TokenType::MULT:    return "MULT";
    case TokenType::DIV:     return "DIV";
    case TokenType::LPARENT: return "LPARENT";
    case TokenType::RPARENT: return "RPARENT";
    default:
        assert(0 && "invalid token type");
        break;
    }
    return "";
}

// definition of Token
struct Token {
    TokenType type;
    std::string value;
};

// definition of DFA
struct DFA {
    /**
     * @brief constructor, set the init state to State::Empty
     */
    DFA();

    /**
     * @brief destructor
     */
    ~DFA();

    // the meaning of copy and assignment for a DFA is not clear, so we do not allow them
    DFA(const DFA&) = delete;   // copy constructor
    DFA& operator=(const DFA&) = delete;    // assignment

    /**
     * @brief take a char as input, change state to next state, and output a Token if necessary
     * @param[in] input: the input character
     * @param[out] buf: the output Token buffer
     * @return  return true if a Token is produced, the buf is valid then
     */
    bool next(char input, Token& buf);

    /**
     * @brief reset the DFA state to begin
     */
    void reset();

private:
    State cur_state;    // record current state of the DFA
    std::string cur_str;    // record input characters
};


DFA::DFA(): cur_state(State::Empty), cur_str() {}

DFA::~DFA() {}

// helper function, you are not require to implement these, but they may be helpful
bool isoperator(char c) {
    switch(c) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '(':
        case ')':
            return true;
        default:
            return false;
    }
}

TokenType get_op_type(std::string s) {
    if (s.length() != 1) {
        assert(0 && "fucking invalid operator");
    }
    char c = s[0];
    switch(c) {
        case '+': return TokenType::PLUS;
        case '-': return TokenType::MINU;
        case '*': return TokenType::MULT;
        case '/': return TokenType::DIV;
        case '(': return TokenType::LPARENT;
        case ')': return TokenType::RPARENT;
        default:
            assert(0 && "fucking invalid operator");
    }
    return TokenType::PLUS;
}

bool DFA::next(char input, Token& buf) {
    if (input == '\n') {
        switch (cur_state) {
            case State::Empty: return false;
            case State::op:
                buf = {get_op_type(cur_str), cur_str};
                return true;
            default:
                buf = {TokenType::INTLTR, cur_str};
                return true;
        }
    }
    std::string output_str;
    switch (cur_state) {
        case State::Empty: {
            if (input == ' ') {
                return false;
            }
            if (std::isdigit(input) || std::isalpha(input)) {
                cur_state = State::IntLiteral;
                cur_str += input;
            }
            if (isoperator(input)) {
                cur_state = State::op;
                cur_str += input;
            }
            return false;
        }
        case State::IntLiteral: {
            if (input == ' ') {
                cur_state = State::Empty;
                output_str = cur_str;
                cur_str = "";

                buf = {TokenType::INTLTR, output_str};
                return true;
            }
            if (std::isdigit(input) || std::isalpha(input)) {
                cur_str += input;
            }
            if (isoperator(input)) {
                cur_state = State::op;
                output_str = cur_str;
                cur_str = input;

                buf = {TokenType::INTLTR, output_str};
                return true;
            }
            return false;
        }
        case State::op: {
            if (input == ' ') {
                cur_state = State::Empty;
                output_str = cur_str;
                cur_str = "";

                buf = {get_op_type(output_str), output_str};
                return true;
            }
            if (std::isdigit(input) || std::isalpha(input)) {
                cur_state = State::IntLiteral;
                output_str = cur_str;
                cur_str = input;

                buf = {get_op_type(output_str), output_str};
                return true;
            }
            if (isoperator(input)) {
                cur_state = State::op;
                output_str = cur_str;
                cur_str = input;

                buf = {get_op_type(output_str), output_str};
                return true;
            }
            return false;
        }
    }
}

void DFA::reset() {
    cur_state = State::Empty;
    cur_str = "";
}

// hw2
enum class NodeType {
    TERMINAL,       // terminal lexical unit
    EXP,
    NUMBER,
    PRIMARYEXP,
    UNARYEXP,
    UNARYOP,
    MULEXP,
    ADDEXP,
    NONE
};
std::string toString(NodeType nt) {
    switch (nt) {
    case NodeType::TERMINAL:   return "Terminal";
    case NodeType::EXP:        return "Exp";
    case NodeType::NUMBER:     return "Number";
    case NodeType::PRIMARYEXP: return "PrimaryExp";
    case NodeType::UNARYEXP:   return "UnaryExp";
    case NodeType::UNARYOP:    return "UnaryOp";
    case NodeType::MULEXP:     return "MulExp";
    case NodeType::ADDEXP:     return "AddExp";
    case NodeType::NONE:       return "NONE";
    default:
        assert(0 && "invalid node type");
        break;
    }
    return "";
}

// tree node basic class
struct AstNode{
    int value;
    NodeType type;  // the node type
    AstNode* parent;    // the parent node
    std::vector<AstNode*> children;     // children of node

    /**
     * @brief constructor
     */
    AstNode(NodeType t = NodeType::NONE, AstNode* p = nullptr): type(t), parent(p), value(0) {}

    /**
     * @brief destructor
     */
    virtual ~AstNode() {
        for(auto child: children) {
            delete child;
        }
    }

    // rejcet copy and assignment
    AstNode(const AstNode&) = delete;
    AstNode& operator=(const AstNode&) = delete;
};

// definition of Parser
// a parser should take a token stream as input, then parsing it, output a AST
struct Parser {
    uint32_t index;
    const std::vector<Token>& token_stream;

    Parser(const std::vector<Token>& tokens): index(0), token_stream(tokens) {}
    ~Parser() {}

    Token peek() {
        if (index >= token_stream.size()) {
            return {TokenType::RPARENT, ""};
        }
        return token_stream[index];
    }

    void advance() {
        index++;
    }

    int to_int(const std::string& s) {
        if (s.empty()) {
            throw std::invalid_argument("Empty string");
        }

        size_t start_pos = 0;
        bool is_negative = false;

        // Handle negative numbers
        if (s[0] == '-') {
            is_negative = true;
            start_pos = 1;
        }

        int base = 10;
        if (s.size() >= start_pos + 2 && s[start_pos] == '0') {
            switch (s[start_pos + 1]) {
                case 'b': case 'B':
                    base = 2;
                start_pos += 2;
                break;
                case 'x': case 'X':
                    base = 16;
                start_pos += 2;
                break;
                default:
                    base = 8;
                start_pos += 1;
                break;
            }
        }

        if (base == 2) {
            int value = 0;
            for (size_t i = start_pos; i < s.size(); ++i) {
                char c = s[i];
                if (c != '0' && c != '1') {
                    throw std::invalid_argument("Invalid binary digit");
                }
                value = value * 2 + (c - '0');
            }
            return is_negative ? -value : value;
        }

        int value = std::stoi(s.substr(start_pos), nullptr, base);
        return is_negative ? -value : value;
    }

    // Exp -> AddExp
    AstNode* parseExp() {
        return parseAddExp();
    }

    // Number -> IntConst
    AstNode* parseNumber() {
        Token token = peek();
        if (token.type != TokenType::INTLTR) {
            throw std::runtime_error("Expected number");
        }
        advance();
        AstNode* node = new AstNode(NodeType::NUMBER);
        node->value = to_int(token.value);
        return node;
    }

    // PrimaryExp -> '(' Exp ')' | Number
    AstNode* parsePrimaryExp() {
        Token token = peek();
        if (token.type == TokenType::LPARENT) {
            advance();
            AstNode* node = parseExp();
            if (peek().type != TokenType::RPARENT) {
                throw std::runtime_error("Expected ')'");
            }
            advance();
            return node;
        }
        return parseNumber();
    }

    // UnaryOp -> '+' | '-'
    AstNode* parseUnaryOp() {
        Token token = peek();
        if (token.type == TokenType::PLUS || token.type == TokenType::MINU) {
            advance();
            AstNode* node = new AstNode(NodeType::UNARYOP);
            node->value = (token.type == TokenType::MINU) ? -1 : 1;
            return node;
        }
        throw std::runtime_error("Expected unary operator");
    }

    // UnaryExp -> PrimaryExp | UnaryOp UnaryExp
    AstNode* parseUnaryExp() {
        Token token = peek();
        if (token.type == TokenType::PLUS || token.type == TokenType::MINU) {
            AstNode* unary_op = parseUnaryOp();
            AstNode* unary_exp = parseUnaryExp();

            AstNode* node = new AstNode(NodeType::UNARYEXP);
            node->children.push_back(unary_op);
            node->children.push_back(unary_exp);
            return node;
        }
        return parsePrimaryExp();
    }

    // MulExp -> UnaryExp { ('*' | '/') UnaryExp }
    AstNode* parseMulExp() {
        AstNode* left = parseUnaryExp();
        while (true) {
            Token token = peek();
            if (token.type == TokenType::MULT || token.type == TokenType::DIV) {
                AstNode* node = new AstNode(NodeType::MULEXP);
                node->value = (token.type == TokenType::MULT) ? 0 : 1;
                advance();
                node->children.push_back(left);
                node->children.push_back(parseUnaryExp());
                left = node;
            } else {
                break;
            }
        }
        return left;
    }

    // AddExp -> MulExp { ('+' | '-') MulExp }
    AstNode* parseAddExp() {
        AstNode* left = parseMulExp();
        while (true) {
            Token token = peek();
            if (token.type == TokenType::PLUS || token.type == TokenType::MINU) {
                AstNode* node = new AstNode(NodeType::ADDEXP);
                node->value = (token.type == TokenType::PLUS) ? 0 : 1; // 用value区分加减
                advance();
                node->children.push_back(left);
                node->children.push_back(parseMulExp());
                left = node;
            } else {
                break;
            }
        }
        return left;
    }

    AstNode* get_abstract_syntax_tree() {
        return parseExp();
    }
};

int get_value(AstNode* node) {
    if (!node) return 0;

    switch (node->type) {
        case NodeType::NUMBER:
            return node->value;

        case NodeType::UNARYEXP: {
            int op = get_value(node->children[0]);
            int val = get_value(node->children[1]);
            return op * val;
        }

        case NodeType::UNARYOP:
            return node->value;

        case NodeType::MULEXP: {
            int left = get_value(node->children[0]);
            int right = get_value(node->children[1]);
            if (node->value == 0) {
                return left * right;
            } else {
                if (right == 0) throw std::runtime_error("Division by zero");
                return left / right;
            }
        }

        case NodeType::ADDEXP: {
            int left = get_value(node->children[0]);
            int right = get_value(node->children[1]);
            if (node->value == 0) {
                return left + right;
            } else {
                return left - right;
            }
        }

        case NodeType::PRIMARYEXP:
            return get_value(node->children[0]);

        default:
            throw std::runtime_error("Unknown node type");
    }
}

int main(){
    std::string stdin_str;
    std::getline(std::cin, stdin_str);
    stdin_str += "\n";
    DFA dfa;
    Token tk;
    std::vector<Token> tokens;
    for (size_t i = 0; i < stdin_str.size(); i++) {
        if(dfa.next(stdin_str[i], tk)){
            tokens.push_back(tk);
        }
    }

    Parser parser(tokens);
    auto root = parser.get_abstract_syntax_tree();
    root-> value = get_value(root);

    std::cout << root->value;

    return 0;
}