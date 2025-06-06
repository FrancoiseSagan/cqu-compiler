#include"front/semantic.h"

#include<cassert>
#include<iostream>

using ir::Instruction;
using ir::Function;
using ir::Operand;
using ir::Operator;

// float转string
std::string float_to_string_lossless(float f) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.9g", f);
  return buffer;
}

// 打印 IR
void DrawInstructions(const std::vector<ir::Instruction *>& inst_ptrs) {
  for (auto &inst: inst_ptrs) {
    std::cout << inst->draw() << std::endl;
  }
}

// 用于生成唯一的作用域名称
#define SCOPE_NAME(type) (std::string(type) + "_" + std::to_string(scope_cnt++))

// 用于获取子节点的宏
#define GET_CHILD_PTR(node, type, index) auto node = dynamic_cast<type*>(root->children[index]); assert(node);

// 用于检查变量是否已声明的宏
#define CHECK_DECL(id) assert(symbol_table.get_ste(id).operand.name != "null" && "Variable not declared")

// 用于生成临时变量名的宏
#define GEN_TEMP_VAR (std::string("_t") + std::to_string(tmp_cnt++))

map<std::string, ir::Function *> *frontend::get_lib_funcs() {
  static map<std::string, ir::Function *> lib_funcs = {
    {"getint", new Function("getint", Type::Int)},
    {"getch", new Function("getch", Type::Int)},
    {"getfloat", new Function("getfloat", Type::Float)},
    {"getarray", new Function("getarray", {Operand("arr", Type::IntPtr)}, Type::Int)},
    {"getfarray", new Function("getfarray", {Operand("arr", Type::FloatPtr)}, Type::Int)},
    {"putint", new Function("putint", {Operand("i", Type::Int)}, Type::null)},
    {"putch", new Function("putch", {Operand("i", Type::Int)}, Type::null)},
    {"putfloat", new Function("putfloat", {Operand("f", Type::Float)}, Type::null)},
    {"putarray", new Function("putarray", {Operand("n", Type::Int), Operand("arr", Type::IntPtr)}, Type::null)},
    {"putfarray", new Function("putfarray", {Operand("n", Type::Int), Operand("arr", Type::FloatPtr)}, Type::null)},
  };
  return &lib_funcs;
}

void frontend::SymbolTable::add_scope(Block *node) {
  // 创建新的作用域信息
  ScopeInfo scope;
  scope.cnt = scope_cnt;

  // 根据Block节点的父节点类型确定作用域类型
  if (node && node->parent) {
    if (dynamic_cast<FuncDef *>(node->parent)) {
      scope.name = SCOPE_NAME("function");
    } else if (dynamic_cast<Stmt *>(node->parent)) {
      auto stmt = dynamic_cast<Stmt *>(node->parent);
      // 根据语句类型设置作用域名称
      if (!stmt->children.empty()) {
        auto first_token = dynamic_cast<Term *>(stmt->children[0]);
        if (first_token) {
          if (first_token->token.type == TokenType::WHILETK) {
            scope.name = SCOPE_NAME("while");
          } else if (first_token->token.type == TokenType::IFTK) {
            scope.name = SCOPE_NAME("if");
          } else {
            scope.name = SCOPE_NAME("block");
          }
        }
      }
    } else {
      scope.name = SCOPE_NAME("block");
    }
  } else {
    scope.name = SCOPE_NAME("block");
  }

  scope_stack.push_back(scope);
}

void frontend::SymbolTable::exit_scope() {
  if (!scope_stack.empty()) {
    scope_stack.pop_back();
  }
}

string frontend::SymbolTable::get_scoped_name(string id) const {
  if (scope_stack.empty()) {
    return id;
  }
  return id + "_" + scope_stack.back().name;
}

Operand frontend::SymbolTable::get_operand(string id) const {
  // 从内层作用域向外层查找
  for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it) {
    auto find = it->table.find(id);
    if (find != it->table.end()) {
      return find->second.operand;
    }
  }
  // 如果是函数，从函数表中查找
  auto func_it = functions.find(id);
  if (func_it != functions.end()) {
    return Operand(id, func_it->second->returnType);
  }
  return Operand();
}

frontend::STE frontend::SymbolTable::get_ste(string id) const {
  // 从内层作用域向外层查找
  for (auto it = scope_stack.rbegin(); it != scope_stack.rend(); ++it) {
    auto find = it->table.find(id);
    if (find != it->table.end()) {
      return find->second;
    }
  }
  return STE{Operand(), vector<int>()};
}

frontend::Analyzer::Analyzer(): tmp_cnt(0), symbol_table() {
  // 初始化全局作用域
  symbol_table.scope_cnt = 1;

  // 创建全局初始化函数
  auto *global_func = new Function("global", Type::null);
  symbol_table.functions["global"] = global_func;

  // 添加全局作用域
  ScopeInfo global_scope;
  global_scope.cnt = 0;
  global_scope.name = "global";
  symbol_table.scope_stack.push_back(global_scope);

  // 添加库函数
  auto lib_funcs = get_lib_funcs();
  for (const auto &pair: *lib_funcs) {
    symbol_table.functions[pair.first] = pair.second;
  }
}

ir::Program frontend::Analyzer::get_ir_program(CompUnit *root) {
  ir::Program program;
  analysisCompUnit(root, program);
  return program;
}

void frontend::Analyzer::analysisCompUnit(CompUnit *root, ir::Program &program) {
  // CompUnit -> FuncDef CompUnit | Decl CompUnit | ε
  if (program.functions.empty()) {
    // 添加 global 函数
    auto *global_func = symbol_table.functions["global"];
    program.addFunction(*global_func);
  }

  if (Decl *node = dynamic_cast<Decl *>(root->children[0])) {
    vector<Instruction *> decl_insts;
    analysisDecl(node, decl_insts);

    // 直接操作 global 函数
    ir::Function &global_func = program.functions[0];
    for (auto &inst: decl_insts) {
      if (inst->op == ir::Operator::def || inst->op == ir::Operator::fdef) {
        if (inst->des.name.find("_t") != 0) {
          // 不是临时变量
          if (inst->des.name.find("_global") == string::npos) {
            inst->des.name += "_global";
          }
          program.globalVal.push_back(ir::GlobalVal(inst->des));
          global_func.addInst(inst); // 直接添加到 program
        }
      } else if (inst->op == ir::Operator::alloc) {
        if (inst->des.name.find("_t") != 0) {
          if (inst->des.name.find("_global") == string::npos) {
            inst->des.name += "_global";
          }
          program.globalVal.push_back(ir::GlobalVal(inst->des, std::stoi(inst->op1.name)));
          global_func.addInst(inst); // 直接添加到 program
        }
      } else if (inst->op == ir::Operator::store) {
        global_func.addInst(inst); // 直接添加到 program
      }
    }
  } else if (auto *node = dynamic_cast<FuncDef *>(root->children[0])) {
    // 确保 global 函数有 return 指令
    ir::Function &global_func = program.functions[0];
    if (global_func.InstVec.empty() || global_func.InstVec.back()->op != ir::Operator::_return) {
      global_func.addInst(new ir::Instruction(
        ir::Operand(),
        ir::Operand(),
        ir::Operand(),
        ir::Operator::_return
      ));
    }

    // 处理普通函数
    auto *new_func = new Function();
    analysisFuncDef(node, new_func);
    program.addFunction(*new_func);
  }

  if (root->children.size() == 2) {
    GET_CHILD_PTR(child_comp, CompUnit, 1);
    analysisCompUnit(child_comp, program);
  }
}

void frontend::Analyzer::analysisDecl(Decl *root, vector<ir::Instruction *> &instructions) {
  // Decl -> ConstDecl | VarDecl
  if (auto node = dynamic_cast<ConstDecl *>(root->children[0])) {
    analysisConstDecl(node, instructions);
  } else if (auto node = dynamic_cast<VarDecl *>(root->children[0])) {
    analysisVarDecl(node, instructions);
  }
}

void frontend::Analyzer::analysisConstDecl(ConstDecl *root, vector<ir::Instruction *> &instructions) {
  // ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
  GET_CHILD_PTR(btype, BType, 1);
  Type type = analysisBType(btype);

  // 处理所有的ConstDef
  for (int i = 2; i < root->children.size() - 1; i += 2) {
    // 跳过逗号
    GET_CHILD_PTR(const_def, ConstDef, i);
    analysisConstDef(const_def, type, instructions);
  }
}

Type frontend::Analyzer::analysisBType(BType *root) {
  // BType -> 'int' | 'float'
  GET_CHILD_PTR(type_term, Term, 0);
  if (type_term->token.type == TokenType::INTTK) {
    return Type::Int;
  } else {
    return Type::Float;
  }
}

Type frontend::Analyzer::analysisFuncType(FuncType *root) {
  // FuncType -> 'void' | 'int' | 'float'
  GET_CHILD_PTR(type_term, Term, 0);
  if (type_term->token.type == TokenType::VOIDTK) {
    return Type::null;
  } else if (type_term->token.type == TokenType::INTTK) {
    return Type::Int;
  } else {
    return Type::Float;
  }
}

void frontend::Analyzer::analysisVarDecl(VarDecl *root, vector<ir::Instruction *> &instructions) {
  // VarDecl -> BType VarDef { ',' VarDef } ';'
  GET_CHILD_PTR(btype, BType, 0);
  Type type = analysisBType(btype);

  // 处理所有的VarDef
  for (int i = 1; i < root->children.size() - 1; i += 2) {
    // 跳过逗号
    GET_CHILD_PTR(var_def, VarDef, i);
    analysisVarDef(var_def, type, instructions);
  }
}

