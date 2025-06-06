#include "backend/generator.h"
#include <cassert>
#include <deque>
#include <iostream>
#include <unordered_map>

#define TODO assert(0 && "todo")
#define FUCK assert(0 && "fuck")
#define DBG false
#define REGLIST \
  { rvREG::X9, rvREG::X18, rvREG::X19 }
#define FREGLIST \
  { rvFREG::F9, rvFREG::F18, rvFREG::F19 }
#define NREG rvREG::X0
#define REGA0 rvREG::X10    // a0
#define REGT0 rvREG::X5     // t0
#define REGRA rvREG::X1     // ra
#define REGSP rvREG::X2     // sp
#define REGS0 rvREG::X8     // s0
#define FREGA0 rvFREG::F10  // fa0
#define FREGT0 rvFREG::F5   // ft5
#define NIMM 0
#define NFREG rvFREG::F0

using backend::VarArea;
using backend::VarLocation;
using rv::rv_inst;
using rv::rvFREG;
using rv::rvOPCODE;
using rv::rvREG;

const int frame_size = 2000;
int genInstrCounter = -1;
std::string cur_funcname;

backend::Generator::Generator(ir::Program& p, std::ofstream& f) : program(p), fout(f) {}

backend::VarLocation::VarLocation(VarArea vararea, int offset, bool isReg, rvREG reg, rvFREG)
    : vararea(vararea), offset(offset), reg(reg), freg(freg), isReg(isReg) {}

backend::stackVarMap::stackVarMap(int offset) : offset(offset) {}

bool isglobal(std::string str) {
  std::string suffix = "_global";
  if (suffix.size() > str.size()) return false;
  return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

int backend::stackVarMap::find_operand(const std::string op) {
  auto iter = this->_table.find(op);
  assert(iter != _table.end());
  return iter->second;
}

int backend::stackVarMap::add_operand(const std::string opname, uint32_t size) {
  auto it = this->_table.find(opname);
  if (it != this->_table.end()) return 0;
  this->_table[opname] = this->offset;
  this->offset = this->offset - size;
  return 0;
}

VarLocation backend::Generator::find_operand(const std::string name) {
  rvREG reg = NREG;
  rvFREG freg = NFREG;
  auto variter = this->cur_varmap->_table.find(name);
  if (variter != this->cur_varmap->_table.end()) return {VarArea::LOCAL, variter->second, false, reg, freg};
  variter = this->param_varmap->_table.find(name);
  if (variter != this->param_varmap->_table.end()) return {VarArea::PARAM, variter->second, false, reg, freg};
  variter = this->global_varmap->_table.find(name);
  if (variter != this->global_varmap->_table.end()) return {VarArea::GLOBL, variter->second, false, reg, freg};
  return {VarArea::ANULL, -1, false, reg};
}

void backend::Generator::freereg(const rv::rvREG reg) {
  std::string name = this->regTag->at((int)reg);
  if (name.empty()) return;
  VarLocation varloc = this->find_operand(name);
  rvREG base = aRegs.front();
  switch (varloc.vararea) {
    case VarArea::LOCAL:
    case VarArea::PARAM:
      this->sentences.push_back("\tsw\t" + toString(reg) + "," + std::to_string(varloc.offset) + "(s0)");
      break;
    case VarArea::GLOBL:
      this->aRegs.pop_front();
      this->aRegs.push_back(base);
      this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::LA, base, NREG, NREG, NIMM, name).draw());
      this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::SW, reg, base).draw());
      break;
    case VarArea::ANULL:
      break;
    default:
      assert(0);
  }
  this->regTag->at((int)reg) = "";
}

void backend::Generator::freereg(const rv::rvFREG freg) {
  std::string name = this->regTag->at((int)freg + 32);
  if (name.empty()) return;
  VarLocation varloc = this->find_operand(name);
  rvREG base = aRegs.front();
  switch (varloc.vararea) {
    case VarArea::LOCAL:
    case VarArea::PARAM:
      this->sentences.push_back("\tfsw\t" + toString(freg) + "," + std::to_string(varloc.offset) + "(s0)");
      break;
    case VarArea::GLOBL:
      this->aRegs.pop_front();
      this->aRegs.push_back(base);
      this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::LA, base, NREG, NREG, NIMM, name).draw());
      this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::FSW, freg, base).draw());
      break;
    case VarArea::ANULL:
      break;
    default:
      assert(0);
  }
  this->regTag->at((int)freg + 32) = "";
}

