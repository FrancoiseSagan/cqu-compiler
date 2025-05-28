#include"front/syntax.h"

#include<iostream>
#include<cassert>

using frontend::Parser;

// #define DEBUG_PARSER
#define TODO assert(0 && "todo")
#define CUR_TOKEN_IS(tk_type) (index < token_stream.size() && token_stream[index].type == TokenType::tk_type)
#define PARSE_TOKEN(tk_type) do { \
    auto term = parseTerm(root, TokenType::tk_type); \
    if (term) { \
        root->children.push_back(term); \
    } \
} while(0)
#define PARSE(name, type) do { \
    auto name = parse##type(); \
    if (name) { \
        name->parent = root; \
        root->children.push_back(name); \
    } \
} while(0)

Parser::Parser(const std::vector<frontend::Token>& tokens): index(0), token_stream(tokens) {}

Parser::~Parser() {}

frontend::CompUnit* Parser::get_abstract_syntax_tree(){
    return parseCompUnit();
}

void Parser::log(AstNode* node){
#ifdef DEBUG_PARSER
        std::cout << "in parse" << toString(node->type) << ", cur_token_type::" << toString(token_stream[index].type) << ", token_val::" << token_stream[index].value << '\n';
#endif
}

frontend::CompUnit* Parser::parseCompUnit() {
    auto root = new CompUnit();
    log(root);

    try {
        // Decl | FuncDef
        if (CUR_TOKEN_IS(VOIDTK) ||
            ((CUR_TOKEN_IS(INTTK) || CUR_TOKEN_IS(FLOATTK)) &&
             index + 2 < token_stream.size() &&
             token_stream[index + 1].type == TokenType::IDENFR &&
             token_stream[index + 2].type == TokenType::LPARENT)) {
            PARSE(func_def, FuncDef);
             } else {
                 PARSE(decl, Decl);
             }

        if (index < token_stream.size()) {
            PARSE(comp_unit, CompUnit);
        }

        return root;
    } catch (...) {
        delete root;
        throw;
    }
}

frontend::Decl* Parser::parseDecl() {
    auto root = new Decl();
    log(root);
    
    if (CUR_TOKEN_IS(CONSTTK)) {
        PARSE(const_decl, ConstDecl);
    } else {
        PARSE(var_decl, VarDecl);
    }
    return root;
}

frontend::ConstDecl* Parser::parseConstDecl() {
    auto root = new ConstDecl();
    log(root);
    
    // 'const'
    PARSE_TOKEN(CONSTTK);
    // BType
    PARSE(btype, BType);
    // ConstDef
    PARSE(const_def, ConstDef);
    // {',' ConstDef}
    while (CUR_TOKEN_IS(COMMA)) {
        PARSE_TOKEN(COMMA);
        PARSE(const_def, ConstDef);
    }
    // ';'
    PARSE_TOKEN(SEMICN);
    return root;
}

frontend::BType* Parser::parseBType() {
    auto root = new BType();
    log(root);
    
    if (CUR_TOKEN_IS(INTTK)) {
        PARSE_TOKEN(INTTK);
        root->t = Type::Int;
    } else {
        PARSE_TOKEN(FLOATTK);
        root->t = Type::Float;
    }
    return root;
}

frontend::ConstDef* Parser::parseConstDef() {
    auto root = new ConstDef();
    log(root);
    
    // Ident
    if (CUR_TOKEN_IS(IDENFR)) {
        PARSE_TOKEN(IDENFR);
    }
    // {'[' ConstExp ']'}
    while (CUR_TOKEN_IS(LBRACK)) {
        PARSE_TOKEN(LBRACK);
        PARSE(const_exp, ConstExp);
        PARSE_TOKEN(RBRACK);
    }
    // '='
    PARSE_TOKEN(ASSIGN);
    // ConstInitVal
    PARSE(const_init_val, ConstInitVal);
    return root;
}

