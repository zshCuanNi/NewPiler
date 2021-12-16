#ifndef NEWPILER_H
#define NEWPILER_H

#include "c_ast.hpp"
#include "e_ast.hpp"

class Newpiler {
public:
  FILE* f_out_;
  CodeList eeyore_codes_;
  FILE* f_log_;
  CodeList tigger_codes_;
  ESymbolTable* e_sym_tab;
  CodeList riscv_codes_;

  // temp helper memeber vars
  EExprPtrList params_;
  vector<int> labels_;
  // used during tigger generation
  EEnFTabPtr func;
  VarRegMap var2reg;
  VarStackMap var2stack;
  RegStackMap reg2stack;
  set<string> used_regs;

  // s0, s1, s2 are reserved for storing temporary values
  vector<string> callee_saved_regs =
  {"s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11"};
  vector<string> caller_saved_regs =
  {"t0", "t1", "t2", "t3", "t4", "t5", "t6"};
  vector<string> param_regs =
  {"a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"};

  Newpiler(FILE* f_out, CodeList eeyore_codes, FILE* f_log = nullptr);
  // parse eeyore to get eeyore ast
  void parse_eeyore();
  void parse_eeyore_line(string code_line, int lineno);
  EExprPtr parse_eeyore_rval(string rval);
  EExprPtr parse_eeyore_expr(string str);
  EExprPtr parse_eeyore_expr(stringstream& sstr);

  // get CFG and do dataflow analysis
  void gen_control_flow_graph();
  void set_stmt_pred_succ();
  void order_stmts();
  void liveness_analysis();
  
  // linear scan register allocation
  void cal_live_intervals();
  void linear_scan();
  
  void compile_eeyore();
  void compile_eeyore_stmt(ENodePtr stmt);
  void parse_compile_tigger();
  void print_eeyore_codes();
  void print_tigger_codes();
  void print_riscv_codes();
  
  // for debug
  void eeyore_ast_debug();
  void eeyore_block_debug();
  void liveness_debug();
  void linear_scan_debug();

  // helper functions
  void save_regs(vector<string> regs);
  void restore_regs(vector<string> regs);
  string get_reg(EExprPtr rval, string specify_reg = "", bool is_load = true);
  string get_reg(string var_id, string specify_reg = "", bool is_load = true);
  string get_reg(int number, string specify_reg = "");
  void store_into_stack(string reg, string var_id);
  bool is_global(string var_id);
  bool is_arr(string var_id);
  bool is_out_of_int12(int int12);
  void gen_sw(string reg2, int int12, string reg1);
  void gen_lw(string reg1, int int12, string reg2);
  void gen_addi(string reg1, string reg2, int int12);
  void gen_slti(string reg1, string reg2, int int12);
  void gen_binop(string bop, string reg1, string reg2, string reg3);
};

bool start_with(string str, string head);
bool is_param(string var_id);
bool is_digit(char c);
char get_useful_peek(stringstream& sstr);
int get_unsigned_int(stringstream& sstr);

template<typename ... Args>
string format( const string& format, Args ... args );

#endif