rv::rvREG backend::Generator::getRd(const ir::Operand* op) {
  auto ret = aRegs.front();
  aRegs.pop_front();
  aRegs.push_back(ret);
  regTag->at((int)ret) = op->name;
  return ret;
}

rv::rvREG backend::Generator::getRs1(const ir::Operand* op) {
  VarLocation iter = this->find_operand(op->name);
  rvREG ret = aRegs.front();
  aRegs.pop_front();
  aRegs.push_back(ret);
  this->regTag->at((int)ret) = op->name;
  if (op->type == ir::Type::IntLiteral) {
    this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::LI, ret, NREG, NREG, std::stoi(op->name)).draw());
    return ret;
  }
  switch (iter.vararea) {
    case VarArea::LOCAL:
      this->sentences.push_back("\tlw\t" + toString(ret) + "," + std::to_string(iter.offset) + "(s0)");
      break;
    case VarArea::PARAM:
      if (iter.offset >= 0)
        this->sentences.push_back("\tlw\t" + toString(ret) + "," + std::to_string(iter.offset) + "(s0)");
      else
        this->sentences.push_back("\tmv\t" + toString(ret) + "," + toString((rvREG)(18 + iter.offset / 4)));
      break;
    case VarArea::GLOBL:
      this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::LA, ret, NREG, NREG, NIMM, op->name).draw());
      if (op->type != ir::Type::IntPtr) this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::LW, ret, ret).draw());
      break;
    default:
      assert(0);
  }
  return ret;
}

rv::rvREG backend::Generator::getRs2(const ir::Operand* op) {
  VarLocation iter = this->find_operand(op->name);
  rvREG ret = aRegs.front();
  aRegs.pop_front();
  aRegs.push_back(ret);
  this->regTag->at((int)ret) = op->name;
  if (op->type == ir::Type::IntLiteral) {
    this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::LI, ret, NREG, NREG, std::stoi(op->name)).draw());
    return ret;
  }
  switch (iter.vararea) {
    case VarArea::LOCAL:
      this->sentences.push_back("\tlw\t" + toString(ret) + "," + std::to_string(iter.offset) + "(s0)");
      break;
    case VarArea::PARAM:
      if (iter.offset >= 0)
        this->sentences.push_back("\tlw\t" + toString(ret) + "," + std::to_string(iter.offset) + "(s0)");
      else
        this->sentences.push_back("\tmv\t" + toString(ret) + "," + toString((rvREG)(18 + iter.offset / 4)));
      break;
    case VarArea::GLOBL:
      this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::LA, ret, NREG, NREG, NIMM, op->name).draw());
      if (op->type != ir::Type::IntPtr) this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::LW, ret, ret).draw());
      break;
    default:
      assert(0);
  }
  return ret;
}

rv::rvFREG backend::Generator::fgetRd(const ir::Operand* op) {
  auto ret = aFRegs.front();
  aFRegs.pop_front();
  aFRegs.push_back(ret);
  regTag->at((int)ret + 32) = op->name;
  return ret;
}

