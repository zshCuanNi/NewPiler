#include <cassert>
#include "newpiler.hpp"

Newpiler::Newpiler(FILE* f_out, CodeList eeyore_codes, FILE* f_log)
: f_out_(f_out), eeyore_codes_(move(eeyore_codes)), f_log_(f_log) {
  // parse_eeyore();
  // gen_control_flow_graph();
  // set_stmt_pred_succ();
  // order_stmts();
  // // eeyore_block_debug();
  // liveness_analysis();
  // cal_live_intervals();
  // // liveness_debug();
  // linear_scan();
  // // linear_scan_debug();
  // compile_eeyore();
  // print_tigger_codes();
}

void Newpiler::print_eeyore_codes() {
  for (string& code_line: eeyore_codes_)
    fprintf(f_out_, "%s\n", code_line.c_str());
}

void Newpiler::print_tigger_codes() {
  for (string& code_line: tigger_codes_)
    fprintf(f_out_, "%s\n", code_line.c_str());
}