// 注：这里的做法不太标准，有初始值的数组初始化是在analysisInitVal做的，0初始化和变量初始化是在这两个函数做的
void frontend::Analyzer::analysisConstDef(ConstDef *root, ir::Type type, vector<ir::Instruction *> &instructions) {
  // ConstDef -> Ident { '[' ConstExp ']' } [ '=' ConstInitVal ]
  GET_CHILD_PTR(id_term, Term, 0);
  string id = id_term->token.value;

  // 检查是否在全局作用域
  bool is_global = symbol_table.scope_stack.back().name == "global";
  string scoped_name = is_global ? id + "_global" : symbol_table.get_scoped_name(id);

  // 检查是否是数组并获取所有维度
  vector<int> shape;
  int init_val_pos = -1;
  for (int i = 1; i < root->children.size(); ++i) {
    if (dynamic_cast<Term *>(root->children[i]) && dynamic_cast<Term *>(root->children[i])->token.type ==
        TokenType::LBRACK) {
      GET_CHILD_PTR(const_exp, ConstExp, i + 1);
      vector<ir::Instruction *> temp_insts;
      analysisConstExp(const_exp, temp_insts);
      shape.push_back(std::stoi(temp_insts.back()->op1.name));
      i += 2; // 跳过逗号
    } else if (root->children[i]->type == NodeType::CONSTINITVAL) {
      init_val_pos = i;
      break;
    }
  }

  if (shape.empty()) {
    // 标量变量
    ir::Operand op(scoped_name, type);
    symbol_table.scope_stack.back().table[id] = STE{op, vector<int>()};

    if (init_val_pos != -1) {
      // 有初始值
      GET_CHILD_PTR(const_init_val, ConstInitVal, init_val_pos);

      // 直接计算constexpr的值，注意我只直接计算了全局变量的值以及非全局变量的数组长度
      if (is_global) {
        if (type == Type::Float) {
          auto value = evalFloatConstExp(const_init_val->children[0]);
          instructions.push_back(new ir::Instruction(
            ir::Operand(float_to_string_lossless(value), Type::FloatLiteral),
            ir::Operand(),
            op,
            Operator::fdef
          ));
          auto f_str = float_to_string_lossless(value);
          const_vars[op.name].type = FLOAT;
          const_vars[op.name].f_val = value;
        } else {
          auto value = evalConstExp(const_init_val->children[0]);
          instructions.push_back(new ir::Instruction(
            ir::Operand(std::to_string(value), Type::IntLiteral),
            ir::Operand(),
            op,
            Operator::def
          ));
          const_vars[op.name].type = INT;
          const_vars[op.name].i_val = value;
        }

        return;
      }

      vector<ir::Instruction *> init_insts;
      analysisConstInitVal(const_init_val, type, op, -1, init_insts);

      // 对字面量直接生成def指令
      if (init_insts.back()->op == Operator::def || init_insts.back()->op == Operator::fdef) {
        if (init_insts.back()->des.type != type) {
          // 需要检查浮点数和整数的转换
          string temp_name = GEN_TEMP_VAR;
          ir::Operand temp_op(temp_name, type);

          instructions.push_back(init_insts.back());

          if (init_insts.back()->des.type == Type::Int && type == Type::Float) {
            instructions.push_back(new ir::Instruction(
              init_insts.back()->des,
              ir::Operand(),
              temp_op,
              Operator::cvt_i2f
            ));
          } else if (init_insts.back()->des.type == Type::Float && type == Type::Int) {
            instructions.push_back(new ir::Instruction(
              init_insts.back()->des,
              ir::Operand(),
              temp_op,
              Operator::cvt_f2i
            ));
          }

          instructions.push_back(new ir::Instruction(
            ir::Operand("0", type == Type::Int ? Type::IntLiteral : Type::FloatLiteral),
            ir::Operand(),
            op,
            type == Type::Int ? Operator::def : Operator::fdef
          ));
          instructions.push_back(new ir::Instruction(
            temp_op,
            ir::Operand(),
            op,
            type == Type::Int ? Operator::mov : Operator::fmov
          ));

          const_vars[op.name].type = type == Type::Int ? INT : FLOAT;
          if (type == Type::Int) {
            const_vars[op.name].i_val = stoi(temp_op.name);
          } else {
            const_vars[op.name].f_val = stof(temp_op.name);
          }
        } else {
          instructions.push_back(new ir::Instruction(
            init_insts.back()->op1,
            ir::Operand(),
            op,
            type == Type::Int ? ir::Operator::def : ir::Operator::fdef
          ));

          const_vars[op.name].type = type == Type::Int ? INT : FLOAT;
          if (type == Type::Int) {
            const_vars[op.name].i_val = stoi(init_insts.back()->op1.name);
          } else {
            const_vars[op.name].f_val = stof(init_insts.back()->op1.name);
          }

        }
      } else {
        // 如果不是字面量，需要先计算值
        string value = init_insts.back()->des.name;
        auto value_type = init_insts.back()->des.type;
        instructions.insert(instructions.end(), init_insts.begin(), init_insts.end());
        if (value_type != type) {
          string temp_name = GEN_TEMP_VAR;
          ir::Operand temp_op(temp_name, type);

          // 检查转换
          if (value_type == Type::Int && type == Type::Float) {
            instructions.push_back(new ir::Instruction(
              ir::Operand(value, value_type),
              ir::Operand(),
              temp_op,
              Operator::cvt_i2f
            ));
          } else if (value_type == Type::Float && type == Type::Int) {
            instructions.push_back(new ir::Instruction(
              ir::Operand(value, value_type),
              ir::Operand(),
              temp_op,
              Operator::cvt_f2i
            ));
          }

          instructions.push_back(new ir::Instruction(
            ir::Operand("0", type == Type::Int ? Type::IntLiteral : Type::FloatLiteral),
            ir::Operand(),
            op,
            type == Type::Int ? Operator::def : Operator::fdef
          ));
          instructions.push_back(new ir::Instruction(
            temp_op,
            ir::Operand(),
            op,
            type == Type::Int ? Operator::mov : Operator::fmov
          ));
        } else {
          instructions.push_back(new ir::Instruction(
            ir::Operand("0", type == Type::Int ? Type::IntLiteral : Type::FloatLiteral),
            ir::Operand(),
            op,
            type == Type::Int ? ir::Operator::def : ir::Operator::fdef
          ));
          instructions.push_back(new ir::Instruction(
            ir::Operand(value, value_type),
            ir::Operand(),
            op,
            type == Type::Int ? Operator::mov : Operator::fmov
          ));
        }
      }
    } else {
      // 无初始值，使用默认值0
      instructions.push_back(new ir::Instruction(
        ir::Operand("0", type == Type::Int ? Type::IntLiteral : Type::FloatLiteral),
        ir::Operand(),
        op,
        type == Type::Int ? ir::Operator::def : ir::Operator::fdef
      ));
    }
  } else {
    // 数组变量
    ir::Operand op(scoped_name, type == Type::Int ? Type::IntPtr : Type::FloatPtr);
    symbol_table.scope_stack.back().table[id] = STE{op, shape};

    // 计算多维数组的总长度
    int total_size = 1;
    for (int dim: shape) {
      total_size *= dim;
    }

    instructions.push_back(new ir::Instruction(
      ir::Operand(std::to_string(total_size), Type::IntLiteral),
      ir::Operand(),
      op,
      ir::Operator::alloc
    ));

    if (init_val_pos != -1) {
      GET_CHILD_PTR(const_init_val, ConstInitVal, init_val_pos);
      analysisConstInitVal(const_init_val, type, op, 0, instructions);
      // int init_val_len = ((int) const_init_val->children.size() - 1) / 2;
      // if (init_val_len < total_size) {
      //   ir::Operand zero("0", type == Type::Int ? Type::IntLiteral : Type::FloatLiteral);
      //   for (int i = init_val_len; i < total_size; i++) {
      //     ir::Operand index(std::to_string(i), Type::IntLiteral);
      //     instructions.push_back(new ir::Instruction(
      //       op,
      //       index,
      //       zero,
      //       Operator::store
      //     ));
      //   }
      // }
    } else {
      // ir::Operand zero("0", type == Type::Int ? Type::IntLiteral : Type::FloatLiteral);
      // for (int i = 0; i < total_size; i++) {
      //   ir::Operand index(std::to_string(i), Type::IntLiteral);
      //   instructions.push_back(new ir::Instruction(
      //     op,
      //     index,
      //     zero,
      //     Operator::store
      //   ));
      // }
    }
  }
}