rv::rvFREG backend::Generator::fgetRs1(const ir::Operand* op) {
  if (op->type == ir::Type::FloatLiteral) {
    return getfnum(op->name);
  }
  VarLocation iter = this->find_operand(op->name);
  rvFREG ret = aFRegs.front();
  aFRegs.pop_front();
  aFRegs.push_back(ret);
  this->regTag->at((int)ret + 32) = op->name;

  switch (iter.vararea) {
    case VarArea::LOCAL:
      this->sentences.push_back("\tflw\t" + toString(ret) + "," + std::to_string(iter.offset) + "(s0)");
      break;
    case VarArea::PARAM:
      if (iter.offset >= 0)
        this->sentences.push_back("\tflw\t" + toString(ret) + "," + std::to_string(iter.offset) + "(s0)");
      else
        this->sentences.push_back("\t" + rv_inst(rv::rvOPCODE::FADD_S, ret, (rvFREG)(18 + iter.offset / 4), NFREG).draw());
      break;
    case VarArea::GLOBL: {
      rvREG tmp_int_reg = rvREG::X5;
      this->sentences.push_back("\tla\t" + toString(tmp_int_reg) + ", " + op->name);

      if (op->type != ir::Type::FloatPtr) {
        this->sentences.push_back("\tflw\t" + toString(ret) + ", 0(" + toString(tmp_int_reg) + ")");
      }
      break;
    }
    default:
      assert(0);
  }
  return ret;
}

rv::rvFREG backend::Generator::fgetRs2(const ir::Operand* op) {
  if (op->type == ir::Type::FloatLiteral) {
    return getfnum(op->name);
  }
  VarLocation iter = this->find_operand(op->name);
  rvFREG ret = aFRegs.front();
  aFRegs.pop_front();
  aFRegs.push_back(ret);
  this->regTag->at((int)ret + 32) = op->name;
  switch (iter.vararea) {
    case VarArea::LOCAL:
      this->sentences.push_back("\tflw\t" + toString(ret) + "," + std::to_string(iter.offset) + "(s0)");
      break;
    case VarArea::PARAM:
      if (iter.offset >= 0)
        this->sentences.push_back("\tflw\t" + toString(ret) + "," + std::to_string(iter.offset) + "(s0)");
      else
        this->sentences.push_back("\t" + rv_inst(rv::rvOPCODE::FADD_S, ret, (rvFREG)(18 + iter.offset / 4), NFREG).draw());
      break;
    case VarArea::GLOBL: {
      rvREG tmp_int_reg = rvREG::X5;
      this->sentences.push_back("\tla\t" + toString(tmp_int_reg) + ", " + op->name);

      if (op->type != ir::Type::FloatPtr) {
        this->sentences.push_back("\tflw\t" + toString(ret) + ", 0(" + toString(tmp_int_reg) + ")");
      }
      break;
    }
    default:
      assert(0);
  }
  return ret;
}

rv::rvREG backend::Generator::getarr(const ir::Operand* op) {
  rvREG ret = aRegs.front();
  aRegs.pop_front();
  aRegs.push_back(ret);
  this->regTag->at((int)ret) = op->name;
  VarLocation iter = this->find_operand(op->name);
  if (iter.vararea == VarArea::LOCAL) {
    return getRs1(op);
  } else {
    this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::LA, ret, NREG, NREG, NIMM, op->name).draw());
    return ret;
  }
}

rv::rvREG backend::Generator::getnum(const std::string name) {
  if (name == "0") {
    return NREG;
  }
  rvREG ret = aRegs.front();
  aRegs.pop_front();
  aRegs.push_back(ret);
  this->regTag->at((int)ret) = name;
  this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::ADDI, ret, NREG, NREG, std::stoi(name)).draw());
  return ret;
}

int floatcnt = 0;

rvFREG backend::Generator::getfnum(const std::string name) {
  if (name == "0") {
    return NFREG;
  }
  rvFREG ret = aFRegs.front();
  aFRegs.pop_front();
  aFRegs.push_back(ret);
  this->regTag->at((int)ret + 32) = name;
  this->sentences.insert(this->globalSentences, "Floatnum" + std::to_string(floatcnt) + ":");
  this->sentences.insert(this->globalSentences, "\t.float " + name);
  rvREG base = aRegs.front();
  aRegs.pop_front();
  aRegs.push_back(base);
  this->sentences.push_back("\t" + rv_inst(rvOPCODE::LA, base, NREG, NREG, NIMM, "Floatnum" + std::to_string(floatcnt++)).draw());
  this->sentences.push_back("\t" + rv_inst(rvOPCODE::FLW, ret, base).draw());
  return ret;
}

