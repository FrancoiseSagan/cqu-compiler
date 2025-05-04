/**
 * @file AST.h
 * @author Yuntao Dai (d1581209858@live.com)
 * @brief 
 * abstract syntax tree
 * there is a basic class AstNode, 
 * and for every non-terminal lexical unit, we create a sub-class for it,
 * sub-class should implement the IR generating function for itself
 * @version 0.1
 * @date 2022-12-19
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef AST_H
#define AST_H

#include"front/token.h"
#include"json/json.h"
#include"ir/ir.h"
using ir::Type;

#include<vector>
#include<string>
using std::vector;
using std::string;

namespace frontend {

// enumerate for node type
  enum class NodeType {
    TERMINAL,       // 终结符（词法单元）
    COMPUINT,       // 编译单元（整个程序）
    DECL,           // 声明（包含常量/变量/函数声明）
    FUNCDEF,        // 函数定义
    CONSTDECL,      // 常量声明
    BTYPE,          // 基本类型（int/float等）
    CONSTDEF,       // 常量定义
    CONSTINITVAL,   // 常量初值
    VARDECL,        // 变量声明
    VARDEF,         // 变量定义
    INITVAL,        // 变量初值
    FUNCTYPE,       // 函数返回类型
    FUNCFPARAM,     // 函数形参（单个参数）
    FUNCFPARAMS,    // 函数形参列表（多个参数）
    BLOCK,          // 语句块（{...}）
    BLOCKITEM,      // 语句块项（声明或语句）
    STMT,           // 语句
    EXP,            // 表达式
    COND,           // 条件表达式
    LVAL,           // 左值（可被赋值的表达式）
    NUMBER,         // 数字字面量
    PRIMARYEXP,     // 基本表达式（字面量/变量/括号表达式）
    UNARYEXP,       // 一元表达式
    UNARYOP,        // 一元运算符
    FUNCRPARAMS,    // 函数实参列表
    MULEXP,         // 乘法表达式（*/%）
    ADDEXP,         // 加法表达式（+-）
    RELEXP,         // 关系表达式（<> <= >=）
    EQEXP,          // 相等性表达式（== !=）
    LANDEXP,        // 逻辑与表达式（&&）
    LOREXP,         // 逻辑或表达式（||）
    CONSTEXP,       // 常量表达式
};
std::string toString(NodeType);

// tree node basic class
struct AstNode{
    NodeType type;  // the node type
    AstNode* parent;    // the parent node
    vector<AstNode*> children;     // children of node

    /**
     * @brief constructor
     */
    AstNode(NodeType t, AstNode* p = nullptr);

    /**
     * @brief destructor
     */
    virtual ~AstNode();

    /**
     * @brief Get the json output object
     * @param root: a Json::Value buffer, should be initialized before calling this function
     */
    void get_json_output(Json::Value& root) const;

    // rejcet copy and assignment
    AstNode(const AstNode&) = delete;
    AstNode& operator=(const AstNode&) = delete;
};

struct Term: AstNode {
    Token token;

    /**
     * @brief constructor
     */
    Term(Token t, AstNode* p = nullptr);
};


struct CompUnit: AstNode {
    /**
     * @brief constructor
     */
    CompUnit(AstNode* p = nullptr);
};

struct Decl: AstNode{
    /**
     * @brief constructor
     */
    Decl(AstNode* p = nullptr);
};

struct FuncDef: AstNode{
    string n;
    Type t;
    
    /**
     * @brief constructor
     */
    FuncDef(AstNode* p = nullptr);
};

struct ConstDecl: AstNode {
    Type t;

    /**
     * @brief constructor
     */
    ConstDecl(AstNode* p = nullptr);        
};

struct BType: AstNode {
    Type t;

    /**
     * @brief constructor
     */
    BType(AstNode* p = nullptr);
};

struct ConstDef: AstNode{
    /**
     * @brief constructor
     */
    ConstDef(AstNode* p = nullptr);
};

struct ConstInitVal: AstNode{
    /**
     * @brief constructor
     */
    ConstInitVal(AstNode* p = nullptr);
};

struct VarDecl: AstNode{
    /**
     * @brief constructor
     */
    VarDecl(AstNode* p = nullptr);
};

struct VarDef: AstNode{
    /**
     * @brief constructor
     */
    VarDef(AstNode* p = nullptr);
};

struct InitVal: AstNode{
    /**
     * @brief constructor
     */
    InitVal(AstNode* p = nullptr);
};

struct FuncType: AstNode{
    /**
     * @brief constructor
     */
    FuncType(AstNode* p = nullptr);
};

struct FuncFParam: AstNode{
    /**
     * @brief constructor
     */
    FuncFParam(AstNode* p = nullptr);
};

struct FuncFParams: AstNode{
    /**
     * @brief constructor
     */
    FuncFParams(AstNode* p = nullptr);
};

struct Block: AstNode{
    /**
     * @brief constructor
     */
    Block(AstNode* p = nullptr);
};

struct BlockItem: AstNode{
    /**
     * @brief constructor
     */
    BlockItem(AstNode* p = nullptr);
};

struct Stmt: AstNode{
    /**
     * @brief constructor
     */
    Stmt(AstNode* p = nullptr);
};

struct Exp: AstNode{
    /**
     * @brief constructor
     */
    Exp(AstNode* p = nullptr);
};

struct Cond: AstNode{
    /**
     * @brief constructor
     */
    Cond(AstNode* p = nullptr);
};

struct LVal: AstNode{
    /**
     * @brief constructor
     */
    LVal(AstNode* p = nullptr);
};

struct Number: AstNode{
  union {
    int i;
    float f;
  } v;  // 用于存储数值
  Type t;  // 用于存储类型（Int或Float）
    /**
     * @brief constructor
     */
    Number(AstNode* p = nullptr);
};

struct PrimaryExp: AstNode{
    /**
     * @brief constructor
     */
    PrimaryExp(AstNode* p = nullptr);
};

struct UnaryExp: AstNode{
    /**
     * @brief constructor
     */
    UnaryExp(AstNode* p = nullptr);
};

struct UnaryOp: AstNode{
    /**
     * @brief constructor
     */
    UnaryOp(AstNode* p = nullptr);
};

struct FuncRParams: AstNode{
    /**
     * @brief constructor
     */
    FuncRParams(AstNode* p = nullptr);
};

struct MulExp: AstNode{
    /**
     * @brief constructor
     */
    MulExp(AstNode* p = nullptr);
};

struct AddExp: AstNode{
    /**
     * @brief constructor
     */
    AddExp(AstNode* p = nullptr);
};

struct RelExp: AstNode{
    /**
     * @brief constructor
     */
    RelExp(AstNode* p = nullptr);
};

struct EqExp: AstNode{
    /**
     * @brief constructor
     */
    EqExp(AstNode* p = nullptr);
};

struct LAndExp: AstNode{
    /**
     * @brief constructor
     */
    LAndExp(AstNode* p = nullptr);
};

struct LOrExp: AstNode{
    /**
     * @brief constructor
     */
    LOrExp(AstNode* p = nullptr);
};

struct ConstExp: AstNode{
    /**
     * @brief constructor
     */
    ConstExp(AstNode* p = nullptr);
};

} // namespace frontend

#endif