void frontend::Analyzer::analysisVarDef(VarDef *root, ir::Type type, vector<ir::Instruction *> &instructions) {
  // VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
  GET_CHILD_PTR(id_term, Term, 0);
  string id = id_term->token.value;

  // 检查是否在全局作用域
  bool is_global = symbol_table.scope_stack.back().name == "global";
  string scoped_name = is_global ? id + "_global" : symbol_table.get_scoped_name(id);

  // 检查是否是数组并获取所有维度
  vector<int> shape;
  int init_val_pos = -1;
  for (int i = 1; i < root->children.size(); ++i) {
    if (dynamic_cast<Term *>(root->children[i]) && dynamic_cast<Term *>(root->children[i])->token.type ==
        TokenType::LBRACK) {
      GET_CHILD_PTR(const_exp, ConstExp, i + 1);
      vector<ir::Instruction *> temp_insts;
      analysisConstExp(const_exp, temp_insts);

      auto sp = temp_insts.back()->op == Operator::def
                  ? std::stoi(temp_insts.back()->op1.name)
                  : evalConstExp(const_exp);
      shape.push_back(sp);
      i += 2;
        } else if (root->children[i]->type == NodeType::INITVAL) {
          init_val_pos = i;
          break;
        }
  }

  if (shape.empty()) {
    // 标量变量
    ir::Operand op(scoped_name, type);
    symbol_table.scope_stack.back().table[id] = STE{op, vector<int>()};

    if (init_val_pos != -1) {
      // 有初始值
      GET_CHILD_PTR(init_val, InitVal, init_val_pos);
      vector<ir::Instruction *> init_insts;
      analysisInitVal(init_val, type, op, -1, init_insts);

      // 直接生成def指令
      if (init_insts.back()->op == Operator::def || init_insts.back()->op == Operator::fdef) {
        if (init_insts.back()->des.type != type) {
          // 转换
          string temp_name = GEN_TEMP_VAR;
          ir::Operand temp_op(temp_name, type);

          instructions.push_back(init_insts.back());

          // Then add conversion instruction
          if (init_insts.back()->des.type == Type::Int && type == Type::Float) {
            instructions.push_back(new ir::Instruction(
              init_insts.back()->des,
              ir::Operand(),
              temp_op,
              Operator::cvt_i2f
            ));
          } else if (init_insts.back()->des.type == Type::Float && type == Type::Int) {
            instructions.push_back(new ir::Instruction(
              init_insts.back()->des,
              ir::Operand(),
              temp_op,
              Operator::cvt_f2i
            ));
          }

          instructions.push_back(new ir::Instruction(
            temp_op,
            ir::Operand(),
            op,
            type == Type::Int ? Operator::mov : Operator::fmov
          ));
        } else {
          instructions.push_back(new ir::Instruction(
            init_insts.back()->op1,
            ir::Operand(),
            op,
            type == Type::Int ? ir::Operator::def : ir::Operator::fdef
          ));
        }
      } else {
        // 如果不是字面量，需要先计算值
        string value = init_insts.back()->des.name;
        auto value_type = init_insts.back()->des.type;
        instructions.insert(instructions.end(), init_insts.begin(), init_insts.end());
        if (value_type != type) {
          string temp_name = GEN_TEMP_VAR;
          ir::Operand temp_op(temp_name, type);

          if (value_type == Type::Int && type == Type::Float) {
            instructions.push_back(new ir::Instruction(
              ir::Operand(value, value_type),
              ir::Operand(),
              temp_op,
              Operator::cvt_i2f
            ));
          } else if (value_type == Type::Float && type == Type::Int) {
            instructions.push_back(new ir::Instruction(
              ir::Operand(value, value_type),
              ir::Operand(),
              temp_op,
              Operator::cvt_f2i
            ));
          }

          instructions.push_back(new ir::Instruction(
            temp_op,
            ir::Operand(),
            op,
            type == Type::Int ? Operator::mov : Operator::fmov
          ));
        } else {
          instructions.push_back(new ir::Instruction(
            ir::Operand("0", type == Type::Int ? Type::IntLiteral : Type::FloatLiteral),
            ir::Operand(),
            op,
            type == Type::Int ? ir::Operator::def : ir::Operator::fdef
          ));
          instructions.push_back(new ir::Instruction(
            ir::Operand(value, value_type),
            ir::Operand(),
            op,
            type == Type::Int ? Operator::mov : Operator::fmov
          ));
        }
      }
    } else {
      // 无初始值，使用默认值0
      instructions.push_back(new ir::Instruction(
        ir::Operand("0", type == Type::Int ? Type::IntLiteral : Type::FloatLiteral),
        ir::Operand(),
        op,
        type == Type::Int ? ir::Operator::def : ir::Operator::fdef
      ));
    }
  } else {
    // 数组变量
    ir::Operand op(scoped_name, type == Type::Int ? Type::IntPtr : Type::FloatPtr);
    symbol_table.scope_stack.back().table[id] = STE{op, shape};

    int total_size = 1;
    for (int dim: shape) {
      total_size *= dim;
    }

    instructions.push_back(new ir::Instruction(
      ir::Operand(std::to_string(total_size), Type::IntLiteral),
      ir::Operand(),
      op,
      ir::Operator::alloc
    ));

    if (init_val_pos != -1) {
      GET_CHILD_PTR(init_val, InitVal, init_val_pos);
      analysisInitVal(init_val, type, op, 0, instructions);
      // int init_val_len = ((int) init_val->children.size() - 1) / 2;
      // if (init_val_len < total_size) {
      //   ir::Operand zero("0", type == Type::Int ? Type::IntLiteral : Type::FloatLiteral);
      //   for (int i = init_val_len; i < total_size; i++) {
      //     ir::Operand index(std::to_string(i), Type::IntLiteral);
      //     instructions.push_back(new ir::Instruction(
      //       op,
      //       index,
      //       zero,
      //       Operator::store
      //     ));
      //   }
      // }
    } else {
      // ir::Operand zero("0", type == Type::Int ? Type::IntLiteral : Type::FloatLiteral);
      // for (int i = 0; i < total_size; i++) {
      //   ir::Operand index(std::to_string(i), Type::IntLiteral);
      //   instructions.push_back(new ir::Instruction(
      //     op,
      //     index,
      //     zero,
      //     Operator::store
      //   ));
      // }
    }
  }
}

void frontend::Analyzer::analysisInitVal(
  InitVal *root,
  ir::Type type,
  const ir::Operand &arr_base,
  int offset,
  vector<ir::Instruction *> &instructions
) {
  // InitVal -> Exp | '{' [InitVal {',' InitVal}] '}'
  if (root->children.size() == 1) {
    // 单个表达式初始化
    GET_CHILD_PTR(exp, Exp, 0);
    vector<ir::Instruction *> exp_insts;
    analysisExp(exp, exp_insts);

    // 检查是否为字面量
    if (exp_insts.back()->op == Operator::def) {
      if (offset == -1) {
        // 非数组初始化
        instructions.push_back(exp_insts.back());
      } else {
        // 数组初始化，使用 store 指令
        auto value = exp_insts.back()->op1;
        if ((type == Type::Float && value.type == Type::IntLiteral) || (type == Type::Int && value.type == Type::FloatLiteral)) {
          //转换
          string conv_var = GEN_TEMP_VAR;
          if (type == Type::Float) {
            // int转float
            instructions.push_back(new ir::Instruction(
                value,
                ir::Operand(),
                ir::Operand(conv_var, Type::Float),
                Operator::cvt_i2f
            ));
            value = ir::Operand(conv_var, Type::Float);
          }
          else if (type == Type::Int) {
            // float转int
            instructions.push_back(new ir::Instruction(
                value,
                ir::Operand(),
                ir::Operand(conv_var, Type::Int),
                Operator::cvt_f2i
            ));
            value = ir::Operand(conv_var, Type::Int);
          }
        }
        instructions.push_back(new ir::Instruction(
          arr_base, // op1: 数组名
          ir::Operand(std::to_string(offset), Type::IntLiteral), // op2: 下标
          value, // 字面量值
          ir::Operator::store
        ));
      }
    } else {
      // 非字面量，先计算到临时变量
      instructions.insert(instructions.end(), exp_insts.begin(), exp_insts.end());
      if (offset == -1) {
        // 非数组初始化
      } else {
        // 数组初始化，使用 store 指令
        auto value = exp_insts.back()->des;
        if (type != value.type) {
          //转换
          string conv_var = GEN_TEMP_VAR;
          if (type == Type::Float) {
            // int转float
            instructions.push_back(new ir::Instruction(
                value,
                ir::Operand(),
                ir::Operand(conv_var, Type::Float),
                Operator::cvt_i2f
            ));
            value = ir::Operand(conv_var, Type::Float);
          }
          else if (type == Type::Int) {
            // float转int
            instructions.push_back(new ir::Instruction(
                value,
                ir::Operand(),
                ir::Operand(conv_var, Type::Int),
                Operator::cvt_f2i
            ));
            value = ir::Operand(conv_var, Type::Int);
          }
        }
        instructions.push_back(new ir::Instruction(
          arr_base, // op1: 数组名
          ir::Operand(std::to_string(offset), Type::IntLiteral), // op2: 下标
          value, // 临时变量
          ir::Operator::store
        ));
      }
    }
  } else {
    // 初始化列表（如 {1, 2, 3}）
    int current_offset = offset;
    for (int i = 1; i < root->children.size() - 1; i += 2) {
      GET_CHILD_PTR(init_val, InitVal, i);
      analysisInitVal(init_val, type, arr_base, current_offset, instructions);
      current_offset++;
    }
  }
}

void frontend::Analyzer::analysisConstInitVal(
  ConstInitVal *root,
  ir::Type type,
  const ir::Operand &arr_base,
  int offset,
  vector<ir::Instruction *> &instructions
) {
  // ConstInitVal -> ConstExp | '{' [ConstInitVal {',' ConstInitVal}] '}'
  if (root->children.size() == 1) {
    // 单个表达式初始化
    GET_CHILD_PTR(exp, ConstExp, 0);
    vector<ir::Instruction *> exp_insts;
    analysisConstExp(exp, exp_insts);

    // 检查是否是字面量
    if (exp_insts.back()->op == Operator::def) {
      if (offset == -1) {
        // 非数组初始化
        instructions.push_back(exp_insts.back());
      } else {
        // 数组初始化，使用 store 指令
        auto value = exp_insts.back()->op1;
        if ((type == Type::Float && value.type == Type::IntLiteral) || (type == Type::Int && value.type == Type::FloatLiteral)) {
          //转换
          string conv_var = GEN_TEMP_VAR;
          if (type == Type::Float) {
            // int转float
            instructions.push_back(new ir::Instruction(
                value,
                ir::Operand(),
                ir::Operand(conv_var, Type::Float),
                Operator::cvt_i2f
            ));
            value = ir::Operand(conv_var, Type::Float);
          }
          else if (type == Type::Int) {
            // float转int
            instructions.push_back(new ir::Instruction(
                value,
                ir::Operand(),
                ir::Operand(conv_var, Type::Int),
                Operator::cvt_f2i
            ));
            value = ir::Operand(conv_var, Type::Int);
          }
        }
        instructions.push_back(new ir::Instruction(
          arr_base, // op1: 数组名
          ir::Operand(std::to_string(offset), Type::IntLiteral), // op2: 下标
          value, // 字面量值
          ir::Operator::store
        ));
      }
    } else {
      // 非字面量，先计算到临时变量
      instructions.insert(instructions.end(), exp_insts.begin(), exp_insts.end());

      if (offset == -1) {
        // 非数组初始化
      } else {
        // 数组初始化，使用 store 指令
        auto value = exp_insts.back()->des;
        if (type != value.type) {
          //转换
          string conv_var = GEN_TEMP_VAR;
          if (type == Type::Float) {
            // int转float
            instructions.push_back(new ir::Instruction(
                value,
                ir::Operand(),
                ir::Operand(conv_var, Type::Float),
                Operator::cvt_i2f
            ));
            value = ir::Operand(conv_var, Type::Float);
          }
          else if (type == Type::Int) {
            // float转int
            instructions.push_back(new ir::Instruction(
                value,
                ir::Operand(),
                ir::Operand(conv_var, Type::Int),
                Operator::cvt_f2i
            ));
            value = ir::Operand(conv_var, Type::Int);
          }
        }
        instructions.push_back(new ir::Instruction(
          arr_base, // op1: 数组名
          ir::Operand(std::to_string(offset), Type::IntLiteral), // op2: 下标
          value, // 临时变量
          ir::Operator::store
        ));
      }
    }
  } else {
    // 初始化列表（如 {1, 2, 3}）
    int current_offset = offset;
    for (int i = 1; i < root->children.size() - 1; i += 2) {
      GET_CHILD_PTR(const_init_val, ConstInitVal, i);
      analysisConstInitVal(const_init_val, type, arr_base, current_offset, instructions);
      current_offset++;
    }
  }
}

