#ifndef NEWPILER_H
#define NEWPILER_H

#include "c_ast.hpp"
#include "e_ast.hpp"

class Newpiler {
public:
  FILE* f_out_;
  CodeList eeyore_codes_;
  CodeList tigger_codes_;
  ESymbolTable* e_sym_tab;

  // temp helper memeber vars
  EExprPtrList params_;
  vector<int> labels_;

  Newpiler(FILE* f_out, CodeList eeyore_codes);
  void parse_eeyore();
  void parse_eeyore_line(string code_line, int lineno);
  EExprPtr parse_eeyore_rval(string rval);
  EExprPtr parse_eeyore_expr(string str);
  EExprPtr parse_eeyore_expr(stringstream& sstr);
  void eeyore_debug();
  void print_eeyore_codes();
  void print_tigger_codes();
};

bool start_with(string& str, string head);

#endif