void backend::Generator::gen() {
  this->sentences.emplace_back("\t.option\tnopic");

  this->global_varmap = new stackVarMap();
  gen_globalval(program.globalVal);
  this->sentences.emplace_back("\t.text");
  this->globalSentences = --this->sentences.end();
  for (auto func : program.functions) gen_func(func);
  int n = 0;
  for (const auto& sentence : this->sentences) {
    this->fout << sentence << "\n";
  }
  delete this->global_varmap;
}

void backend::Generator::gen_func(const ir::Function& func) {
  // refresh
  this->cur_varmap = new stackVarMap();
  this->param_varmap = new stackVarMap(0);
  this->regTag = new std::vector<std::string>(64, "");
  cur_funcname = func.name;
  this->aRegs = REGLIST;
  this->aFRegs = FREGLIST;

  this->sentences.push_back("\t.align\t1");
  this->sentences.push_back("\t.globl\t" + func.name);
  this->sentences.push_back("\t.type\t" + func.name + ",\t" + "@function");
  this->sentences.push_back(func.name + ":");

  this->sentences.push_back("\taddi\tsp,sp,-" + std::to_string(frame_size));
  this->sentences.push_back("\tsw\tra," + std::to_string(frame_size - 4) + "(sp)");
  this->sentences.push_back("\tsw\ts0," + std::to_string(frame_size - 8) + "(sp)");
  this->sentences.push_back("\tsw\ts1," + std::to_string(frame_size - 12) + "(sp)");
  this->sentences.push_back("\tsw\ts2," + std::to_string(frame_size - 16) + "(sp)");
  this->sentences.push_back("\tsw\ts3," + std::to_string(frame_size - 20) + "(sp)");

  this->sentences.push_back("\taddi\ts0,sp," + std::to_string(frame_size));
  this->sentences.push_back("\tfmv.w.x\tft0,zero");

  gen_paramval(func.ParameterList);
  for (auto instr : func.InstVec) {

    this->sentences.push_back("#" + std::to_string(genInstrCounter + 1) + ": " + instr->draw());

    gen_instr(*instr);
  }

  this->sentences.push_back(cur_funcname + "_return:");
  this->sentences.push_back("\tlw\tra," + std::to_string(frame_size - 4) + "(sp)");
  this->sentences.push_back("\tlw\ts0," + std::to_string(frame_size - 8) + "(sp)");
  this->sentences.push_back("\tlw\ts1," + std::to_string(frame_size - 12) + "(sp)");
  this->sentences.push_back("\tlw\ts2," + std::to_string(frame_size - 16) + "(sp)");
  this->sentences.push_back("\tlw\ts3," + std::to_string(frame_size - 20) + "(sp)");
  this->sentences.push_back("\taddi\tsp,sp," + std::to_string(frame_size));
  this->sentences.push_back("\tjr\tra");
  this->sentences.push_back("\t.size\t" + func.name + ",\t.-" + func.name);
  delete this->cur_varmap;
  delete this->param_varmap;
  delete this->regTag;
}

std::vector<std::list<std::string>::iterator> pc_ir2riscv;
std::unordered_multimap<int, std::string> GotoCounter;