void frontend::Analyzer::analysisFuncDef(FuncDef *root, ir::Function *func) {
  // FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block

  // 分析函数类型
  GET_CHILD_PTR(func_type, FuncType, 0);
  func->returnType = analysisFuncType(func_type);
  returnTypeStack.push(func->returnType);

  // 获取函数名
  GET_CHILD_PTR(id_term, Term, 1);
  func->name = id_term->token.value;

  // 添加到符号表
  symbol_table.functions[func->name] = func;

  GET_CHILD_PTR(block, Block, root->children.size()-1);
  symbol_table.add_scope(block);

  // 分析函数参数(如果有)
  if (root->children[3]->type == NodeType::FUNCFPARAMS) {
    GET_CHILD_PTR(params, FuncFParams, 3);
    analysisFuncFParams(params, func);
  }

  // 分析函数体前，如果是main函数，添加对global函数的调用
  if (func->name == "main") {
    // 生成临时变量存储返回值（即使global无返回值）
    string tmp_var = "_t" + std::to_string(tmp_cnt++);

    // 添加call指令：call tmp_var, global()
    func->addInst(new ir::CallInst(
      ir::Operand("global", ir::Type::null), // 函数名
      ir::Operand(tmp_var, func->returnType) // 临时变量存储返回值
    ));
  }

  // 分析函数体
  vector<ir::Instruction *> body_insts;
  analysisBlock(block, body_insts, false);

  // 添加函数体指令
  for (auto inst: body_insts) {
    func->addInst(inst);
  }

  if (func->returnType == Type::null && func->name != "global") {
    func->addInst(new ir::Instruction(
      Operand(),
      Operand(),
      Operand(),
      Operator::_return
    ));
  }

  symbol_table.exit_scope();
}

void frontend::Analyzer::analysisFuncFParams(FuncFParams *root, ir::Function *func) {
  // FuncFParams -> FuncFParam { ',' FuncFParam }
  for (int i = 0; i < root->children.size(); i += 2) {
    // 跳过逗号
    GET_CHILD_PTR(param, FuncFParam, i);
    analysisFuncFParam(param, func);
  }
}

void frontend::Analyzer::analysisFuncFParam(FuncFParam *root, ir::Function *func) {
  // FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
  GET_CHILD_PTR(btype, BType, 0);
  GET_CHILD_PTR(id_term, Term, 1);

  Type type = analysisBType(btype);
  string id = id_term->token.value;

  // 检查是否是数组参数
  if (root->children.size() > 2) {
    type = (type == Type::Int) ? Type::IntPtr : Type::FloatPtr;
  }

  // 创建参数操作数
  ir::Operand param_op(id, type);
  func->ParameterList.push_back(param_op);

  // 添加到当前作用域的符号表
  symbol_table.scope_stack.back().table[id] = STE{param_op, vector<int>()};
}

void frontend::Analyzer::analysisBlock(Block *root, vector<ir::Instruction *> &instructions, bool new_scope) {
  // Block -> '{' { BlockItem } '}'

  // 创建新的作用域
  if (new_scope) {
    symbol_table.add_scope(root);
  }

  // 分析所有BlockItem
  for (int i = 1; i < root->children.size() - 1; i++) {
    // 跳过花括号
    GET_CHILD_PTR(block_item, BlockItem, i);
    analysisBlockItem(block_item, instructions);
  }

  // 退出作用域
  if (new_scope) {
    symbol_table.exit_scope();
  }
}

void frontend::Analyzer::analysisBlockItem(BlockItem *root, vector<ir::Instruction *> &instructions) {
  // BlockItem -> Decl | Stmt
  if (auto node = dynamic_cast<Decl *>(root->children[0])) {
    analysisDecl(node, instructions);
  } else if (auto node = dynamic_cast<Stmt *>(root->children[0])) {
    analysisStmt(node, instructions);
  }
}

void frontend::Analyzer::analysisStmt(Stmt *root, vector<ir::Instruction *> &instructions) {
  // Stmt -> LVal '=' Exp ';'
  //       | Block
  //       | 'if' '(' Cond ')' Stmt [ 'else' Stmt ]
  //       | 'while' '(' Cond ')' Stmt
  //       | 'break' ';'
  //       | 'continue' ';'
  //       | 'return' [Exp] ';'
  //       | [Exp] ';'
  if (root->children.empty()) return;

  auto first = root->children[0];
  if (auto lval = dynamic_cast<LVal *>(first)) {
    // 赋值语句
    vector<ir::Instruction *> lval_insts;
    analysisLVal(lval, lval_insts, true); // true表示作为左值

    GET_CHILD_PTR(exp, Exp, 2);
    vector<ir::Instruction *> exp_insts;
    analysisExp(exp, exp_insts);
    bool is_literal = true;
    bool shortcut = exp_insts.size() == 1 && (exp_insts.back()->op == Operator::def || exp_insts.back()->op ==
                                              Operator::fdef);

    if (!shortcut) {
      is_literal = false;
      instructions.insert(instructions.end(), exp_insts.begin(), exp_insts.end());
    }

    if (lval_insts.back()->op == Operator::getptr) {
      // 数组赋值
      instructions.insert(instructions.end(), lval_insts.begin(), lval_insts.end());
      instructions.pop_back();
      auto arr_name = lval_insts.back()->op1;
      auto arr_offset = lval_insts.back()->op2;
      auto value = is_literal ? exp_insts.back()->op1 : exp_insts.back()->des;
      Type arr_elem_type = Type::Int;
      if (arr_name.type == Type::FloatPtr) {
        arr_elem_type = Type::Float;
      }
      auto tmpGetType = [](ir::Type t) {
        if (t == Type::IntLiteral) return Type::Int;
        return t;
      };

      if (tmpGetType(value.type) != arr_elem_type) {
        string conv_var = GEN_TEMP_VAR;

        if (arr_elem_type == Type::Float) {
          // int转float
          instructions.push_back(new ir::Instruction(
              value,
              ir::Operand(),
              ir::Operand(conv_var, Type::Float),
              Operator::cvt_i2f
          ));
          value = ir::Operand(conv_var, Type::Float);
        }
        else if (arr_elem_type == Type::Int) {
          // float转int
          instructions.push_back(new ir::Instruction(
              value,
              ir::Operand(),
              ir::Operand(conv_var, Type::Int),
              Operator::cvt_f2i
          ));
          value = ir::Operand(conv_var, Type::Int);
        }
        // 其他情况不处理
      }

      instructions.push_back(new ir::Instruction(
        arr_name,
        arr_offset,
        value,
        Operator::store
      ));
      return;
    }

    auto target = lval_insts.back()->op1;
    auto value = is_literal ? exp_insts.back()->op1 : exp_insts.back()->des;
    instructions.push_back(new ir::Instruction(
      value,
      ir::Operand(),
      target,
      target.type == Type::Int ? ir::Operator::mov : ir::Operator::fmov
    ));
  } else if (auto block = dynamic_cast<Block *>(first)) {
    // 语句块
    analysisBlock(block, instructions, true);
  } else if (auto term = dynamic_cast<Term *>(first)) {
    if (term->token.type == TokenType::IFTK) {
      GET_CHILD_PTR(cond, Cond, 2);
      GET_CHILD_PTR(then_stmt, Stmt, 4);

      vector<ir::Instruction *> cond_insts;
      analysisCond(cond, cond_insts);
      auto cond_result = cond_insts.back()->des;
      instructions.insert(instructions.end(), cond_insts.begin(), cond_insts.end());

      // 条件跳转：为真则跳过下一条跳转指令
      auto cond_goto = new ir::Instruction(
        cond_result,
        ir::Operand(),
        ir::Operand("2", Type::IntLiteral),
        ir::Operator::_goto
      );
      instructions.push_back(cond_goto);

      // 无条件跳转：跳转到else或结束（后面回填）
      auto goto_else_or_end = new ir::Instruction(
        ir::Operand(),
        ir::Operand(),
        ir::Operand("-1", Type::IntLiteral),
        ir::Operator::_goto
      );
      instructions.push_back(goto_else_or_end);

      // 记录then块开始位置
      int then_start = instructions.size();

      // 直接分析then语句到主指令序列
      analysisStmt(then_stmt, instructions);
      int then_end = instructions.size();

      // 如果有else，添加跳转到结束的指令（后面回填）
      ir::Instruction *goto_end_after_then = nullptr;
      bool has_else = root->children.size() > 5;
      if (has_else) {
        goto_end_after_then = new ir::Instruction(
          ir::Operand(),
          ir::Operand(),
          ir::Operand("-1", Type::IntLiteral),
          ir::Operator::_goto
        );
        instructions.push_back(goto_end_after_then);
      }

      // 记录else块开始位置
      int else_start = instructions.size();
      if (has_else) {
        GET_CHILD_PTR(else_stmt, Stmt, 6);
        analysisStmt(else_stmt, instructions);
      }
      int end_pos = instructions.size();

      // 回填跳转偏移量
      int else_or_end_offset = else_start - (then_start - 1);
      goto_else_or_end->des = ir::Operand(std::to_string(else_or_end_offset), Type::IntLiteral);

      if (has_else) {
        int end_offset = end_pos - (then_end);
        goto_end_after_then->des = ir::Operand(std::to_string(end_offset), Type::IntLiteral);
      }
    } else if (term->token.type == TokenType::RETURNTK) {
      if (root->children.size() > 2) {
        GET_CHILD_PTR(exp, Exp, 1);
        vector<ir::Instruction *> exp_insts;
        analysisExp(exp, exp_insts);
        instructions.insert(instructions.end(), exp_insts.begin(), exp_insts.end());

        // 获取表达式结果和当前函数返回类型
        ir::Operand ret_value = exp_insts.back()->des;
        Type ret_type = ret_value.type;
        Type func_ret_type = returnTypeStack.top();

        // 处理类型转换
        if (ret_type != func_ret_type) {
          string conv_var = GEN_TEMP_VAR;
          Operator conv_op;

          if (ret_type == Type::Int && func_ret_type == Type::Float) {
            conv_op = Operator::cvt_i2f;
          } else if (ret_type == Type::Float && func_ret_type == Type::Int) {
            conv_op = Operator::cvt_f2i;
          } else {
            // 不支持的转换类型，报错或保持原样
            conv_op = Operator::def;
          }

          if (conv_op == Operator::cvt_i2f || conv_op == Operator::cvt_f2i) {
            // 添加类型转换指令
            instructions.push_back(new ir::Instruction(
              ret_value,
              ir::Operand(),
              ir::Operand(conv_var, func_ret_type),
              conv_op
            ));
            ret_value = ir::Operand(conv_var, func_ret_type);
          }
        }

        // 生成return指令
        instructions.push_back(new ir::Instruction(
          ret_value,
          ir::Operand(),
          ir::Operand(),
          ir::Operator::_return
        ));
      } else {
        instructions.push_back(new ir::Instruction(
          ir::Operand(),
          ir::Operand(),
          ir::Operand(),
          ir::Operator::_return
        ));
      }
    } else if (term->token.type == TokenType::BREAKTK) {
      // 检查是否在循环内部
      if (breakJumpsStack.empty()) {
        assert(false && "fuck");
      }

      // 创建无条件跳转指令（目标位置暂时未知，后面回填）
      int break_jump_pos = instructions.size();
      auto break_jump = new ir::Instruction(
        ir::Operand(), // 第一个操作数为null表示无条件跳转
        ir::Operand(), // 第二个操作数不使用
        ir::Operand("0", Type::IntLiteral), // 目的操作数是跳转偏移量（后面回填）
        ir::Operator::_goto
      );
      instructions.push_back(break_jump);

      // 记录break跳转位置，等待回填
      breakJumpsStack.top().push_back(break_jump_pos);
    } else if (term->token.type == TokenType::CONTINUETK) {
      // 检查是否在循环内部
      if (continueJumpsStack.empty()) {
        assert(false && "fuck");
      }

      // 创建无条件跳转指令（目标位置暂时未知，后面回填）
      int continue_jump_pos = instructions.size();
      auto continue_jump = new ir::Instruction(
        ir::Operand(), // 第一个操作数为null表示无条件跳转
        ir::Operand(), // 第二个操作数不使用
        ir::Operand("0", Type::IntLiteral), // 目的操作数是跳转偏移量（后面回填）
        ir::Operator::_goto
      );
      instructions.push_back(continue_jump);

      // 记录continue跳转位置，等待回填
      continueJumpsStack.top().push_back(continue_jump_pos);
    } else if (term->token.type == TokenType::WHILETK) {
      GET_CHILD_PTR(cond, Cond, 2);
      GET_CHILD_PTR(body_stmt, Stmt, 4);

      // 创建新的跳转上下文
      breakJumpsStack.push(std::vector<int>());
      continueJumpsStack.push(std::vector<int>());

      // 记录循环开始位置（条件判断处）
      int loop_start = instructions.size();
      continueTargets.push(loop_start);

      // 生成条件代码
      vector<ir::Instruction *> cond_insts;
      analysisCond(cond, cond_insts);
      instructions.insert(instructions.end(), cond_insts.begin(), cond_insts.end());
      auto cond_result = cond_insts.back()->des;

      // 第一个goto：条件为假(0)时不跳转，真(非0)时跳转到第二个goto之后
      int first_jump_pos = instructions.size();
      auto first_jump = new ir::Instruction(
        cond_result, // 第一个操作数是条件结果
        ir::Operand(), // 第二个操作数不使用
        ir::Operand("2", Type::IntLiteral), // 跳转到第二个goto之后
        ir::Operator::_goto
      );
      instructions.push_back(first_jump);

      // 第二个goto：无条件跳转到循环结束处（后面回填）
      int second_jump_pos = instructions.size();
      auto second_jump = new ir::Instruction(
        ir::Operand(), // 第一个操作数为null表示无条件跳转
        ir::Operand(), // 第二个操作数不使用
        ir::Operand("0", Type::IntLiteral), // 目的操作数是跳转偏移量（后面回填）
        ir::Operator::_goto
      );
      instructions.push_back(second_jump);

      // 生成循环体
      analysisStmt(body_stmt, instructions);

      // 回填continue跳转指令
      for (int pos: continueJumpsStack.top()) {
        int offset = loop_start - pos;
        instructions[pos]->des = ir::Operand(std::to_string(offset), Type::IntLiteral);
      }

      // 循环末尾无条件跳回循环开始
      int loop_back_pos = instructions.size();
      auto loop_back = new ir::Instruction(
        ir::Operand(), // 第一个操作数为null表示无条件跳转
        ir::Operand(), // 第二个操作数不使用
        ir::Operand(std::to_string(loop_start - loop_back_pos), Type::IntLiteral),
        ir::Operator::_goto
      );
      instructions.push_back(loop_back);

      // 设置break目标位置（循环后的第一条指令）
      int break_target_pos = instructions.size();
      breakTargets.push(break_target_pos);

      // 回填break跳转指令
      for (int pos: breakJumpsStack.top()) {
        int offset = break_target_pos - pos;
        instructions[pos]->des = ir::Operand(std::to_string(offset), Type::IntLiteral);
      }

      // 回填第二个goto（当条件为0时跳出循环）
      int exit_offset = break_target_pos - second_jump_pos;
      second_jump->des = ir::Operand(std::to_string(exit_offset), Type::IntLiteral);

      // 清理跳转上下文
      breakJumpsStack.pop();
      continueJumpsStack.pop();
      breakTargets.pop();
      continueTargets.pop();
    }
  } else if (auto exp = dynamic_cast<Exp *>(first)) {
    // 表达式语句
    analysisExp(exp, instructions);
  }
}


