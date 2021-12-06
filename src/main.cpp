#include <cassert>
#include <cstring>
#include <unistd.h>
#include "sysyast.hpp"

extern FILE* yyin;
FILE* f_log;
CBlkPtr CASTRoot;
CSymbolTable* sym_tab;

int yyparse();
int yylex();

int main(int argc, char** argv) {
  assert(strcmp("-S", argv[1]) == 0);

  string out_name;
  string log_name;

  if (argv[2][0] != '-') {
  // "./compiler -S testcase.c -o testcase.S"
  } else if (strcmp("-e", argv[2])  == 0) { 
  // "./compiler -S -e testcase.c -o testcase.S"
    yyin = fopen(argv[3], "r");
    out_name = string(argv[5]);
    log_name = out_name.substr(0, out_name.find("."));
    log_name.append(".log");
    f_log = fopen(log_name.c_str(), "w");
    assert(f_log);
  }

  FILE* f_out = fopen(out_name.c_str(), "w");
  assert(f_out);

  sym_tab = NEW(CSymbolTable)();
  do {
    yyparse();
  } while (!feof(yyin));

  CASTRoot->code_gen();
  for (string& code_line: CASTRoot->codes_)
    fprintf(f_out, "%s\n", code_line.c_str());
  
  fclose(yyin);
  fclose(f_out);
  fclose(f_log);
}