frontend::ConstInitVal* Parser::parseConstInitVal() {
    auto root = new ConstInitVal();
    log(root);
    
    if (CUR_TOKEN_IS(LBRACE)) {
        // '{'
        PARSE_TOKEN(LBRACE);
        if (!CUR_TOKEN_IS(RBRACE)) {
            // ConstInitVal
            PARSE(const_init_val, ConstInitVal);
            // {',' ConstInitVal}
            while (CUR_TOKEN_IS(COMMA)) {
                PARSE_TOKEN(COMMA);
                PARSE(const_init_val, ConstInitVal);
            }
        }
        // '}'
        PARSE_TOKEN(RBRACE);
    } else {
        // ConstExp
        PARSE(const_exp, ConstExp);
    }
    return root;
}

frontend::VarDecl* Parser::parseVarDecl() {
    auto root = new VarDecl();
    log(root);
    
    // BType
    PARSE(btype, BType);
    // VarDef
    PARSE(var_def, VarDef);
    // {',' VarDef}
    while (CUR_TOKEN_IS(COMMA)) {
        PARSE_TOKEN(COMMA);
        PARSE(var_def, VarDef);
    }
    // ';'
    PARSE_TOKEN(SEMICN);
    return root;
}

frontend::VarDef* Parser::parseVarDef() {
    auto root = new VarDef();
    log(root);
    
    // Ident
    PARSE_TOKEN(IDENFR);
    // {'[' ConstExp ']'}
    while (CUR_TOKEN_IS(LBRACK)) {
        PARSE_TOKEN(LBRACK);
        PARSE(const_exp, ConstExp);
        PARSE_TOKEN(RBRACK);
    }
    // ['=' InitVal]
    if (CUR_TOKEN_IS(ASSIGN)) {
        PARSE_TOKEN(ASSIGN);
        PARSE(init_val, InitVal);
    }
    return root;
}

frontend::InitVal* Parser::parseInitVal() {
    auto root = new InitVal();
    log(root);
    
    if (CUR_TOKEN_IS(LBRACE)) {
        // '{'
        PARSE_TOKEN(LBRACE);
        if (!CUR_TOKEN_IS(RBRACE)) {
            // InitVal
            PARSE(init_val, InitVal);
            // {',' InitVal}
            while (CUR_TOKEN_IS(COMMA)) {
                PARSE_TOKEN(COMMA);
                PARSE(init_val, InitVal);
            }
        }
        // '}'
        PARSE_TOKEN(RBRACE);
    } else {
        // Exp
        PARSE(exp, Exp);
    }
    return root;
}

frontend::FuncDef* Parser::parseFuncDef() {
    auto root = new FuncDef();
    log(root);
    
    // FuncType
    PARSE(func_type, FuncType);
    // Ident
    PARSE_TOKEN(IDENFR);
    // '('
    PARSE_TOKEN(LPARENT);
    // [FuncFParams]
    if (!CUR_TOKEN_IS(RPARENT)) {
        PARSE(func_fparams, FuncFParams);
    }
    // ')'
    PARSE_TOKEN(RPARENT);
    // Block
    PARSE(block, Block);
    return root;
}

frontend::FuncType* Parser::parseFuncType() {
    auto root = new FuncType();
    log(root);
    
    if (CUR_TOKEN_IS(VOIDTK)) {
        PARSE_TOKEN(VOIDTK);
    } else if (CUR_TOKEN_IS(INTTK)) {
        PARSE_TOKEN(INTTK);
    } else {
        PARSE_TOKEN(FLOATTK);
    }
    return root;
}

frontend::FuncFParams* Parser::parseFuncFParams() {
    auto root = new FuncFParams();
    log(root);
    
    // FuncFParam
    PARSE(func_fparam, FuncFParam);
    // {',' FuncFParam}
    while (CUR_TOKEN_IS(COMMA)) {
        PARSE_TOKEN(COMMA);
        PARSE(func_fparam, FuncFParam);
    }
    return root;
}