void frontend::Analyzer::analysisExp(Exp *root, vector<ir::Instruction *> &instructions) {
  // Exp -> AddExp
  GET_CHILD_PTR(add_exp, AddExp, 0);
  analysisAddExp(add_exp, instructions);
}

void frontend::Analyzer::analysisCond(Cond *root, vector<ir::Instruction *> &instructions) {
  // Cond -> LOrExp
  GET_CHILD_PTR(lor_exp, LOrExp, 0);
  analysisLOrExp(lor_exp, instructions);
}

void frontend::Analyzer::analysisLVal(LVal *root, vector<ir::Instruction *> &instructions, bool is_left) {
  // LVal -> Ident {'[' Exp ']'}
  GET_CHILD_PTR(id_term, Term, 0);
  string id = id_term->token.value;

  // 检查变量是否已声明
  CHECK_DECL(id);
  auto ste = symbol_table.get_ste(id);

  if (root->children.size() == 1) {
    // 简单变量
    auto op = ste.operand;
    if (op.type == Type::IntPtr) {
      instructions.push_back(new ir::Instruction(
        op,
        ir::Operand("0", Type::IntLiteral),
        ir::Operand(GEN_TEMP_VAR, op.type),
        Operator::getptr
      ));
      return;
    } else if (op.type == Type::FloatPtr) {
      instructions.push_back(new ir::Instruction(
        op,
        ir::Operand("0", Type::IntLiteral),
        ir::Operand(GEN_TEMP_VAR, op.type),
        Operator::getptr
      ));
      return;
    }
    instructions.push_back(new ir::Instruction(
      op,
      ir::Operand(),
      ir::Operand(GEN_TEMP_VAR, op.type),
      (ste.operand.type == Type::Int) ? ir::Operator::mov : ir::Operator::fmov
    ));
  } else {
    // 多维数组访问
    vector<ir::Operand> indices;
    for (size_t i = 1; i < root->children.size(); i += 3) {
      // Skip Ident and brackets
      GET_CHILD_PTR(index_exp, Exp, i + 1);
      vector<ir::Instruction *> index_insts;
      analysisExp(index_exp, index_insts);
      instructions.insert(instructions.end(), index_insts.begin(), index_insts.end());
      indices.push_back(index_insts.back()->des);
    }

    // 计算线性偏移：offset = idx0 * (d1 * d2 * ...) + idx1 * (d2 * d3 * ...) + ... + idxN
    ir::Operand offset_op = indices[0];
    for (size_t i = 0; i < indices.size() - 1; ++i) {
      int multiplier = 1;
      for (size_t j = i + 1; j < ste.dimension.size(); ++j) {
        multiplier *= ste.dimension[j];
      }

      if (multiplier != 1) {
        string temp_mul = GEN_TEMP_VAR;
        instructions.push_back(new ir::Instruction(
          indices[i],
          ir::Operand(std::to_string(multiplier), Type::IntLiteral),
          ir::Operand(temp_mul, Type::Int),
          ir::Operator::mul
        ));
        offset_op = ir::Operand(temp_mul, Type::Int);
      } else {
        offset_op = indices[i];
      }

      string temp_add = GEN_TEMP_VAR;
      instructions.push_back(new ir::Instruction(
        offset_op,
        indices[i + 1],
        ir::Operand(temp_add, Type::Int),
        ir::Operator::add
      ));
      offset_op = ir::Operand(temp_add, Type::Int);
    }

    // 生成数组访问指令
    auto temp_var = GEN_TEMP_VAR;
    if (is_left) {
      instructions.push_back(new ir::Instruction(
        ste.operand,
        offset_op,
        ir::Operand(temp_var, ste.operand.type),
        ir::Operator::getptr
      ));
    } else {
      instructions.push_back(new ir::Instruction(
        ste.operand,
        offset_op,
        ir::Operand(temp_var,
                    ste.operand.type == Type::IntPtr ? Type::Int : Type::Float),
        ir::Operator::load
      ));
    }
  }
}

void frontend::Analyzer::analysisNumber(Number *root) {
  // Number -> IntConst | FloatConst
  GET_CHILD_PTR(const_term, Term, 0);

  const std::string &value = const_term->token.value;

  if (const_term->token.type == TokenType::INTLTR) {
    try {
      if (value.size() > 1) {
        if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
          // 十六进制 (0x或0X开头)
          root->v.i = std::stoi(value.substr(2), nullptr, 16);
        } else if (value[0] == '0' && (value[1] == 'b' || value[1] == 'B')) {
          // 二进制 (0b或0B开头)
          root->v.i = std::stoi(value.substr(2), nullptr, 2);
        } else if (value[0] == '0' && value.size() > 1) {
          // 八进制 (0开头)
          root->v.i = std::stoi(value.substr(1), nullptr, 8);
        } else {
          // 十进制
          root->v.i = std::stoi(value);
        }
      } else {
        // 单个数字
        root->v.i = std::stoi(value);
      }
      root->t = Type::Int;
    } catch (const std::exception &e) {
      throw std::runtime_error("Invalid integer literal: " + value);
    }
  } else {
    try {
      root->v.f = std::stof(value);
      root->t = Type::Float;
    } catch (const std::exception &e) {
      throw std::runtime_error("Invalid float literal: " + value);
    }
  }
}

