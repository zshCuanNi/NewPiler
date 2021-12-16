#include <sstream>
#include "newpiler.hpp"

void Newpiler::strength_reduction() {
  for (auto& rc: riscv_codes_) {
    if (rc[0] != '\t') continue;
    stringstream ss_rc(rc);
    if (get_useful_peek(ss_rc) == '.') continue;

    string instr, c0, c1, c2;
    char instr_ch[32], ch0[32], ch1[32], ch2[32];
    int res;
    res = sscanf(rc.c_str(), "%s %[^,], %[^,], %s", instr_ch, ch0, ch1, ch2);
    instr = instr_ch;
    // .label | ret
    if (res == 1) continue;
    // j .label | call func
    if (res >= 2) c0 = ch0;
    // li/seqz/snez/neg/mv/sw/lw/la/lui
    if (res >= 3) c1 = ch1;
    // BinOp reg1, reg2, reg3/int12 | bOp reg1, reg2, .label
    if (res >= 4) {
      c2 = ch2;
      if (instr == "muli") {
      // muli Reg, Reg, (0|1|2|4|...)
      // muli instr should be replaced by li(0) | mv(1) | slli(2^k)
        int multiplier = stoi(c2);
        if (multiplier == 0)
          rc = format("\t%s%s, 0", add_suffix("li").c_str(), c0.c_str());
        else if (multiplier == 1)
          rc = format("\t%s%s, %s",
            add_suffix("mv").c_str(),
            c0.c_str(),
            c1.c_str());
        else {
          int k = 0;
          while (multiplier >>= 1) k++;
          rc = format("\t%s%s, %s, %d",
            add_suffix("slli").c_str(),
            c0.c_str(),
            c1.c_str(),
            k);
        }
      }
    }
  }
}