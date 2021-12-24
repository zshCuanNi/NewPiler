#include <map>
#include "newpiler.hpp"

#define RPUSH riscv_codes_.push_back

bool Newpiler::is_out_of_int12(int int12) {
  if (int12 >= -2048 && int12 < 2048) return false;
  RPUSH(format("\t%ss0, %d", add_suffix("li"), int12));
  return true;
}

/* sw reg2, int12(reg1) */
void Newpiler::gen_sw(string reg2, int int12, string reg1) {
  if (is_out_of_int12(int12)) {
    RPUSH(format("\t%ss0, s0, %s", add_suffix("add"), reg1));
    RPUSH(format("\t%s%s, 0(s0)", add_suffix("sw"), reg2));
  } else
    RPUSH(format("\t%s%s, %d(%s)", add_suffix("sw"), reg2, int12, reg1));
}

/* lw reg1, int12(reg2) */
void Newpiler::gen_lw(string reg1, int int12, string reg2) {
  if (is_out_of_int12(int12)) {
    RPUSH(format("\t%ss0, s0, %s", add_suffix("add"), reg2));
    RPUSH(format("\t%s%s, 0(s0)", add_suffix("lw"), reg1));
  } else
    RPUSH(format("\t%s%s, %d(%s)", add_suffix("lw"), reg1, int12, reg2));
}

/* addi reg1, reg2, int12 */
void Newpiler::gen_addi(string reg1, string reg2, int int12) {
  if (is_out_of_int12(int12))
    RPUSH(format("\t%s%s, %s, s0", add_suffix("add"), reg1, reg2));
  else
    RPUSH(format("\t%s%s, %s, %d", add_suffix("addi"), reg1, reg2, int12));
}

/* slti reg1, reg2, int12 */
void Newpiler::gen_slti(string reg1, string reg2, int int12) {
  if (is_out_of_int12(int12))
    RPUSH(format("\t%s%s, %s, s0", add_suffix("slt"), reg1, reg2));
  else
    RPUSH(format("\t%s%s, %s, %d", add_suffix("slti"), reg1, reg2, int12));
}

/* reg1 = reg2 bop reg3 => bop_instr reg1, reg2, reg3 ... */
void Newpiler::gen_binop(string bop, string reg1, string reg2, string reg3) {
  if (bop == "&&") {
    RPUSH(format("\t%s%s, %s", add_suffix("snez"), reg1, reg2));
    RPUSH(format("\t%ss0, %s", add_suffix("snez"), reg3));
    RPUSH(format("\t%s%s, %s, s0", add_suffix("and"), reg1, reg1));
    return;
  }

  std::map<string, string> op2instr = {
    {"+", "add"}, {"-", "sub"}, {"*", "mul"}, {"/", "div"}, {"%", "rem"},
    {"<", "slt"}, {">", "sgt"}, {"<=", "sgt"}, {">=", "slt"},
    {"||", "or"}, {"!=", "xor"}, {"==", "xor"}
  };
  RPUSH(format("\t%s%s, %s, %s", add_suffix(op2instr[bop]), reg1, reg2, reg3));
  
  if (bop == "<=" || bop == ">=" || bop == "==")
    RPUSH(format("\t%s%s, %s", add_suffix("seqz"), reg1, reg1));
  if (bop == "||" || bop == "!=")
    RPUSH(format("\t%s%s, %s", add_suffix("snez"), reg1, reg1));
}

bool is_power_of_2(int x) { return (x & (x - 1)) == 0; }