void frontend::Analyzer::analysisPrimaryExp(PrimaryExp *root, vector<ir::Instruction *> &instructions) {
  // PrimaryExp -> '(' Exp ')' | LVal | Number

  if (root->children.size() == 3) {
    // 括号表达式
    auto exp = dynamic_cast<Exp *>(root->children[1]);
    if (exp) {
      analysisExp(exp, instructions);
    } else {
      throw std::runtime_error("Invalid AST structure: middle of bracketed PrimaryExp is not Exp");
    }
  } else if (auto lval = dynamic_cast<LVal *>(root->children[0])) {
    analysisLVal(lval, instructions, false);
  } else if (auto number = dynamic_cast<Number *>(root->children[0])) {
    analysisNumber(number);
    string temp_var = GEN_TEMP_VAR;
    std::string literal_value;
    Type literal_type;
    ir::Operator op;

    if (number->t == Type::Int) {
      literal_value = std::to_string(number->v.i);
      literal_type = Type::IntLiteral;
      op = ir::Operator::def;
    } else {
      literal_value = float_to_string_lossless(number->v.f);
      literal_type = Type::FloatLiteral;
      op = ir::Operator::fdef;
    }

    instructions.push_back(new ir::Instruction(
      ir::Operand(literal_value, literal_type),
      ir::Operand(),
      ir::Operand(temp_var, number->t),
      op
    ));
  }
}


void frontend::Analyzer::analysisUnaryExp(UnaryExp *root, vector<ir::Instruction *> &instructions) {
  // UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
  if (auto primary_exp = dynamic_cast<PrimaryExp *>(root->children[0])) {
    analysisPrimaryExp(primary_exp, instructions);
  } else if (root->children.size() > 1) {
    // 函数调用或一元运算
    if (auto id_term = dynamic_cast<Term *>(root->children[0])) {
      // 函数调用
      string func_name = id_term->token.value;
      vector<ir::Operand> params;

      // 处理参数
      if (root->children.size() > 3) {
        GET_CHILD_PTR(rparams, FuncRParams, 2);
        vector<ir::Instruction *> param_insts;
        analysisFuncRParams(rparams, param_insts, symbol_table.functions[func_name], params);
        instructions.insert(instructions.end(), param_insts.begin(), param_insts.end());
      }

      // 生成函数调用指令
      auto ret_type = symbol_table.functions[func_name]->returnType;
      if (ret_type != Type::null) {
        string temp_var = GEN_TEMP_VAR;
        instructions.push_back(new ir::CallInst(
          ir::Operand(func_name),
          params,
          ir::Operand(temp_var, ret_type)
        ));
      } else {
        instructions.push_back(new ir::CallInst(
          ir::Operand(func_name),
          params,
          ir::Operand()
        ));
      }
    } else {
      // 一元运算
      GET_CHILD_PTR(unary_op, UnaryOp, 0);
      GET_CHILD_PTR(unary_exp, UnaryExp, 1);

      vector<ir::Instruction *> exp_insts;
      analysisUnaryExp(unary_exp, exp_insts);
      // 短路
      if (exp_insts.size() == 1 && exp_insts.back()->op == Operator::def) {
        // 字面量，直接短路计算
        auto op_term = dynamic_cast<Term *>(unary_op->children[0]);
        if (op_term->token.value == "-") {
          string temp_var = GEN_TEMP_VAR;
          auto literal_val_str = exp_insts.back()->op1.name;
          if (literal_val_str[0] == '-') {
            literal_val_str = literal_val_str.substr(1);
          } else {
            literal_val_str = '-' + literal_val_str;
          }
          instructions.push_back(new ir::Instruction(
            ir::Operand(literal_val_str,
                        exp_insts.back()->des.type == Type::Int ? Type::IntLiteral : Type::FloatLiteral),
            exp_insts.back()->des,
            ir::Operand(temp_var, exp_insts.back()->des.type),
            exp_insts.back()->des.type == Type::Int ? ir::Operator::def : ir::Operator::fdef
          ));
          return;
        }
      }
      instructions.insert(instructions.end(), exp_insts.begin(), exp_insts.end());

      // 获取运算符
      auto op_term = dynamic_cast<Term *>(unary_op->children[0]);
      string temp_var = GEN_TEMP_VAR;

      if (op_term->token.value == "-") {
        instructions.push_back(new ir::Instruction(
          ir::Operand("0", exp_insts.back()->des.type == Type::Int ? Type::IntLiteral : Type::FloatLiteral),
          exp_insts.back()->des,
          ir::Operand(temp_var, exp_insts.back()->des.type),
          exp_insts.back()->des.type == Type::Int ? ir::Operator::sub : ir::Operator::fsub
        ));
      } else if (op_term->token.value == "!") {
        ir::Operand operand = exp_insts.back()->des;

        if (operand.type == Type::Float) {
          // 如果是浮点数，先转换为整数
          string int_var = GEN_TEMP_VAR;
          instructions.push_back(new ir::Instruction(
              operand,
              ir::Operand(),
              ir::Operand(int_var, Type::Int),
              ir::Operator::cvt_f2i
          ));
          operand = ir::Operand(int_var, Type::Int);
        }

        // 执行逻辑非操作
        instructions.push_back(new ir::Instruction(
            operand,
            ir::Operand(),
            ir::Operand(temp_var, Type::Int),
            ir::Operator::_not
        ));
      }
      // + 运算符不需要生成新指令
    }
  }
}

void frontend::Analyzer::analysisFuncRParams(FuncRParams *root, vector<ir::Instruction *> &instructions,
                                             ir::Function *func, vector<ir::Operand> &params) {
  // FuncRParams -> Exp { ',' Exp }
  int param_index = 0;
  for (int i = 0; i < root->children.size(); i += 2) {
    GET_CHILD_PTR(exp, Exp, i);
    vector<ir::Instruction *> exp_insts;
    analysisExp(exp, exp_insts);
    instructions.insert(instructions.end(), exp_insts.begin(), exp_insts.end());

    // 类型转换检查
    if (exp_insts.back()->des.type != func->ParameterList[param_index].type) {
      string temp_var = GEN_TEMP_VAR;
      instructions.push_back(new ir::Instruction(
        exp_insts.back()->des,
        ir::Operand(),
        ir::Operand(temp_var, func->ParameterList[param_index].type),
        exp_insts.back()->des.type == Type::Int ? ir::Operator::cvt_i2f : ir::Operator::cvt_f2i
      ));
    }
    params.push_back(instructions.back()->des);
    param_index++;
  }
}

void frontend::Analyzer::analysisAddExp(AddExp *root, vector<ir::Instruction *> &instructions) {
  // AddExp -> MulExp { ('+' | '-') MulExp }
  GET_CHILD_PTR(first_mul, MulExp, 0);
  analysisMulExp(first_mul, instructions);

  // 处理后续的加减运算
  for (int i = 1; i < root->children.size(); i += 2) {
    GET_CHILD_PTR(op_term, Term, i);
    GET_CHILD_PTR(mul_exp, MulExp, i + 1);

    vector<ir::Instruction *> mul_insts;
    analysisMulExp(mul_exp, mul_insts);
    instructions.insert(instructions.end(), mul_insts.begin(), mul_insts.end());

    // 获取操作数类型
    ir::Operand op1 = instructions[instructions.size() - mul_insts.size() - 1]->des;
    ir::Operand op2 = mul_insts.back()->des;
    Type type1 = op1.type;
    Type type2 = op2.type;

    // 确定结果类型（如果类型不同，使用更高精度的类型）
    Type result_type = (type1 == Type::Float || type2 == Type::Float) ? Type::Float : Type::Int;
    string temp_var = GEN_TEMP_VAR;

    // 处理类型转换
    if (type1 != type2) {
      string converted_var = GEN_TEMP_VAR;
      if (type1 == Type::Int && type2 == Type::Float) {
        instructions.push_back(new ir::Instruction(
          op1,
          ir::Operand(),
          ir::Operand(converted_var, Type::Float),
          Operator::cvt_i2f
        ));
        op1 = ir::Operand(converted_var, Type::Float);
      } else if (type1 == Type::Float && type2 == Type::Int) {
        instructions.push_back(new ir::Instruction(
          op2,
          ir::Operand(),
          ir::Operand(converted_var, Type::Float),
          Operator::cvt_i2f
        ));
        op2 = ir::Operand(converted_var, Type::Float);
      }
    }

    // 生成加减运算指令
    if (op_term->token.value == "+") {
      instructions.push_back(new ir::Instruction(
        op1,
        op2,
        ir::Operand(temp_var, result_type),
        result_type == Type::Int ? Operator::add : Operator::fadd
      ));
    } else {
      // "-"
      instructions.push_back(new ir::Instruction(
        op1,
        op2,
        ir::Operand(temp_var, result_type),
        result_type == Type::Int ? Operator::sub : Operator::fsub
      ));
    }
  }
}

void frontend::Analyzer::analysisMulExp(MulExp *root, vector<ir::Instruction *> &instructions) {
  // MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
  GET_CHILD_PTR(first_unary, UnaryExp, 0);
  analysisUnaryExp(first_unary, instructions);

  // 处理后续的乘除模运算
  for (int i = 1; i < root->children.size(); i += 2) {
    GET_CHILD_PTR(op_term, Term, i);
    GET_CHILD_PTR(unary_exp, UnaryExp, i + 1);

    vector<ir::Instruction *> unary_insts;
    analysisUnaryExp(unary_exp, unary_insts);
    instructions.insert(instructions.end(), unary_insts.begin(), unary_insts.end());

    // 获取操作数
    ir::Operand op1 = instructions[instructions.size() - unary_insts.size() - 1]->des;
    ir::Operand op2 = unary_insts.back()->des;
    Type type1 = op1.type;
    Type type2 = op2.type;

    // 处理模运算的特殊情况
    if (op_term->token.value == "%") {
      assert(type1 == Type::Int && type2 == Type::Int && "Modulo operation only supports integer operands");
      string temp_var = GEN_TEMP_VAR;
      instructions.push_back(new ir::Instruction(
        op1,
        op2,
        ir::Operand(temp_var, Type::Int),
        Operator::mod
      ));
      continue;
    }

    // 处理类型转换（乘法和除法）
    if (type1 != type2) {
      string converted_var = GEN_TEMP_VAR;
      if (type1 == Type::Int && type2 == Type::Float) {
        // Convert op1 (int) to float
        instructions.push_back(new ir::Instruction(
          op1,
          ir::Operand(),
          ir::Operand(converted_var, Type::Float),
          Operator::cvt_i2f
        ));
        op1 = ir::Operand(converted_var, Type::Float);
      } else if (type1 == Type::Float && type2 == Type::Int) {
        // Convert op2 (int) to float
        instructions.push_back(new ir::Instruction(
          op2,
          ir::Operand(),
          ir::Operand(converted_var, Type::Float),
          Operator::cvt_i2f
        ));
        op2 = ir::Operand(converted_var, Type::Float);
      }
    }

    // 确定运算类型（浮点或整数）
    bool is_float = (op1.type == Type::Float || op2.type == Type::Float);
    string temp_var = GEN_TEMP_VAR;

    // 生成运算指令
    if (op_term->token.value == "*") {
      instructions.push_back(new ir::Instruction(
        op1,
        op2,
        ir::Operand(temp_var, is_float ? Type::Float : Type::Int),
        is_float ? Operator::fmul : Operator::mul
      ));
    } else {
      // "/"
      instructions.push_back(new ir::Instruction(
        op1,
        op2,
        ir::Operand(temp_var, is_float ? Type::Float : Type::Int),
        is_float ? Operator::fdiv : Operator::div
      ));
    }
  }
}

