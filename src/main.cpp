#include <cassert>
#include <cstring>
#include <unistd.h>
#include "newpiler.hpp"

extern FILE* yyin;
FILE* f_log; // may not be used
CBlkPtr C_AST_Root;
CSymbolTable* C_sym_tab;

int yyparse();

int main(int argc, char** argv) {
  assert(strcmp("-S", argv[1]) == 0);

  string out_name;
  string log_name;

  if (argv[2][0] != '-') {
  // "build/compiler -S testcase.c -o testcase.S"
  } else if (strcmp("-e", argv[2])  == 0) { 
  // "build/compiler -S -e testcase.c -o testcase.S"
    yyin = fopen(argv[3], "r");
    out_name = string(argv[5]);
    log_name = out_name.substr(0, out_name.find("."));
    log_name.append(".log");
    f_log = fopen(log_name.c_str(), "w");
    assert(f_log);
  }

  FILE* f_out = fopen(out_name.c_str(), "w");
  assert(f_out);

  C_sym_tab = NEW(CSymbolTable)();
  do {
    yyparse();
  } while (!feof(yyin));

  C_AST_Root->code_gen();
  Newpiler new_piler = Newpiler(f_out, C_AST_Root->codes_, f_log);
  // new_piler.print_eeyore_codes();
  new_piler.parse_eeyore();
  new_piler.gen_control_flow_graph();
  new_piler.eeyore_block_debug();
  fclose(yyin);
  fclose(f_out);
  fclose(f_log);
}
