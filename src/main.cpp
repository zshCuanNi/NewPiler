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
  bool gen_eeyore = false, gen_tigger = false, gen_riscv = false;

  if (argv[2][0] != '-') {
  // "build/compiler -S testcase.c -o testcase.S"
    gen_riscv = true;
    yyin = fopen(argv[2], "r");
    out_name = string(argv[4]);
    log_name = out_name.substr(0, out_name.find("."));
    log_name.append(".log");
    f_log = fopen(log_name.c_str(), "w");
    assert(f_log);
  } else if (strcmp("-e", argv[2])  == 0) {
  // "build/compiler -S -e testcase.c -o testcase.S"
    gen_eeyore = true;
    yyin = fopen(argv[3], "r");
    out_name = string(argv[5]);
    log_name = out_name.substr(0, out_name.find("."));
    log_name.append(".log");
    f_log = fopen(log_name.c_str(), "w");
    assert(f_log);
  } else if (strcmp("-t", argv[2])  == 0) {
  // "build/compiler -S -t testcase.c -o testcase.S"
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

  C_sym_tab = NEW(CSymbolTable)();
  do {
    yyparse();
  } while (!feof(yyin));

  C_AST_Root->code_gen();
  Newpiler new_piler = Newpiler(f_out, C_AST_Root->codes_, f_log);
  // new_piler.print_eeyore_codes();
  new_piler.parse_eeyore();
  new_piler.gen_control_flow_graph();
  new_piler.set_stmt_pred_succ();
  new_piler.order_stmts();
  if (gen_eeyore) {
    new_piler.eeyore_block_debug();
  } else {
    new_piler.liveness_analysis();
    new_piler.cal_live_intervals();
    // new_piler.liveness_debug();
    new_piler.linear_scan();
    new_piler.linear_scan_debug();
    new_piler.compile_eeyore();
    if (gen_tigger) {
      new_piler.print_tigger_codes();
    } else if (gen_riscv) {
      new_piler.parse_compile_tigger();
      new_piler.strength_reduction();
      new_piler.print_riscv_codes();
    }
  }

  fclose(yyin);
  fclose(f_out);
  fclose(f_log);
}