void frontend::Analyzer::analysisRelExp(RelExp *root, vector<ir::Instruction *> &instructions) {
  // RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
  GET_CHILD_PTR(first_add, AddExp, 0);
  analysisAddExp(first_add, instructions);

  // 处理后续的关系运算
  for (int i = 1; i < root->children.size(); i += 2) {
    GET_CHILD_PTR(op_term, Term, i);
    GET_CHILD_PTR(add_exp, AddExp, i + 1);

    vector<ir::Instruction *> add_insts;
    analysisAddExp(add_exp, add_insts);
    instructions.insert(instructions.end(), add_insts.begin(), add_insts.end());

    // 获取操作数
    ir::Operand op1 = instructions[instructions.size() - add_insts.size() - 1]->des;
    ir::Operand op2 = add_insts.back()->des;
    Type type1 = op1.type;
    Type type2 = op2.type;

    // 处理类型转换
    if (type1 != type2) {
      string converted_var = GEN_TEMP_VAR;
      if (type1 == Type::Int && type2 == Type::Float) {
        instructions.push_back(new ir::Instruction(
          op1,
          ir::Operand(),
          ir::Operand(converted_var, Type::Float),
          Operator::cvt_i2f
        ));
        op1 = ir::Operand(converted_var, Type::Float);
      } else if (type1 == Type::Float && type2 == Type::Int) {
        instructions.push_back(new ir::Instruction(
          op2,
          ir::Operand(),
          ir::Operand(converted_var, Type::Float),
          Operator::cvt_i2f
        ));
        op2 = ir::Operand(converted_var, Type::Float);
      }
    }

    // 确定比较类型（浮点或整数）
    bool is_float = (op1.type == Type::Float || op2.type == Type::Float);
    string temp_var = GEN_TEMP_VAR;

    // 生成关系运算指令
    if (op_term->token.value == "<") {
      instructions.push_back(new ir::Instruction(
        op1,
        op2,
        ir::Operand(temp_var, is_float ? Type::Float : Type::Int), // 浮点比较结果为Float
        is_float ? Operator::flss : Operator::lss
      ));
    } else if (op_term->token.value == ">") {
      instructions.push_back(new ir::Instruction(
        op1,
        op2,
        ir::Operand(temp_var, is_float ? Type::Float : Type::Int), // 浮点比较结果为Float
        is_float ? Operator::fgtr : Operator::gtr
      ));
    } else if (op_term->token.value == "<=") {
      instructions.push_back(new ir::Instruction(
        op1,
        op2,
        ir::Operand(temp_var, is_float ? Type::Float : Type::Int), // 浮点比较结果为Float
        is_float ? Operator::fleq : Operator::leq
      ));
    } else {
      // ">="
      instructions.push_back(new ir::Instruction(
        op1,
        op2,
        ir::Operand(temp_var, is_float ? Type::Float : Type::Int), // 浮点比较结果为Float
        is_float ? Operator::fgeq : Operator::geq
      ));
    }
  }
}

void frontend::Analyzer::analysisEqExp(EqExp *root, vector<ir::Instruction *> &instructions) {
  // EqExp -> RelExp { ('==' | '!=') RelExp }
  GET_CHILD_PTR(first_rel, RelExp, 0);
  analysisRelExp(first_rel, instructions);

  // 处理后续的相等性运算
  for (int i = 1; i < root->children.size(); i += 2) {
    GET_CHILD_PTR(op_term, Term, i);
    GET_CHILD_PTR(rel_exp, RelExp, i + 1);

    vector<ir::Instruction *> rel_insts;
    analysisRelExp(rel_exp, rel_insts);
    instructions.insert(instructions.end(), rel_insts.begin(), rel_insts.end());

    // 生成相等性运算指令
    string temp_var = GEN_TEMP_VAR;
    bool is_int = instructions[instructions.size() - rel_insts.size() - 1]->des.type == Type::Int;

    if (op_term->token.value == "==") {
      instructions.push_back(new ir::Instruction(
        instructions[instructions.size() - rel_insts.size() - 1]->des,
        rel_insts.back()->des,
        ir::Operand(temp_var, Type::Int),
        is_int ? ir::Operator::eq : ir::Operator::feq
      ));
    } else {
      // "!="
      instructions.push_back(new ir::Instruction(
        instructions[instructions.size() - rel_insts.size() - 1]->des,
        rel_insts.back()->des,
        ir::Operand(temp_var, Type::Int),
        is_int ? ir::Operator::neq : ir::Operator::fneq
      ));
    }
  }
}

// And 和 Or 不能使用转换IR，例如0.3实际应该是true，转换后会被截断成0，正确做法是使用fneq IR
void frontend::Analyzer::analysisLAndExp(LAndExp *root, vector<ir::Instruction *> &instructions) {
  GET_CHILD_PTR(eq_exp, EqExp, 0);
  analysisEqExp(eq_exp, instructions);
  auto first_cond = instructions.back()->des;
  if (first_cond.type == Type::Float) {
    string cmp_var = GEN_TEMP_VAR;
    instructions.push_back(new ir::Instruction(
        first_cond,
        ir::Operand("0.0", Type::FloatLiteral),
        ir::Operand(cmp_var, Type::Float),
        ir::Operator::fneq
    ));

    // 2. fneq结果是float，需要转为int
    string int_var = GEN_TEMP_VAR;
    instructions.push_back(new ir::Instruction(
        ir::Operand(cmp_var, Type::Float),
        ir::Operand(),
        ir::Operand(int_var, Type::Int),
        ir::Operator::cvt_f2i
    ));
    first_cond = ir::Operand(int_var, Type::Int);  // 更新为整数类型
  }

  if (root->children.size() == 1) {
    return;
  }

  string result = GEN_TEMP_VAR;
  instructions.push_back(new ir::Instruction(
    first_cond,
    ir::Operand(),
    ir::Operand("2", Type::IntLiteral), // 假如是true，就跳到分析第二个表达式
    ir::Operator::_goto
  ));

  // 第二条 goto X：first_cond 为 false，跳到 set_false
  auto goto_set_false = new ir::Instruction(
    ir::Operand("1", Type::IntLiteral), // 恒真
    ir::Operand(),
    ir::Operand("-1", Type::IntLiteral), // 占位
    ir::Operator::_goto
  );
  instructions.push_back(goto_set_false);
  int goto_set_false_pos = instructions.size() - 1;

  // 分析第二个表达式
  GET_CHILD_PTR(land_exp, LAndExp, 2);
  vector<ir::Instruction *> land_insts;
  analysisLAndExp(land_exp, land_insts);
  auto second_cond = land_insts.back()->des;
  if (second_cond.type == Type::Float) {
    string cmp_var = GEN_TEMP_VAR;
    instructions.push_back(new ir::Instruction(
        second_cond,
        ir::Operand("0.0", Type::FloatLiteral),
        ir::Operand(cmp_var, Type::Float),
        ir::Operator::fneq
    ));

    string int_var = GEN_TEMP_VAR;
    instructions.push_back(new ir::Instruction(
        ir::Operand(cmp_var, Type::Float),
        ir::Operand(),
        ir::Operand(int_var, Type::Int),
        ir::Operator::cvt_f2i
    ));
    second_cond = ir::Operand(int_var, Type::Int);  // 更新为整数类型
  }
  instructions.insert(instructions.end(), land_insts.begin(), land_insts.end());

  instructions.push_back(new ir::Instruction(
    second_cond,
    ir::Operand(),
    ir::Operand(result, Type::Int),
    ir::Operator::mov
  ));

  // 跳过 set_false 部分
  auto goto_end = new ir::Instruction(
    ir::Operand("1", Type::IntLiteral),
    ir::Operand(),
    ir::Operand("-1", Type::IntLiteral), // 占位
    ir::Operator::_goto
  );
  instructions.push_back(goto_end);
  int goto_end_pos = instructions.size() - 1;

  // set_false:
  int set_false_pos = instructions.size();
  instructions.push_back(new ir::Instruction(
    ir::Operand("0", Type::IntLiteral),
    ir::Operand(),
    ir::Operand(result, Type::Int),
    ir::Operator::mov
  ));

  // 修正跳转
  goto_set_false->des = ir::Operand(std::to_string(set_false_pos - goto_set_false_pos), Type::IntLiteral);
  goto_end->des = ir::Operand(std::to_string(instructions.size() - goto_end_pos), Type::IntLiteral);

  // 为了让上层能用 des 获取，这里再加一条 mov result 到自己
  instructions.push_back(new ir::Instruction(
    ir::Operand(result, Type::Int),
    ir::Operand(),
    ir::Operand(result, Type::Int),
    ir::Operator::mov
  ));
}