frontend::FuncFParam* Parser::parseFuncFParam() {
    auto root = new FuncFParam();
    log(root);
    
    // BType
    PARSE(btype, BType);
    // Ident
    PARSE_TOKEN(IDENFR);
    // ['[' ']' {'[' Exp ']'}]
    if (CUR_TOKEN_IS(LBRACK)) {
        PARSE_TOKEN(LBRACK);
        PARSE_TOKEN(RBRACK);
        while (CUR_TOKEN_IS(LBRACK)) {
            PARSE_TOKEN(LBRACK);
            PARSE(exp, Exp);
            PARSE_TOKEN(RBRACK);
        }
    }
    return root;
}

frontend::Block* Parser::parseBlock() {
    auto root = new Block();
    log(root);
    
    // '{'
    PARSE_TOKEN(LBRACE);
    // {BlockItem}
    while (!CUR_TOKEN_IS(RBRACE)) {
        PARSE(block_item, BlockItem);
    }
    // '}'
    PARSE_TOKEN(RBRACE);
    return root;
}

frontend::BlockItem* Parser::parseBlockItem() {
    auto root = new BlockItem();
    log(root);
    
    if (CUR_TOKEN_IS(CONSTTK) || 
        (CUR_TOKEN_IS(INTTK) || CUR_TOKEN_IS(FLOATTK)) && 
        token_stream[index + 1].type != TokenType::LPARENT) {
        // Decl
        PARSE(decl, Decl);
    } else {
        // Stmt
        PARSE(stmt, Stmt);
    }
    return root;
}

frontend::Stmt* Parser::parseStmt() {
    auto root = new Stmt();
    log(root);

    if (CUR_TOKEN_IS(IFTK)) {
        // 'if' '(' Cond ')' Stmt ['else' Stmt]
        PARSE_TOKEN(IFTK);
        PARSE_TOKEN(LPARENT);
        PARSE(cond, Cond);
        PARSE_TOKEN(RPARENT);
        PARSE(stmt, Stmt);
        if (CUR_TOKEN_IS(ELSETK)) {
            PARSE_TOKEN(ELSETK);
            PARSE(stmt, Stmt);
        }
    }
    else if (CUR_TOKEN_IS(WHILETK)) {
        // 'while' '(' Cond ')' Stmt
        PARSE_TOKEN(WHILETK);
        PARSE_TOKEN(LPARENT);
        PARSE(cond, Cond);
        PARSE_TOKEN(RPARENT);
        PARSE(stmt, Stmt);
    }
    else if (CUR_TOKEN_IS(BREAKTK)) {
        // 'break' ';'
        PARSE_TOKEN(BREAKTK);
        PARSE_TOKEN(SEMICN);
    }
    else if (CUR_TOKEN_IS(CONTINUETK)) {
        // 'continue' ';'
        PARSE_TOKEN(CONTINUETK);
        PARSE_TOKEN(SEMICN);
    }
    else if (CUR_TOKEN_IS(RETURNTK)) {
        // 'return' [Exp] ';'
        PARSE_TOKEN(RETURNTK);
        if (!CUR_TOKEN_IS(SEMICN)) {
            PARSE(exp, Exp);
        }
        PARSE_TOKEN(SEMICN);
    }
    else if (CUR_TOKEN_IS(LBRACE)) {
        // Block
        PARSE(block, Block);
    }
    else {
        // 检查: LVal '=' Exp ';' 回溯
        uint32_t save_index = index;
        bool is_assign = false;
        try {
            PARSE(lval, LVal);
            if (CUR_TOKEN_IS(ASSIGN)) {
                is_assign = true;
            }
        } catch (...) {}
        index = save_index;
        root->children.clear();

        if (is_assign) {
            // LVal '=' Exp ';'
            PARSE(lval, LVal);
            PARSE_TOKEN(ASSIGN);
            PARSE(exp, Exp);
            PARSE_TOKEN(SEMICN);
        } else {
            // [Exp] ';'
            if (!CUR_TOKEN_IS(SEMICN)) {
                PARSE(exp, Exp);
            }
            PARSE_TOKEN(SEMICN);
        }
    }
    return root;
}

