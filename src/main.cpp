#include <cassert>
#include <cstring>
#include <unistd.h>
#include "newpiler.hpp"

extern FILE* yyin;
FILE* f_log; // may not be used
CBlkPtr c_ast_root;
CSymbolTable* c_sym_tab;

int yyparse();

int main(int argc, char** argv) {
  assert(strcmp("-S", argv[1]) == 0);

  string out_name;
  string log_name;
  bool gen_eeyore = false, gen_tigger = false, gen_riscv = false;

  if (argv[2][0] != '-') {
  // "./build/compiler -S testcase.c -o testcase.S"
    gen_riscv = true;
    yyin = fopen(argv[2], "r");
    out_name = string(argv[4]);
    log_name = out_name.substr(0, out_name.find("."));
    log_name.append(".log");
    f_log = fopen(log_name.c_str(), "w");
    assert(f_log);
  } else if (strcmp("-e", argv[2])  == 0) {
  // "./build/compiler -S -e testcase.c -o testcase.S"
    gen_eeyore = true;
    yyin = fopen(argv[3], "r");
    out_name = string(argv[5]);
    log_name = out_name.substr(0, out_name.find("."));
    log_name.append(".log");
    f_log = fopen(log_name.c_str(), "w");
    assert(f_log);
  } else if (strcmp("-t", argv[2])  == 0) {
  // "./build/compiler -S -t testcase.c -o testcase.S"
    gen_tigger = true;
    yyin = fopen(argv[3], "r");
    out_name = string(argv[5]);
    log_name = out_name.substr(0, out_name.find("."));
    log_name.append(".log");
    f_log = fopen(log_name.c_str(), "w");
    assert(f_log);
  }

  FILE* f_out = fopen(out_name.c_str(), "w");
  assert(f_out);

  c_sym_tab = NEW(CSymbolTable)();
  do {
    yyparse();
  } while (!feof(yyin));

  c_ast_root->code_gen();
  Newpiler newpiler = Newpiler(f_out,c_ast_root->codes_, f_log);
  // newpiler.print_eeyore_codes();
  newpiler.parse_eeyore();
  newpiler.gen_control_flow_graph();
  newpiler.set_stmt_pred_succ();
  newpiler.order_stmts();
  if (gen_eeyore) {
    newpiler.eeyore_block_debug();
  } else {
    newpiler.liveness_analysis();
    newpiler.cal_live_intervals();
    // newpiler.liveness_debug();
    newpiler.linear_scan();
    newpiler.linear_scan_debug();
    newpiler.compile_eeyore();
    if (gen_tigger) {
      newpiler.print_tigger_codes();
    } else if (gen_riscv) {
      newpiler.parse_compile_tigger();
      newpiler.strength_reduction();
      newpiler.print_riscv_codes();
    }
  }

  fclose(yyin);
  fclose(f_out);
  fclose(f_log);
}