void backend::Generator::gen_instr(const ir::Instruction& instr) {

  if (DBG) {
    std::cout << instr.draw() << std::endl;
  }

  pc_ir2riscv.push_back(--this->sentences.end());
  genInstrCounter++;
  auto itor = GotoCounter.equal_range(genInstrCounter);
  for (auto it = itor.first; it != itor.second; it = GotoCounter.erase(it)) {
    this->sentences.push_back(it->second + ":");
  }
  rvREG rd, rs1, rs2;
  rvFREG frd, frs1, frs2;
  int argument_cnt = 0;
  int fargument_cnt = 0;
  VarLocation varlocation;
  int offset = 0;
  switch (instr.op) {
    case ir::Operator::add:
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      rs2 = getRs2(&instr.op2);
      ensure_var_in_map(instr.des.name);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::ADD, rd, rs1, rs2).draw());
      this->freereg(rd);
      break;
    case ir::Operator::_goto:
      offset = std::stoi(instr.des.name);
      if (offset == 1) break;
      if (instr.op1.type != ir::Type::null) {
        if (instr.op1.type == ir::Type::FloatLiteral) {
          if (std::stof(instr.op1.name)) {
            if (offset > 0) {
              this->sentences.push_back("\t" + rv_inst(rvOPCODE::J, NREG, NREG, NREG, NIMM, ".MY" + std::to_string(genInstrCounter)).draw());
              GotoCounter.insert({genInstrCounter + offset, ".MY" + std::to_string(genInstrCounter)});
            } else {
              this->sentences.insert(std::next(pc_ir2riscv[genInstrCounter + offset]), ".MY" + std::to_string(genInstrCounter) + ":");
              this->sentences.push_back("\t" + rv_inst(rvOPCODE::J, NREG, NREG, NREG, NIMM, ".MY" + std::to_string(genInstrCounter)).draw());
            }
          }
        } else {
          rs1 = getRs1(&instr.op1);
          if (offset > 0) {
            this->sentences.push_back("\t" + rv_inst(rvOPCODE::BNE, rs1, NREG, NREG, NIMM, ".MY" + std::to_string(genInstrCounter)).draw());
            GotoCounter.insert({genInstrCounter + offset, ".MY" + std::to_string(genInstrCounter)});
          } else {
            this->sentences.insert(std::next(pc_ir2riscv[genInstrCounter + offset]), ".MY" + std::to_string(genInstrCounter) + ":");
            this->sentences.push_back("\t" + rv_inst(rvOPCODE::J, NREG, NREG, NREG, NIMM, ".MY" + std::to_string(genInstrCounter)).draw());
          }
        }
      } else {
        if (offset > 0) {
          this->sentences.push_back("\t" + rv_inst(rvOPCODE::J, NREG, NREG, NREG, NIMM, ".MY" + std::to_string(genInstrCounter)).draw());
          GotoCounter.insert({genInstrCounter + offset, ".MY" + std::to_string(genInstrCounter)});
        } else {
          this->sentences.insert(std::next(pc_ir2riscv[genInstrCounter + offset]), ".MY" + std::to_string(genInstrCounter) + ":");
          this->sentences.push_back("\t" + rv_inst(rvOPCODE::J, NREG, NREG, NREG, NIMM, ".MY" + std::to_string(genInstrCounter)).draw());
        }
      }
      break;
    case ir::Operator::call:
      for (auto argument : dynamic_cast<const ir::CallInst&>(instr).argumentList) {
        if (argument.type == ir::Type::FloatLiteral || argument.type == ir::Type::Float) {
          if (fargument_cnt < 8) {
            frd = (rvFREG)(10 + fargument_cnt);
            frs1 = fgetRs1(&argument);
            this->sentences.push_back("\t" + rv_inst(rvOPCODE::FADD_S, frd, frs1).draw());
          } else {
            assert(0 && "Not Realized");
          }
          fargument_cnt++;
        } else {
          if (argument_cnt < 8) {
            rd = (rvREG)(10 + argument_cnt);
            rs1 = getRs1(&argument);
            this->sentences.push_back("\t" + rv_inst(rvOPCODE::MOV, rd, rs1).draw());
          } else {
            rs1 = getRs1(&argument);
            this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::SW, rs1, rvREG::X2, NREG, 4 * argument_cnt - 32).draw());
          }
        }

        argument_cnt++;
      }
      this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::CALL, NREG, NREG, NREG, NIMM, instr.op1.name).draw());

      if (instr.des.type == ir::Type::null) {
        break;
      }
      if (instr.des.type == ir::Type::Int) {
        rd = getRd(&instr.des);
        this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::MOV, rd, REGA0, NREG, NIMM, instr.op1.name).draw());
        ensure_var_in_map(instr.des.name);
        this->freereg(rd);
      } else {
        frd = fgetRd(&instr.des);
        this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::FADD_S, frd, FREGA0, NFREG, NIMM, instr.op1.name).draw());
        ensure_var_in_map(instr.des.name);
        this->freereg(frd);
      }
      break;
    case ir::Operator::alloc:
      varlocation = find_operand(instr.des.name);
      if (varlocation.vararea == VarArea::GLOBL) break;
      ensure_var_in_map(instr.des.name);
      this->sentences.insert(this->globalSentences, "\t.data");
      this->sentences.insert(this->globalSentences, instr.des.name + ":");
      this->sentences.insert(this->globalSentences, "\t.space\t" + std::to_string(std::stoi(instr.op1.name) * 4));
      this->global_varmap->add_operand(instr.des.name);
      rd = this->getRd(&instr.des);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::LA, rd, NREG, NREG, NIMM, instr.des.name).draw());
      this->freereg(rd);
      break;
    case ir::Operator::store:
      rs1 = getarr(&instr.op1);
      rs2 = getRs1(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::SLLI, rs2, rs2, NREG, 2).draw());
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::ADD, rs1, rs1, rs2).draw());
      if (instr.op1.type == ir::Type::IntPtr) {
        rd = getRs2(&instr.des);
        this->sentences.push_back("\t" + rv_inst(rvOPCODE::SW, rd, rs1).draw());
      } else {
        frd = fgetRs2(&instr.des);
        this->sentences.push_back("\t" + rv_inst(rvOPCODE::FSW, frd, rs1).draw());
      }

      break;
    case ir::Operator::load:
      ensure_var_in_map(instr.des.name);
      rs2 = getRs2(&instr.op2);
      rs1 = getarr(&instr.op1);
      rd = getRd(&instr.des);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::SLLI, rs2, rs2, NREG, 2).draw());
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::ADD, rd, rs1, rs2).draw());
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::LW, rd, rd).draw());
      this->freereg(rd);
      break;
    case ir::Operator::getptr:
      ensure_var_in_map(instr.des.name);
      rs1 = getarr(&instr.op1);  // 数组基地址
      rs2 = getRs1(&instr.op2);  // 数组下标
      rd = getRd(&instr.des);    // 目标寄存器

      this->sentences.push_back("\t" + rv_inst(rvOPCODE::SLLI, rs2, rs2, NREG, 2).draw());
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::ADD, rd, rs1, rs2).draw());
      this->freereg(rd);
      break;
    case ir::Operator::def:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = instr.op1.name == "0" ? NREG : getRs1(&instr.op1);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::MOV, rd, rs1).draw());
      this->freereg(rd);
      break;
    case ir::Operator::fdef:
      ensure_var_in_map(instr.des.name);
      frd = fgetRd(&instr.des);
      frs1 = instr.op1.name == "0" ? NFREG : fgetRs1(&instr.op1);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::FADD_S, frd, frs1, NFREG).draw());
      this->freereg(frd);
      break;
    case ir::Operator::mov:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::MOV, rd, rs1, NREG, NIMM).draw());
      this->freereg(rd);
      break;
    case ir::Operator::fmov:
      ensure_var_in_map(instr.des.name);
      frd = fgetRd(&instr.des);
      frs1 = fgetRs1(&instr.op1);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::FADD_S, frd, frs1, NFREG).draw());
      this->freereg(frd);
      break;
    case ir::Operator::cvt_i2f:
      ensure_var_in_map(instr.des.name);
      frd = fgetRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      this->sentences.push_back("\tfrcsr\tt0");
      this->sentences.push_back("\tfsrmi\t2");
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::FCVT_S_W, frd, rs1).draw());
      this->sentences.push_back("\tfscsr\tt0");
      this->freereg(frd);
      break;

    case ir::Operator::cvt_f2i:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      frs1 = fgetRs1(&instr.op1);
      this->sentences.push_back("\tfrcsr\tt0");
      this->sentences.push_back("\tfsrmi\t2");
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::FCVT_W_S, rd, frs1, NIMM).draw());
      this->sentences.push_back("\tfscsr\tt0");
      this->freereg(rd);
      break;
    case ir::Operator::addi:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::ADDI, rd, rs1, NREG, std::stoi(instr.op2.name)).draw());
      this->freereg(rd);
      break;
    case ir::Operator::fadd:
      ensure_var_in_map(instr.des.name);
      frd = fgetRd(&instr.des);
      frs1 = fgetRs1(&instr.op1);
      frs2 = fgetRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::FADD_S, frd, frs1, frs2).draw());
      this->freereg(frd);
      break;
    case ir::Operator::sub:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      rs2 = getRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::SUB, rd, rs1, rs2).draw());
      this->freereg(rd);
      break;
    case ir::Operator::subi:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      this->sentences.push_back(+"\t" + rv_inst(rvOPCODE::ADDI, rd, rs1, NREG, -std::stoi(instr.op2.name)).draw());
      this->freereg(rd);
      break;
    case ir::Operator::fsub:
      ensure_var_in_map(instr.des.name);
      frd = fgetRd(&instr.des);
      frs1 = fgetRs1(&instr.op1);
      frs2 = fgetRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::FSUB_S, frd, frs1, frs2).draw());
      this->freereg(frd);
      break;
    case ir::Operator::mul:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      rs2 = getRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::MUL, rd, rs1, rs2).draw());
      this->freereg(rd);
      break;
    case ir::Operator::fmul:
      ensure_var_in_map(instr.des.name);
      frd = fgetRd(&instr.des);
      frs1 = fgetRs1(&instr.op1);
      frs2 = fgetRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::FMUL_S, frd, frs1, frs2).draw());
      this->freereg(frd);
      break;
    case ir::Operator::div:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      rs2 = getRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::DIV, rd, rs1, rs2).draw());
      this->freereg(rd);
      break;
    case ir::Operator::fdiv:
      ensure_var_in_map(instr.des.name);
      frd = fgetRd(&instr.des);
      frs1 = fgetRs1(&instr.op1);
      frs2 = fgetRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::FDIV_S, frd, frs1, frs2).draw());
      this->freereg(frd);
      break;
    case ir::Operator::lss:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      rs2 = getRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::SLT, rd, rs1, rs2).draw());
      this->freereg(rd);
      break;
    case ir::Operator::flss:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      frs1 = fgetRs1(&instr.op1);
      frs2 = fgetRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::FLT_S, rd, frs1, frs2).draw());
      this->freereg(rd);
      break;
    case ir::Operator::leq:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      rs2 = getRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::SLT, rd, rs2, rs1).draw());
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::XORI, rd, rd, NREG, 1).draw());
      this->freereg(rd);
      break;
    case ir::Operator::fleq:
      break;
    case ir::Operator::gtr:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      rs2 = getRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::SLT, rd, rs2, rs1).draw());
      this->freereg(rd);
      break;
    case ir::Operator::fgtr:
      break;
    case ir::Operator::geq:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      rs2 = getRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::SLT, rd, rs1, rs2).draw());
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::XORI, rd, rd, NREG, 1).draw());
      this->freereg(rd);
      break;
    case ir::Operator::fgeq:
      break;
    case ir::Operator::eq:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      rs2 = getRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::XOR, rd, rs1, rs2).draw());
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::SLTIU, rd, rd, NREG, 1).draw());
      this->freereg(rd);
      break;
    case ir::Operator::feq:
      break;
    case ir::Operator::neq:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      rs2 = getRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::XOR, rd, rs1, rs2).draw());
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::SLTIU, rd, rd, NREG, 1).draw());
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::XORI, rd, rd, NREG, 1).draw());
      this->freereg(rd);
      break;
    case ir::Operator::fneq:  // TODO:我的浮点数的IR所有fneq都是和0做的，实际上感觉实验里根本就不应该用fneq指令
                              // 所以直接把rs2当成0了，这是不对的
      ensure_var_in_map(instr.des.name);
      frd = fgetRd(&instr.des);
      frs1 = fgetRs1(&instr.op1);

      this->sentences.push_back("\tfmv.x.s " + toString(rvREG::X5) + ", " + toString(frs1));
      this->sentences.push_back("\tsnez " + toString(rvREG::X5) + ", " + toString(rvREG::X5));
      this->sentences.push_back("\tfcvt.s.w " + toString(frd) + ", " + toString(rvREG::X5));
      this->freereg(frd);
      break;
    case ir::Operator::mod:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      rs2 = getRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::REM, rd, rs1, rs2).draw());
      this->freereg(rd);
      break;
    case ir::Operator::_not:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::SEQZ, rd, rs1, rs2).draw());
      this->freereg(rd);
      break;
    case ir::Operator::_and:
      ensure_var_in_map(instr.des.name);
      rs1 = getRs1(&instr.op1);
      rs2 = getRs2(&instr.op2);
      rd = getRd(&instr.des);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::SLTIU, rs1, rs1, NREG, 1).draw());
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::XORI, rs1, rs1, NREG, 1).draw());
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::SLTIU, rs2, rs2, NREG, 1).draw());
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::XORI, rs2, rs2, NREG, 1).draw());
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::AND, rd, rs1, rs2).draw());
      this->freereg(rd);
      break;
    case ir::Operator::_or:
      ensure_var_in_map(instr.des.name);
      rd = getRd(&instr.des);
      rs1 = getRs1(&instr.op1);
      rs2 = getRs2(&instr.op2);
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::OR, rd, rs1, rs2).draw());
      this->freereg(rd);
      break;
    case ir::Operator::_return:
      if (instr.op1.type == ir::Type::null) break;
      if (instr.op1.type == ir::Type::Int || instr.op1.type == ir::Type::IntLiteral) {
        rs1 = getRs1(&instr.op1);
        this->sentences.push_back("\t" + rv_inst(rvOPCODE::MOV, REGA0, rs1).draw());
      } else {
        frs1 = fgetRs1(&instr.op1);
        this->sentences.push_back("\t" + rv_inst(rvOPCODE::FADD_S, FREGA0, frs1).draw());
      }
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::J, NREG, NREG, NREG, NIMM, cur_funcname + "_return").draw());
      break;
    case ir::Operator::__unuse__: {
      break;
    }
    default:
      assert(0 && "Fucking Unexpected Operater");
  }
}

