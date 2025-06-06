/**
 * @file semantic.h
 * @author Yuntao Dai (d1581209858@live.com)
 * @brief 
 * @version 0.1
 * @date 2023-01-06
 * 
 * a Analyzer should 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include"ir/ir.h"
#include"front/abstract_syntax_tree.h"

#include<map>
#include<string>
#include<vector>
using std::map;
using std::string;
using std::vector;

namespace frontend {
  // definition of symbol table entry
  struct STE {
    ir::Operand operand;
    vector<int> dimension;
  };

  using map_str_ste = map<string, STE>;

  // definition of scope infomation
  struct ScopeInfo {
    int cnt;
    string name;
    map_str_ste table;
  };

  // surpport lib functions
  map<std::string, ir::Function *> *get_lib_funcs();

  // definition of symbol table
  struct SymbolTable {
    int scope_cnt;
    vector<ScopeInfo> scope_stack;
    map<std::string, ir::Function *> functions;

    /**
     * @brief enter a new scope, record the infomation in scope stacks
     * @param node: a Block node, entering a new Block means a new name scope
     */
    void add_scope(Block *);

    /**
     * @brief exit a scope, pop out infomations
     */
    void exit_scope();

    /**
     * @brief Get the scoped name, to deal the same name in different scopes, we change origin id to a new one with scope infomation,
     * for example, we have these code:
     * "
     * int a;
     * {
     *      int a; ....
     * }
     * "
     * in this case, we have two variable both name 'a', after change they will be 'a' and 'a_block'
     * @param id: origin id
     * @return string: new name with scope infomations
     */
    string get_scoped_name(string id) const;

    /**
     * @brief get the right operand with the input name
     * @param id identifier name
     * @return Operand
     */
    ir::Operand get_operand(string id) const;

    /**
     * @brief get the right ste with the input name
     * @param id identifier name
     * @return STE
     */
    STE get_ste(string id) const;
  };


  struct Analyzer {
    int tmp_cnt;
    vector<ir::Instruction *> g_init_inst;
    SymbolTable symbol_table;

    // 支持嵌套break和continue
    std::stack<std::vector<int>> breakJumpsStack;  // 每个循环一个break跳转列表
    std::stack<std::vector<int>> continueJumpsStack; // 每个循环一个continue跳转列表
    std::stack<int> continueTargets; // continue目标位置（条件判断处）
    std::stack<int> breakTargets;    // break目标位置（循环结束处）

    // 用于返回类型转换
    std::stack<ir::Type> returnTypeStack;

    // 用于直接计算const值时获取变量的值
    enum const_num_type {INT, FLOAT};
    struct const_num {
      const_num_type type;
      int i_val;
      float f_val;
    };
    map<string, const_num> const_vars;

    /**
     * @brief constructor
     */
    Analyzer();

    // analysis functions
    ir::Program get_ir_program(CompUnit *);

    int evalConstExp(AstNode *node);

    float evalFloatConstExp(AstNode *node);

    // analysis functions
    void analysisCompUnit(CompUnit *, ir::Program &);

    void analysisDecl(Decl *, vector<ir::Instruction *> &);

    void analysisConstDecl(ConstDecl *, vector<ir::Instruction *> &);

    void analysisVarDecl(VarDecl *, vector<ir::Instruction *> &);

    void analysisConstDef(ConstDef *, ir::Type, vector<ir::Instruction *> &);

    void analysisVarDef(VarDef *, ir::Type, vector<ir::Instruction *> &);

    void analysisFuncDef(FuncDef *, ir::Function *);

    void analysisFuncFParams(FuncFParams *, ir::Function *);

    void analysisFuncFParam(FuncFParam *, ir::Function *);

    void analysisBlock(Block *, vector<ir::Instruction *> &, bool);

    void analysisBlockItem(BlockItem *, vector<ir::Instruction *> &);

    void analysisStmt(Stmt *, vector<ir::Instruction *> &);

    void analysisCond(Cond *, vector<ir::Instruction *> &);

    Type analysisBType(BType *);

    Type analysisFuncType(FuncType *);

    void analysisConstInitVal(ConstInitVal*, ir::Type, const ir::Operand&, int, vector<ir::Instruction*>&);

    void analysisInitVal(InitVal*, ir::Type, const ir::Operand&, int, vector<ir::Instruction*>&);

    void analysisConstExp(ConstExp *, vector<ir::Instruction *> &);

    void analysisAddExp(AddExp *, vector<ir::Instruction *> &);

    void analysisMulExp(MulExp *, vector<ir::Instruction *> &);

    void analysisUnaryExp(UnaryExp *, vector<ir::Instruction *> &);

    void
    analysisFuncRParams(FuncRParams *, vector<ir::Instruction *> &, ir::Function *, vector<ir::Operand> &);

    void analysisPrimaryExp(PrimaryExp *, vector<ir::Instruction *> &);

    void analysisExp(Exp *, vector<ir::Instruction *> &);

    void analysisLVal(LVal *, vector<ir::Instruction *> &, bool);

    void analysisNumber(Number *);

    void analysisLOrExp(LOrExp *, vector<ir::Instruction *> &);

    void analysisLAndExp(LAndExp *, vector<ir::Instruction *> &);

    void analysisEqExp(EqExp *, vector<ir::Instruction *> &);

    void analysisRelExp(RelExp *, vector<ir::Instruction *> &);

    // reject copy & assignment
    Analyzer(const Analyzer &) = delete;

    Analyzer &operator=(const Analyzer &) = delete;
  };
} // namespace frontend

#endif
