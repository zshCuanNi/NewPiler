#include <sstream>
#include <cassert>
#include "newpiler.hpp"

inline bool start_with(string& str, string head) {
  return str.compare(0, head.size(), head) == 0;
}

inline bool is_digit(char c) {
  return c >= '0' && c <= '9';
}

inline char get_useful_peek(stringstream& sstr) {
  char ch;
  for (ch = sstr.peek(); ch == ' ' || ch == '\t'; ch = sstr.peek())
    sstr.get();
  return ch;
}

inline int get_unsigned_int(stringstream& sstr) {
  while (!is_digit(sstr.peek())) sstr.get();
  int ret_int;
  sstr >> ret_int;
  return ret_int;
}

void Newpiler::parse_eeyore() {
  assert(!eeyore_codes_.empty());
  e_sym_tab = NEW(ESymbolTable)();
  int lineno = 1;
  for (string& code_line: eeyore_codes_) {
    // printf("line %d\n", lineno);
    if (code_line.empty()) {
      lineno++;
      continue;
    }
    parse_eeyore_line(code_line, lineno++);
  }
}

void Newpiler::parse_eeyore_line(string code_line, int lineno) {
  stringstream line_in(code_line);
  ENodePtr stmt=nullptr;
  string line_head;

  line_in >> line_head;

  // var (NUM) SYMBOL
  if (start_with(line_head, "var")) {
    int width;
    string var_id;
    if (is_digit(get_useful_peek(line_in))) {
    // var NUM SYMBOL
      line_in >> width >> var_id;
      e_sym_tab->register_var(var_id, true, width);
    } else {
    // var SYMBOL
      line_in >> var_id;
      e_sym_tab->register_var(var_id);
    }
  } else
  // f_func [NUM]
  if (start_with(line_head, "f_")) {
    int param_num;
    param_num = get_unsigned_int(line_in);
    e_sym_tab->register_func(line_head, param_num);
  } else
  // end f_func
  if (start_with(line_head, "end")) {
    // don't forget to get back to the global scope
    e_sym_tab->cur_func_ = "global";
  } else
  // if RVal LogicOp RVal goto ld
  if (start_with(line_head, "if")) {
    string lhs, op, rhs;
    int go_where;
    line_in >> lhs >> op >> rhs;
    go_where = get_unsigned_int(line_in);
    EExprPtr lhs_node = parse_eeyore_rval(lhs);
    EExprPtr rhs_node = parse_eeyore_rval(rhs);
    stmt = NEW(EIf)(NEW(EExpr)(eeOP, op, lhs_node, rhs_node), go_where);
  } else
  // goto ld
  if (start_with(line_head, "goto")) {
    int go_where;
    go_where = get_unsigned_int(line_in);
    stmt = NEW(EGoto)(go_where);
  } else
  // param RVal
  if (start_with(line_head, "param")) {
    string param;
    line_in >> param;
    EExprPtr param_node = parse_eeyore_rval(param);
    params_.push_back(param_node);
  } else
  // call f_func
  if (start_with(line_head, "call")) {
    string func_id;
    line_in >> func_id;
    stringstream sstr("call " + func_id);
    stmt = parse_eeyore_expr(sstr);
  } else
  // return (RVal)
  if (start_with(line_head, "return")) {
    if (line_in.peek() == EOF)
      stmt = NEW(EReturn)();
    else {
      string rval;
      line_in >> rval;
      EExprPtr ret_val = parse_eeyore_rval(rval);
      stmt = NEW(EReturn)(ret_val);
    }
  } else
  // ld:
  if (start_with(line_head, "l")) {
    labels_.push_back(stoi(line_head.substr(1, line_head.size()-2)));
  } else {
  // SYMBOL = {RVal | call f_func | UnOp RVal | RVal BinOp RVal | SYMBOL[RVal]}
  // SYMBOL[RVal] = RVal
    string lhs(line_head), str_asn;
    line_in >> str_asn;
    EExprPtr lhs_node = parse_eeyore_expr(lhs);
    EExprPtr rhs_node = parse_eeyore_expr(line_in);
    stmt = NEW(EAssign)(lhs_node, rhs_node);
  }
  
  // for those who generated stmt, add it to function table and set labels
  if (stmt) {
    stmt->lineno_ = lineno;
    e_sym_tab->add_stmt(stmt);
    if (labels_.empty()) return;
    // deal with labels (some maybe redundant) of the form:
    // l1:
    // l2:
    //     t0 = 1
    stmt->labels_ = labels_;
    EEnFTabPtr func = e_sym_tab->get_cur_func();
    for (int label: labels_)
      func->label_to_node_[label] = stmt;
    labels_.clear();
  }
}

EExprPtr Newpiler::parse_eeyore_rval(string rval) {
  // Number includes non-negative and negative numbers.
  if (is_digit(rval[0]) || (rval[0] == '-' && is_digit(rval[1])))
    return NEW(ENumber)(stoi(rval));
  else
    return NEW(ESymbol)(rval);
}

EExprPtr Newpiler::parse_eeyore_expr(string str) {
  stringstream sstr(str);
  return parse_eeyore_expr(sstr);
}

EExprPtr Newpiler::parse_eeyore_expr(stringstream& sstr) {
  EExprPtr ret_node;
  string c0, c1, c2;
  sstr >> c0 >> c1 >> c2;
  if (!c2.empty()) {
  // RVal BinOp RVal
    EExprPtr lhs_node = parse_eeyore_rval(c0);
    EExprPtr rhs_node = parse_eeyore_rval(c2);
    ret_node = NEW(EExpr)(eeOP, c1, lhs_node, rhs_node);
  } else if (!c1.empty()) {
    if (start_with(c0, "call")) {
    // call f_func
      ret_node = NEW(ECall)(c1, params_);
      params_.clear();
    } else
    // UnOp RVal
      ret_node = NEW(EExpr)(eeOP, c0, parse_eeyore_rval(c1));
  } else {
    if (*c0.rbegin() == ']') {
    // SYMBOL[RVal]
      char symbol[32], index[32];
      sscanf(c0.c_str(), "%[^[ ]%*[[ ]%[^] ]", symbol, index);
      EExprPtr sym_node = parse_eeyore_rval(symbol);
      EExprPtr ind_node = parse_eeyore_rval(index);
      ret_node = NEW(EExpr)(eeARR, "", sym_node, ind_node);
    } else
    // RVal
      ret_node = parse_eeyore_rval(c0);
  }
  return ret_node;
}
