#include <cassert>
#include "newpiler.hpp"

Newpiler::Newpiler(FILE* f_out, CodeList eeyore_codes, FILE* f_log):
f_out_(f_out), eeyore_codes_(move(eeyore_codes)), f_log_(f_log) {}

void Newpiler::print_eeyore_codes() {
  for (string& code_line: eeyore_codes_)
    fprintf(f_out_, "%s\n", code_line.c_str());
}

void Newpiler::print_tigger_codes() {
  for (string& code_line: tigger_codes_)
    fprintf(f_out_, "%s\n", code_line.c_str());
}