void backend::Generator::gen_paramval(const std::vector<ir::Operand>& ParameterList) {
  this->param_varmap->offset = -32;
  for (const auto& val : ParameterList) {
    if (val.type == ir::Type::Float || val.type == ir::Type::FloatLiteral) {
      if (this->param_varmap->offset >= 0)
        this->sentences.push_back("\t" + rv_inst(rvOPCODE::FLW, FREGT0, REGS0, this->param_varmap->offset).draw());
      else
        this->sentences.push_back("\t" + rv_inst(rvOPCODE::FADD_S, FREGT0, (rvFREG)(18 + this->param_varmap->offset / 4)).draw());
      this->sentences.push_back("\t" + rv_inst(rvOPCODE::FSW, FREGT0, REGS0, this->cur_varmap->offset).draw());
    } else {
      if (this->param_varmap->offset >= 0)
        this->sentences.push_back("\tlw\t" + toString(REGT0) + "," + std::to_string(this->param_varmap->offset) + "(s0)");
      else
        this->sentences.push_back("\tmv\t" + toString(REGT0) + "," + toString((rvREG)(18 + this->param_varmap->offset / 4)));
      this->sentences.push_back("\tsw\t" + toString(REGT0) + "," + std::to_string(this->cur_varmap->offset) + "(s0)");
    }
    this->param_varmap->add_operand(val.name, -4);
    this->cur_varmap->add_operand(val.name);
  }
}

void backend::Generator::gen_globalval(const std::vector<ir::GlobalVal>& GlobalvalList) {
  this->sentences.push_back("\t.data");
  for (const auto& val : GlobalvalList) {
    if (val.val.type != ir::Type::IntLiteral && val.val.type != ir::Type::FloatLiteral) {
      if (val.maxlen == 0) {
        this->sentences.push_back(val.val.name + ":");
        this->sentences.push_back("\t.word\t0");
      } else {
        this->sentences.push_back(val.val.name + ":");
        this->sentences.push_back("\t.space\t" + std::to_string(val.maxlen * 4));
      }
      this->global_varmap->add_operand(val.val.name);
    }
  }
}

void backend::Generator::ensure_var_in_map(const std::string& var_name) {
  VarLocation varlocation = find_operand(var_name);
  if (varlocation.vararea != VarArea::GLOBL) this->cur_varmap->add_operand(var_name);
}