void frontend::Analyzer::analysisLOrExp(LOrExp *root, vector<ir::Instruction *> &instructions) {
  GET_CHILD_PTR(land_exp, LAndExp, 0);
  analysisLAndExp(land_exp, instructions);
  auto first_cond = instructions.back()->des;
  // 处理第一个条件为float的情况
  if (first_cond.type == Type::Float) {
    string cmp_var = GEN_TEMP_VAR;
    instructions.push_back(new ir::Instruction(
        first_cond,
        ir::Operand("0.0", Type::FloatLiteral),
        ir::Operand(cmp_var, Type::Float),
        ir::Operator::fneq
    ));

    string int_var = GEN_TEMP_VAR;
    instructions.push_back(new ir::Instruction(
        ir::Operand(cmp_var, Type::Float),
        ir::Operand(),
        ir::Operand(int_var, Type::Int),
        ir::Operator::cvt_f2i
    ));
    first_cond = ir::Operand(int_var, Type::Int);  // 更新为整数类型
  }

  if (root->children.size() > 1) {
    string result_var = GEN_TEMP_VAR;

    // 如果第一个条件满足的话，直接跳转到末尾
    auto goto_true = new ir::Instruction(
      first_cond,
      ir::Operand(),
      ir::Operand("-1", Type::IntLiteral), // 占位
      ir::Operator::_goto
    );
    instructions.push_back(goto_true);
    int goto_true_pos = instructions.size() - 1;

    // 分析右边 LOrExp
    GET_CHILD_PTR(lor_exp, LOrExp, 2);
    vector<ir::Instruction *> lor_insts;
    analysisLOrExp(lor_exp, lor_insts);
    auto second_cond = lor_insts.back()->des;
    if (second_cond.type == Type::Float) {
      string cmp_var = GEN_TEMP_VAR;
      instructions.push_back(new ir::Instruction(
          second_cond,
          ir::Operand("0.0", Type::FloatLiteral),
          ir::Operand(cmp_var, Type::Float),
          ir::Operator::fneq
      ));

      string int_var = GEN_TEMP_VAR;
      instructions.push_back(new ir::Instruction(
          ir::Operand(cmp_var, Type::Float),
          ir::Operand(),
          ir::Operand(int_var, Type::Int),
          ir::Operator::cvt_f2i
      ));
      second_cond = ir::Operand(int_var, Type::Int);  // 更新为整数类型
    }
    instructions.insert(instructions.end(), lor_insts.begin(), lor_insts.end());

    instructions.push_back(new ir::Instruction(
      second_cond,
      ir::Operand(),
      ir::Operand(result_var, Type::Int),
      ir::Operator::mov
    ));

    // 跳转到 end
    auto goto_end = new ir::Instruction(
      ir::Operand(),
      ir::Operand(),
      ir::Operand("-1", Type::IntLiteral),
      ir::Operator::_goto
    );
    instructions.push_back(goto_end);
    int goto_end_pos = instructions.size() - 1;

    // true_label
    int true_label_pos = instructions.size();
    instructions.push_back(new ir::Instruction(
      ir::Operand("1", Type::IntLiteral),
      ir::Operand(),
      ir::Operand(result_var, Type::Int),
      ir::Operator::mov
    ));

    // 修复跳转
    goto_true->des = ir::Operand(std::to_string(true_label_pos - goto_true_pos), Type::IntLiteral);
    goto_end->des = ir::Operand(std::to_string(instructions.size() - goto_end_pos), Type::IntLiteral);

    // 设置最后的 des
    instructions.push_back(new ir::Instruction(
      ir::Operand(result_var, Type::Int),
      ir::Operand(),
      ir::Operand(result_var, Type::Int),
      ir::Operator::mov
    ));
  }
}

void frontend::Analyzer::analysisConstExp(ConstExp *root, vector<ir::Instruction *> &instructions) {
  // ConstExp -> AddExp
  GET_CHILD_PTR(add_exp, AddExp, 0);
  analysisAddExp(add_exp, instructions);
}

// 递归计算constexp节点的值的
int frontend::Analyzer::evalConstExp(AstNode *node) {
  // ConstExp -> AddExp
  if (node->type == NodeType::CONSTEXP) {
    return evalConstExp(node->children[0]); // 递归计算 AddExp
  }

  // AddExp -> MulExp { ('+' | '-') MulExp }
  else if (node->type == NodeType::ADDEXP) {
    int result = evalConstExp(node->children[0]); // 计算第一个 MulExp
    for (int i = 1; i < node->children.size(); i += 2) {
      AstNode *op_node = node->children[i];
      AstNode *rhs_node = node->children[i + 1];
      int rhs = evalConstExp(rhs_node); // 计算右边的 MulExp

      // 判断运算符
      Term *op_term = dynamic_cast<Term *>(op_node);
      if (op_term->token.type == TokenType::PLUS) result += rhs;
      else result -= rhs;
    }
    return result;
  }

  // MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
  else if (node->type == NodeType::MULEXP) {
    int result = evalConstExp(node->children[0]); // 计算第一个 UnaryExp
    for (int i = 1; i < node->children.size(); i += 2) {
      AstNode *op_node = node->children[i];
      AstNode *rhs_node = node->children[i + 1];
      int rhs = evalConstExp(rhs_node); // 计算右边的 UnaryExp

      // 判断运算符
      Term *op_term = dynamic_cast<Term *>(op_node);
      if (op_term->token.type == TokenType::MULT) result *= rhs;
      else if (op_term->token.type == TokenType::DIV) result /= rhs;
      else result %= rhs;
    }
    return result;
  }

  // UnaryExp -> PrimaryExp | ('+' | '-') UnaryExp
  else if (node->type == NodeType::UNARYEXP) {
    if (node->children[0]->type == NodeType::PRIMARYEXP) {
      return evalConstExp(node->children[0]); // PrimaryExp
    } else {
      Term *op_term = dynamic_cast<Term *>(node->children[0]->children[0]);
      int val = evalConstExp(node->children[1]); // UnaryExp
      return (op_term->token.type == TokenType::MINU) ? -val : val;
    }
  }

  // PrimaryExp -> '(' Exp ')' | LVal | Number
  else if (node->type == NodeType::PRIMARYEXP) {
    AstNode *child = node->children[0];
    if (child->type == NodeType::TERMINAL) {
      Term *term = dynamic_cast<Term *>(child);
      if (term->token.type == TokenType::LPARENT) {
        return evalConstExp(node->children[1]); // Exp
      }
    } else if (child->type == NodeType::LVAL) {
      return evalConstExp(child); // LVal
    } else if (child->type == NodeType::NUMBER) {
      return evalConstExp(child); // Number
    }
  }

  // LVal -> Ident
  else if (node->type == NodeType::LVAL) {
    Term *ident = dynamic_cast<Term *>(node->children[0]);
    std::string var_name = ident->token.value;

    // 查找符号表，获取常量值
    auto op = symbol_table.get_operand(var_name);
    if (op.type == Type::null) {
      throw std::runtime_error("Undefined or non-const variable: " + var_name);
    }
    if (const_vars.find(op.name) == const_vars.end()) {
      throw std::runtime_error("Undefined const var");
    }
    return const_vars[op.name].i_val;
  }

  // Number -> IntConst
  else if (node->type == NodeType::NUMBER) {
    Term *num_term = dynamic_cast<Term *>(node->children[0]);
    return std::stoi(num_term->token.value); // 直接返回整数值
  }

  // 其他情况（理论上不会走到这里）
  throw std::runtime_error("Unsupported node type in evalConstExp");
}

float frontend::Analyzer::evalFloatConstExp(AstNode *node) {
  // ConstExp -> AddExp
  if (node->type == NodeType::CONSTEXP) {
    return evalFloatConstExp(node->children[0]); // 递归计算 AddExp
  }

  // AddExp -> MulExp { ('+' | '-') MulExp }
  else if (node->type == NodeType::ADDEXP) {
    float result = evalFloatConstExp(node->children[0]); // 计算第一个 MulExp
    for (int i = 1; i < node->children.size(); i += 2) {
      AstNode *op_node = node->children[i];
      AstNode *rhs_node = node->children[i + 1];
      float rhs = evalFloatConstExp(rhs_node); // 计算右边的 MulExp

      // 判断运算符
      Term *op_term = dynamic_cast<Term *>(op_node);
      if (op_term->token.type == TokenType::PLUS) result += rhs;
      else result -= rhs;
    }
    return result;
  }

  // MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
  else if (node->type == NodeType::MULEXP) {
    float result = evalFloatConstExp(node->children[0]); // 计算第一个 UnaryExp
    for (int i = 1; i < node->children.size(); i += 2) {
      AstNode *op_node = node->children[i];
      AstNode *rhs_node = node->children[i + 1];
      float rhs = evalFloatConstExp(rhs_node); // 计算右边的 UnaryExp

      // 判断运算符
      Term *op_term = dynamic_cast<Term *>(op_node);
      if (op_term->token.type == TokenType::MULT) result *= rhs;
      else if (op_term->token.type == TokenType::DIV) result /= rhs;
    }
    return result;
  }

  // UnaryExp -> PrimaryExp | ('+' | '-') UnaryExp
  else if (node->type == NodeType::UNARYEXP) {
    if (node->children[0]->type == NodeType::PRIMARYEXP) {
      return evalFloatConstExp(node->children[0]); // PrimaryExp
    } else {
      Term *op_term = dynamic_cast<Term *>(node->children[0]->children[0]);
      float val = evalFloatConstExp(node->children[1]); // UnaryExp
      return (op_term->token.type == TokenType::MINU) ? -val : val;
    }
  }

  // PrimaryExp -> '(' Exp ')' | LVal | Number
  else if (node->type == NodeType::PRIMARYEXP) {
    AstNode *child = node->children[0];
    if (child->type == NodeType::TERMINAL) {
      Term *term = dynamic_cast<Term *>(child);
      if (term->token.type == TokenType::LPARENT) {
        return evalFloatConstExp(node->children[1]); // Exp
      }
    } else if (child->type == NodeType::LVAL) {
      return evalFloatConstExp(child); // LVal
    } else if (child->type == NodeType::NUMBER) {
      return evalFloatConstExp(child); // Number
    }
  }

  // LVal -> Ident
  else if (node->type == NodeType::LVAL) {
    Term *ident = dynamic_cast<Term *>(node->children[0]);
    std::string var_name = ident->token.value;

    // 查找符号表，获取常量值
    auto op = symbol_table.get_operand(var_name);
    if (op.type == Type::null) {
      throw std::runtime_error("Undefined or non-const variable: " + var_name);
    }
    if (const_vars.find(op.name) == const_vars.end()) {
      throw std::runtime_error("Undefined const var");
    }
    return const_vars[op.name].f_val;
  }

  // Number -> FloatConst
  else if (node->type == NodeType::NUMBER) {
    Term *num_term = dynamic_cast<Term *>(node->children[0]);
    return std::stof(num_term->token.value); // 直接返回整数值
  }

  // 其他情况（理论上不会走到这里）
  throw std::runtime_error("Unsupported node type in evalConstExp");
}