void Newpiler::parse_compile_tigger() {
  assert(!tigger_codes_.empty());
  int STK;
  for (string& code_line: tigger_codes_) {
    if (code_line.empty()) continue;

    std::stringstream line_in(code_line);
    string line_head;

    line_in >> line_head;

    // Vk = (malloc) NUM
    if (start_with(line_head, "v")) {
      string var, eq_str;
      int int_val;
      var = line_head;
      line_in >> eq_str;
      if (get_useful_peek(line_in) == 'm') {
      // vk = malloc NUM
        string m_str;
        line_in >> m_str >> int_val;
        RPUSH(format("\t%s%s, %d, 4", add_suffix(".comm"), var, int_val));
      } else {
      // vk = NUM
        line_in >> int_val;
        RPUSH("\t" + add_suffix(".global") + var);
        RPUSH("\t" + add_suffix(".section") + ".sdata");
        RPUSH("\t" + add_suffix(".align") + "2");
        RPUSH("\t" + add_suffix(".type") + var + ", @object");
        RPUSH("\t" + add_suffix(".size") + var + ", 4");
        RPUSH(var + ":");
        RPUSH(format("\t%s%d", add_suffix(".word"), int_val));
      }
    } else
    // f_func [NUM] [NUM]
    if (start_with(line_head, "f_")) {
      string func = line_head.substr(2, line_head.length());
      char bra_ch;
      int int1, int2;
      line_in >> bra_ch >> int1 >> bra_ch >> bra_ch >> int2;
      STK = (int2 / 4 + 1) * 16;
      RPUSH("\t" + add_suffix(".text"));
      RPUSH("\t" + add_suffix(".align") + "2");
      RPUSH("\t" + add_suffix(".global") + func);
      RPUSH("\t" + add_suffix(".type") + func + ", @function");
      RPUSH(func + ":");
      gen_addi("sp", "sp", -STK);
      gen_sw("ra", STK - 4, "sp");
    } else
    // end f_func
    if (start_with(line_head, "end")) {
      string func;
      line_in >> func;
      func = func.substr(2, func.length());
      RPUSH(format("\t%s%s, .-%s", add_suffix(".size"), func, func));
    } else
    // if Reg LOGICOP Reg goto ld
    if (start_with(line_head, "if")) {
      string reg1, lop, reg2, goto_str, label;
      line_in >> reg1 >> lop >> reg2 >> goto_str >> label;
      std::map<string, string> op2instr = {
        {"<", "blt"}, {">", "bgt"}, {"<=", "ble"},
        {">=", "bge"}, {"!=", "bne"}, {"==", "beq"}
      };
      RPUSH(format("\t%s%s, %s, .%s",
                    add_suffix(op2instr[lop]),
                    reg1,
                    reg2,
                    label));
    } else
    // goto ld
    if (start_with(line_head, "goto")) {
      string label;
      line_in >> label;
      RPUSH("\t" + add_suffix("j") + "." + label);
    } else
    // loadaddr NUM/vk Reg
    if (start_with(line_head, "loadaddr")) {
      string reg;
      if (get_useful_peek(line_in) == 'v') {
      // loadaddr vk Reg
        string var;
        line_in >> var >> reg;
        RPUSH(format("\t%s%s, %s", add_suffix("la"), reg, var));
      } else {
      // loadaddr NUM Reg
        int int10;
        line_in >> int10 >> reg;
        gen_addi(reg, "sp", int10 * 4);
      }
    } else
    // load NUM/vk Reg
    if (start_with(line_head, "load")) {
      string reg;
      if (get_useful_peek(line_in) == 'v') {
      // load vk Reg
        string var;
        line_in >> var >> reg;
        RPUSH(format("\t%s%s, %%hi(%s)", add_suffix("lui"), reg, var));
        RPUSH(format("\t%s%s, %%lo(%s)(%s)", add_suffix("lw"), reg, var, reg));
      } else {
      // load NUM Reg
        int int10;
        line_in >> int10 >> reg;
        gen_lw(reg, int10 * 4, "sp");
      }
    } else
    // ld:
    if (start_with(line_head, "l")) {
      string label = line_head;
      RPUSH("." + label);
    } else
    // call f_func
    if (start_with(line_head, "call")) {
      string func;
      line_in >> func;
      func = func.substr(2, func.length());
      RPUSH("\t" + add_suffix("call") + func);
    } else
    // return
    if (start_with(line_head, "return")) {
      gen_lw("ra", STK - 4, "sp");
      gen_addi("sp", "sp", STK);
      RPUSH("\tret");
    } else
    // store Reg NUM
    if (start_with(line_head, "store")) {
      string reg;
      int int10;
      line_in >> reg >> int10;
      gen_sw(reg, int10 * 4, "sp");
    } else {
    // Reg = Reg BinOp Reg/NUM | OP Reg | Reg/NUM | Reg[NUM]
    // Reg[NUM] = Reg
      if (line_head.find('[') != line_head.npos) {
      // Reg[NUM] = Reg
        int l_bra = line_head.find('[');
        int r_bra = line_head.find(']');
        string reg1, index_str, eq_str, reg2;
        int index;
        reg1 = line_head.substr(0, l_bra);
        index_str = line_head.substr(l_bra + 1, r_bra - l_bra - 1);
        index = stoi(index_str);
        line_in >> eq_str >> reg2;
        gen_sw(reg2, index, reg1);
      } else {
      // Reg = ...
        string reg1, eq_str;
        reg1 = line_head;
        line_in >> eq_str;
        string c0, c1, c2;
        line_in >> c0 >> c1 >> c2;
        if (!c2.empty()) {
        // Reg = Reg BinOp Reg/NUM
          if (is_digit(c2[0]) || c2[0] == '-') {
          // Reg = Reg BinOp NUM
            int int12 = stoi(c2);
            if (c1 == "+") gen_addi(reg1, c0, int12);
            else if (c1 == "<") gen_slti(reg1, c0, int12);
            else if (c1 == "*" && is_power_of_2(int12)) {
            // optimiation: Reg = Reg * (0|1|2|4|...) ==> muli Reg, Reg, (0|1|2|4|...)
            // muli instr should be replaced by li(0) | mv(1) | slli(2^k)
            // in Strength Reduction
              RPUSH(format("\t%s%s, %s, %d",
                            add_suffix("muli"),
                            reg1,
                            c0,
                            int12));
            }  else if (c1 == "/" && is_power_of_2(int12)) {
            // optimiation: Reg1 = Reg2 / (1|2|4|...) ==> divi Reg1, Reg2, (1|2|4|...)
            // divi instr should be replaced by
            // mv(1) |
            // (2^k) & Reg2 != s0: srli s0, Reg2, 31; add s0, s0, Reg2; sra Reg1, s0, k
            // (2^k) & Reg2 == s0: srli Reg1, s0, 31; add s0, Reg1, s0; sra Reg1, s0, k
            // in Strength Reduction
              RPUSH(format("\t%s%s, %s, %d",
                            add_suffix("divi"),
                            reg1,
                            c0,
                            int12));
            } else if (c1 == "%" && (int12 == 2 || int12 == 1)) {
            // optimiation: Reg1 = Reg2 / 2 ==> remi Reg1, Reg2, 2
            // this instr should be replaced by
            // Reg2 != s0: srli s0, Reg2, 31; add Reg1, s0, Reg2
            // Reg2 == s0: srli Reg1, s0, 31; add Reg1, Reg1, s0
            // in Strength Reduction
              RPUSH(format("\t%s%s, %s, %d",
                            add_suffix("remi"),
                            reg1,
                            c0,
                            int12));
            } else {
              RPUSH(format("\t%ss0, %d", add_suffix("li"), int12));
              gen_binop(c1, reg1, c0, "s0");
            }
          } else {
          // Reg = Reg BinOp Reg
            gen_binop(c1, reg1, c0, c2);
          }
        } else if (!c1.empty()) {
        // Reg = OP Reg
          std::map<string, string> op2instr = {{"-", "neg"}, {"!", "seqz"}};
          RPUSH(format("\t%s%s, %s", add_suffix(op2instr[c0]), reg1, c1));
        } else if (c0.find('[') != c0.npos) {
        // Reg = Reg[NUM]
          int l_bra = c0.find('[');
          int r_bra = c0.find(']');
          string reg2, index_str;
          int index;
          reg2 = c0.substr(0, l_bra);
          index_str = c0.substr(l_bra + 1, r_bra - l_bra - 1);
          index = stoi(index_str);
          gen_lw(reg1, index, reg2);
        } else {
        // Reg = Reg/NUM
          if (is_digit(c0[0]) || c0[0] == '-')
          // Reg = NUM
            RPUSH(format("\t%s%s, %s", add_suffix("li"), reg1, c0));
          else
          // Reg = Reg
            RPUSH(format("\t%s%s, %s", add_suffix("mv"), reg1, c0));
        }
      }
    }
  }
}
