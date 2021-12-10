#include <cassert>
#include "newpiler.hpp"

/* Print every eeyore stmt stored in e_sym_tab. */
void Newpiler::eeyore_debug() {
  assert(e_sym_tab);
  for (auto func_id: e_sym_tab->func_ids_) {
    auto func = e_sym_tab->func_tab_[func_id];
    if (func->func_id_ == "global") {
      for (auto id_var_pair: func->locals_) {
        auto var = id_var_pair.second;
        if (var->is_arr_)
          fprintf(f_out_, "var %d %s\n", var->width_, var->eeyore_id_.c_str());
        else
          fprintf(f_out_, "var %s\n", var->eeyore_id_.c_str());
      }
      for (auto stmt: func->stmts_)
        fprintf(f_out_, "%s\n", stmt->debug_print().c_str());
    } else {
      fprintf(f_out_, "%s [%d]\n", func->func_id_.c_str(), func->param_num_);
      for (auto id_var_pair: func->locals_) {
        auto var = id_var_pair.second;
        if (var->is_arr_)
          fprintf(f_out_, "\tvar %d %s\n", var->width_, var->eeyore_id_.c_str());
        else
          fprintf(f_out_, "\tvar %s\n", var->eeyore_id_.c_str());
      }
      for (auto stmt: func->stmts_) {
        string stmt_str = stmt->debug_print();
        // if has label prefix, no need to add heading \t
        if (!start_with(stmt_str, "l")) stmt_str = "\t" + stmt_str;
        fprintf(f_out_, "%s\n", stmt_str.c_str());
      }
      fprintf(f_out_, "end %s\n\n", func->func_id_.c_str());
    }
  }
}

string ENode::debug_print() {
  string ret_str = "";
  for (int label: labels_)
    ret_str.append(format("l%d:\n", label));
  if (ret_str.size() != 0) ret_str.append("\t");
  return ret_str;
}

string EExpr::debug_print() {
  // puts(typeid(*this).name());
  string ret_str = "";
  switch (expr_type_) {
    case eeOP:
      if (rhs_)
        ret_str = format("%s %s %s", lhs_->debug_print().c_str(), op_.c_str(), rhs_->debug_print().c_str());
      else
        ret_str = format("%s%s", op_.c_str(), lhs_->debug_print().c_str());
      break;
    case eeARR:
      ret_str = format("%s[%s]", lhs_->debug_print().c_str(), rhs_->debug_print().c_str());
      break;
    default: assert(false);
  }
  return ret_str;
}

string ESymbol::debug_print() {
  // puts(typeid(*this).name());
  string ret_str = ENode::debug_print();
  return ret_str.append(id_);
}

string ENumber::debug_print() {
  // puts(typeid(*this).name());
  string ret_str = ENode::debug_print();
  return ret_str.append(format("%d", val_));
}

/* generated debug text is of form:
 * param t0
 * \tparam 5
 * \tparam T1
 * \tcall f_func
 * Here params may not exist.
 */
string ECall::debug_print() {
  // puts(typeid(*this).name());
  string ret_str = ENode::debug_print();
  for (int i = 0; i < (int)params_.size(); i++)
    if (i == 0)
      ret_str.append("param " + params_[i]->debug_print() + "\n");
    else
      ret_str.append("\tparam " + params_[i]->debug_print() + "\n");
  ret_str.append("\tcall " + func_id_);
  return ret_str;
}

string EAssign::debug_print() {
  // puts(typeid(*this).name());
  string ret_str = ENode::debug_print();
  // special case: t = call f_func with params
  if (rhs_->expr_type_ == eeCALL) {
    string call_str = rhs_->debug_print();
    string::size_type pos = call_str.find_last_of("\n");
    if (pos != string::npos) {
    // has params
      string params_str = call_str.substr(0, pos + 1);
      call_str = call_str.substr(pos + 2, call_str.size() - (pos + 2));
      ret_str = ret_str + params_str + format("\t%s = %s", lhs_->debug_print().c_str(), call_str.c_str());
    } else {
    // has no params
      call_str = call_str.substr(1, call_str.size() - 1);
      ret_str.append(format("%s = %s", lhs_->debug_print().c_str(), call_str.c_str()));
    }
  } else
    ret_str.append(format("%s = %s", lhs_->debug_print().c_str(), rhs_->debug_print().c_str()));
  return ret_str;
}

string EReturn::debug_print() {
  // puts(typeid(*this).name());
  string ret_str = ENode::debug_print();
  return ret_str.append(ret_val_? "return " + ret_val_->debug_print(): "return");
}

string EIf::debug_print() {
  // puts(typeid(*this).name());
  string ret_str = ENode::debug_print();
  return ret_str.append(format("if %s goto l%d", cond_->debug_print().c_str(), where_));
}

string EGoto::debug_print() {
  // puts(typeid(*this).name());
  string ret_str = ENode::debug_print();
  return ret_str.append(format("goto l%d", where_));
}