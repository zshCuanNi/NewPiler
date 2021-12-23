#include <sstream>
#include <cassert>
#include "newpiler.hpp"

void Newpiler::strength_reduction() {
  for (auto it = riscv_codes_.begin(); it != riscv_codes_.end(); ++it) {
    auto&rc = *it;
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
        else if (multiplier == 1) {
          if (c0 == c1) rc = "";
          else
            rc = format("\t%s%s, %s",
              add_suffix("mv").c_str(),
              c0.c_str(),
              c1.c_str());
        } else {
          int k = 0;
          while (multiplier >>= 1) k++;
          rc = format("\t%s%s, %s, %d",
            add_suffix("slli").c_str(),
            c0.c_str(),
            c1.c_str(),
            k);
        }
      } else if (instr == "divi") {
      // divi Reg1, Reg2, (1|2|4|...)
      // divi instr should be replaced by mv(1) |
      // (2^k) & Reg2 != s0: srli s0, Reg2, 31; add s0, s0, Reg2; sra Reg1, s0, k
      // (2^k) & Reg2 == s0: srli Reg1, s0, 31; add s0, Reg1, s0; sra Reg1, s0, k
        int divisor = stoi(c2);
        if (divisor == 1) {
          if (c0 == c1) rc = "";
          else
            rc = format("\t%s%s, %s",
              add_suffix("mv").c_str(),
              c0.c_str(),
              c1.c_str());
        } else {
          int k = 0;
          while (divisor >>= 1) k++;
          if (c1 != "s0") {
            vector<string> to_insert = {
              format("\t%ss0, %s, 31",
                add_suffix("srli").c_str(),
                c1.c_str()),
              format("\t%ss0, s0, %s",
                add_suffix("add").c_str(),
                c1.c_str()),
              format("\t%s%s, s0, %d",
                add_suffix("sra").c_str(),
                c0.c_str(),
                k)
            };
            riscv_codes_.insert(it, to_insert.begin(), to_insert.end());
            auto new_it = it; new_it--;
            riscv_codes_.erase(it);
            it = new_it;
          } else if (c0 != "s0") {
            vector<string> to_insert = {
              format("\t%s%s, s0, 31",
                add_suffix("srli").c_str(),
                c0.c_str()),
              format("\t%ss0, %s, s0",
                add_suffix("add").c_str(),
                c0.c_str()),
              format("\t%s%s, s0, %d",
                add_suffix("sra").c_str(),
                c0.c_str(),
                k)
            };
            riscv_codes_.insert(it, to_insert.begin(), to_insert.end());
            auto new_it = it; new_it--;
            riscv_codes_.erase(it);
            it = new_it;
          } else assert(false);
        }
      } else if (instr == "remi") {
      // remi Reg1, Reg2, 2
      // should be replaced by andi Reg1, Reg2, 1
        int divisor = stoi(c2);
        if (divisor == 1) {
          rc = format("\t%s%s, x0",
            add_suffix("mv").c_str(),
            c0.c_str());
        } else {
          rc = format("\t%s%s, %s, 1",
            add_suffix("andi").c_str(),
            c0.c_str(),
            c1.c_str());
        }
      }
    }
  }
}