frontend::Exp* Parser::parseExp() {
    auto root = new Exp();
    log(root);
    
    // Exp -> AddExp
    PARSE(add_exp, AddExp);
    return root;
}

frontend::Cond* Parser::parseCond() {
    auto root = new Cond();
    log(root);
    
    // Cond -> LOrExp
    PARSE(lor_exp, LOrExp);
    return root;
}

frontend::LVal* Parser::parseLVal() {
    auto root = new LVal();
    log(root);
    
    // Ident {'[' Exp ']'}
    PARSE_TOKEN(IDENFR);
    while (CUR_TOKEN_IS(LBRACK)) {
        PARSE_TOKEN(LBRACK);
        PARSE(exp, Exp);
        PARSE_TOKEN(RBRACK);
    }
    return root;
}

frontend::Number* Parser::parseNumber() {
    auto root = new Number();
    log(root);
    
    // IntConst | FloatConst
    if (CUR_TOKEN_IS(INTLTR)) {
        PARSE_TOKEN(INTLTR);
    } else {
        PARSE_TOKEN(FLOATLTR);
    }
    return root;
}

frontend::PrimaryExp* Parser::parsePrimaryExp() {
    auto root = new PrimaryExp();
    log(root);
    
    if (CUR_TOKEN_IS(LPARENT)) {
        // '(' Exp ')'
        PARSE_TOKEN(LPARENT);
        PARSE(exp, Exp);
        PARSE_TOKEN(RPARENT);
    } else if (CUR_TOKEN_IS(INTLTR) || CUR_TOKEN_IS(FLOATLTR)) {
        // Number
        PARSE(number, Number);
    } else {
        // LVal
        PARSE(lval, LVal);
    }
    return root;
}

frontend::UnaryExp* Parser::parseUnaryExp() {
    auto root = new UnaryExp();
    log(root);
    
    if (CUR_TOKEN_IS(IDENFR) && token_stream[index + 1].type == TokenType::LPARENT) {
        // Ident '(' [FuncRParams] ')'
        PARSE_TOKEN(IDENFR);
        PARSE_TOKEN(LPARENT);
        if (!CUR_TOKEN_IS(RPARENT)) {
            PARSE(func_rparams, FuncRParams);
        }
        PARSE_TOKEN(RPARENT);
    } else if (CUR_TOKEN_IS(PLUS) || CUR_TOKEN_IS(MINU) || CUR_TOKEN_IS(NOT)) {
        // UnaryOp UnaryExp
        PARSE(unary_op, UnaryOp);
        PARSE(unary_exp, UnaryExp);
    } else {
        // PrimaryExp
        PARSE(primary_exp, PrimaryExp);
    }
    return root;
}

frontend::UnaryOp* Parser::parseUnaryOp() {
    auto root = new UnaryOp();
    log(root);
    
    // '+' | '-' | '!'
    if (CUR_TOKEN_IS(PLUS)) {
        PARSE_TOKEN(PLUS);
    } else if (CUR_TOKEN_IS(MINU)) {
        PARSE_TOKEN(MINU);
    } else {
        PARSE_TOKEN(NOT);
    }
    return root;
}

frontend::FuncRParams* Parser::parseFuncRParams() {
    auto root = new FuncRParams();
    log(root);
    
    // Exp {',' Exp}
    PARSE(exp, Exp);
    while (CUR_TOKEN_IS(COMMA)) {
        PARSE_TOKEN(COMMA);
        PARSE(exp, Exp);
    }
    return root;
}

