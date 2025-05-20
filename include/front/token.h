/**
 * @file Token.h
 * @author Yuntao Dai (d1581209858@live.com)
 * @brief 
 * definition of Token
 * a Token should has its type and its value(the string)
 * @version 0.1
 * @date 2022-12-14
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef TOKEN_H
#define TOKEN_H

#include<string>
#include<unordered_map>
#include<unordered_set>

namespace frontend {

// enumerate for Token type
enum class TokenType{
    IDENFR,     // identifier	
    INTLTR,		// int literal
    FLOATLTR,		// float literal
    CONSTTK,		// const
    VOIDTK,		// void
    INTTK,		// int
    FLOATTK,		// float
    IFTK,		// if
    ELSETK,		// else
    WHILETK,		// while
    CONTINUETK,		// continue
    BREAKTK,		// break
    RETURNTK,		// return
    PLUS,		// +
    MINU,		// -
    MULT,		// *
    DIV,		// /
    MOD,      // %
    LSS,		// <
    GTR,		// >
    COLON,		// :
    ASSIGN,		// =
    SEMICN,		// ;
    COMMA,		// ,
    LPARENT,		// (
    RPARENT,		// )
    LBRACK,		// [
    RBRACK,		// ]
    LBRACE,		// {
    RBRACE,		// }
    NOT,		// !
    LEQ,		// <=
    GEQ,		// >=
    EQL,		// ==
    NEQ,		// !=
    AND,        // &&
    OR,         // ||
};
std::string toString(TokenType);

struct Token {
    TokenType type;
    std::string value;
};

  const std::unordered_map<std::string, TokenType> keywords = {
    {"const", TokenType::CONSTTK},
    {"int", TokenType::INTTK},
    {"float", TokenType::FLOATTK},
    {"if", TokenType::IFTK},
    {"else", TokenType::ELSETK},
    {"while", TokenType::WHILETK},
    {"continue", TokenType::CONTINUETK},
    {"break", TokenType::BREAKTK},
    {"return", TokenType::RETURNTK},
    {"void", TokenType::VOIDTK}
  };

  // Operators hash table
  const std::unordered_map<std::string, TokenType> operators = {
    {"+", TokenType::PLUS},
    {"-", TokenType::MINU},
    {"*", TokenType::MULT},
    {"/", TokenType::DIV},
    {"%", TokenType::MOD},
    {"<", TokenType::LSS},
    {">", TokenType::GTR},
    {":", TokenType::COLON},
    {"=", TokenType::ASSIGN},
    {";", TokenType::SEMICN},
    {",", TokenType::COMMA},
    {"(", TokenType::LPARENT},
    {")", TokenType::RPARENT},
    {"[", TokenType::LBRACK},
    {"]", TokenType::RBRACK},
    {"{", TokenType::LBRACE},
    {"}", TokenType::RBRACE},
    {"!", TokenType::NOT},
    {"<=", TokenType::LEQ},
    {">=", TokenType::GEQ},
    {"==", TokenType::EQL},
    {"!=", TokenType::NEQ},
    {"&&", TokenType::AND},
    {"||", TokenType::OR}
  };

  const std::unordered_set<char> op_chars = {
    '+', '-', '*', '/', '%',
    '<', '>', ':', '=', ';',
    ',', '(', ')', '[', ']',
    '{', '}', '!', '&', '|'
};

  const std::unordered_set<std::string> double_ops = {
    "<=", ">=", "==", "!=", "&&", "||",
};


} // namespace frontend


#endif