/**
 * @file syntax.h
 * @author Yuntao Dai (d1581209858@live.com)
 * @brief 
 * in the second part, we already has a token stream, now we should analysis it and result in a syntax tree, 
 * which we also called it AST(abstract syntax tree)
 * @version 0.1
 * @date 2022-12-15
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef SYNTAX_H
#define SYNTAX_H

#include"front/abstract_syntax_tree.h"
#include"front/token.h"

#include<vector>

namespace frontend {
  // definition of Parser
  // a parser should take a token stream as input, then parsing it, output a AST
  struct Parser {
    uint32_t index; // current token index
    const std::vector<Token> &token_stream;

    /**
     * @brief constructor
     * @param tokens: the input token_stream
     */
    Parser(const std::vector<Token> &tokens);

    /**
     * @brief destructor
     */
    ~Parser();

    /**
     * @brief creat the abstract syntax tree
     * @return the root of abstract syntax tree
     */
    CompUnit *get_abstract_syntax_tree();

    /**
     * @brief for debug, should be called in the beginning of recursive descent functions
     * @param node: current parsing node
     */
    void log(AstNode *node);

    // parse function
    CompUnit *parseCompUnit(); // 编译单元解析
    Decl *parseDecl(); // 声明解析
    FuncDef *parseFuncDef(); // 函数定义解析

    ConstDecl *parseConstDecl(); // 常量声明解析
    BType *parseBType(); // 基本类型解析
    ConstDef *parseConstDef(); // 常量定义解析
    ConstInitVal *parseConstInitVal(); // 常量初值解析
    VarDecl *parseVarDecl(); // 变量声明解析
    VarDef *parseVarDef(); // 变量定义解析
    InitVal *parseInitVal(); // 初值解析

    FuncType *parseFuncType(); // 函数类型解析
    FuncFParam *parseFuncFParam(); // 函数形参解析
    FuncFParams *parseFuncFParams(); // 函数形参列表解析
    FuncRParams *parseFuncRParams(); // 函数实参列表解析

    Block *parseBlock(); // 语句块解析
    BlockItem *parseBlockItem(); // 语句块项解析
    Stmt *parseStmt(); // 语句解析

    Exp *parseExp(); // 表达式解析
    Cond *parseCond(); // 条件表达式解析
    LVal *parseLVal(); // 左值解析
    Number *parseNumber(); // 数字解析
    PrimaryExp *parsePrimaryExp(); // 基本表达式解析
    UnaryExp *parseUnaryExp(); // 一元表达式解析
    UnaryOp *parseUnaryOp(); // 一元运算符解析

    MulExp *parseMulExp(); // 乘法表达式解析
    AddExp *parseAddExp(); // 加法表达式解析
    RelExp *parseRelExp(); // 关系表达式解析
    EqExp *parseEqExp(); // 相等表达式解析
    LAndExp *parseLAndExp(); // 逻辑与表达式解析
    LOrExp *parseLOrExp(); // 逻辑或表达式解析

    ConstExp *parseConstExp(); // 常量表达式解析

    Term *parseTerm(AstNode *parent, TokenType expected);
  };
} // namespace frontend

#endif