frontend::MulExp* Parser::parseMulExp() {
    auto root = new MulExp();
    log(root);
    
    // UnaryExp {('*' | '/' | '%') UnaryExp}
    PARSE(unary_exp, UnaryExp);
    while (CUR_TOKEN_IS(MULT) || CUR_TOKEN_IS(DIV) || CUR_TOKEN_IS(MOD)) {
        if (CUR_TOKEN_IS(MULT)) {
            PARSE_TOKEN(MULT);
        } else if (CUR_TOKEN_IS(DIV)) {
            PARSE_TOKEN(DIV);
        } else {
            PARSE_TOKEN(MOD);
        }
        PARSE(unary_exp, UnaryExp);
    }
    return root;
}

frontend::AddExp* Parser::parseAddExp() {
    auto root = new AddExp();
    log(root);
    
    // MulExp {('+' | '-') MulExp}
    PARSE(mul_exp, MulExp);
    while (CUR_TOKEN_IS(PLUS) || CUR_TOKEN_IS(MINU)) {
        if (CUR_TOKEN_IS(PLUS)) {
            PARSE_TOKEN(PLUS);
        } else {
            PARSE_TOKEN(MINU);
        }
        PARSE(mul_exp, MulExp);
    }
    return root;
}

frontend::RelExp* Parser::parseRelExp() {
    auto root = new RelExp();
    log(root);
    
    // AddExp {('<' | '>' | '<=' | '>=') AddExp}
    PARSE(add_exp, AddExp);
    while (CUR_TOKEN_IS(LSS) || CUR_TOKEN_IS(GTR) || 
           CUR_TOKEN_IS(LEQ) || CUR_TOKEN_IS(GEQ)) {
        if (CUR_TOKEN_IS(LSS)) {
            PARSE_TOKEN(LSS);
        } else if (CUR_TOKEN_IS(GTR)) {
            PARSE_TOKEN(GTR);
        } else if (CUR_TOKEN_IS(LEQ)) {
            PARSE_TOKEN(LEQ);
        } else {
            PARSE_TOKEN(GEQ);
        }
        PARSE(add_exp, AddExp);
    }
    return root;
}

frontend::EqExp* Parser::parseEqExp() {
    auto root = new EqExp();
    log(root);
    
    // RelExp {('==' | '!=') RelExp}
    PARSE(rel_exp, RelExp);
    while (CUR_TOKEN_IS(EQL) || CUR_TOKEN_IS(NEQ)) {
        if (CUR_TOKEN_IS(EQL)) {
            PARSE_TOKEN(EQL);
        } else {
            PARSE_TOKEN(NEQ);
        }
        PARSE(rel_exp, RelExp);
    }
    return root;
}

frontend::LAndExp* Parser::parseLAndExp() {
    auto root = new LAndExp();
    log(root);
    
    // EqExp ['&&' LAndExp]
    PARSE(eq_exp, EqExp);
    if (CUR_TOKEN_IS(AND)) {
        PARSE_TOKEN(AND);
        PARSE(land_exp, LAndExp);
    }
    return root;
}

frontend::LOrExp* Parser::parseLOrExp() {
    auto root = new LOrExp();
    log(root);
    
    // LAndExp ['||' LOrExp]
    PARSE(land_exp, LAndExp);
    if (CUR_TOKEN_IS(OR)) {
        PARSE_TOKEN(OR);
        PARSE(lor_exp, LOrExp);
    }
    return root;
}

frontend::ConstExp* Parser::parseConstExp() {
    auto root = new ConstExp();
    log(root);
    
    // ConstExp -> AddExp
    PARSE(add_exp, AddExp);
    return root;
}

frontend::Term* Parser::parseTerm(AstNode* parent, TokenType expected) {
    if (index >= token_stream.size() || token_stream[index].type != expected) {
        return nullptr;
    }
    auto term = new Term(token_stream[index], parent);
    index++;
    return term;
}
