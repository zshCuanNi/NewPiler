#include "newpiler.hpp"

string ENode::debug_print() {
  string ret_str = "";
  for (int label: labels_)
    ret_str.append(format("l%d:\n", label));
  if (ret_str.size() != 0) ret_str.append("\t");
  return ret_str;
}

string EExpr::debug_print() {
  string ret_str = "";
  switch (expr_type_) {
    case eeOP:
      if (rhs_)
        ret_str = format("%s %s %s", lhs_->debug_print(), op_, rhs_->debug_print());
      else
        ret_str = format("%s%s", op_, lhs_->debug_print());
      break;
    case eeARR:
      ret_str = format("%s[%s]", lhs_->debug_print(), rhs_->debug_print());
      break;
    default: assert(false);
  }
  return ret_str;
}

string ESymbol::debug_print() {
  string ret_str = ENode::debug_print();
  return ret_str.append(id_);
}

string ENumber::debug_print() {
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
  string ret_str = ENode::debug_print();
  // special case: t = call f_func with params
  if (rhs_->expr_type_ == eeCALL) {
    string call_str = rhs_->debug_print();
    string::size_type pos = call_str.find_last_of("\n");
    if (pos != call_str.npos) {
    // has params
      string params_str = call_str.substr(0, pos + 1);
      call_str = call_str.substr(pos + 2, call_str.size() - (pos + 2));
      ret_str = ret_str + params_str + format("\t%s = %s", lhs_->debug_print(), call_str);
    } else {
    // has no params
      call_str = call_str.substr(1, call_str.size() - 1);
      ret_str.append(format("%s = %s", lhs_->debug_print(), call_str));
    }
  } else
    ret_str.append(format("%s = %s", lhs_->debug_print(), rhs_->debug_print()));
  return ret_str;
}

string EReturn::debug_print() {
  string ret_str = ENode::debug_print();
  return ret_str.append(ret_val_? "return " + ret_val_->debug_print(): "return");
}

string EIf::debug_print() {
  string ret_str = ENode::debug_print();
  return ret_str.append(format("if %s goto l%d", cond_->debug_print(), where_));
}

string EGoto::debug_print() {
  string ret_str = ENode::debug_print();
  return ret_str.append(format("goto l%d", where_));
}

void gen_var_decls(FILE* f_out, EVarTable& var_table, bool is_indent) {
  string indent = is_indent? "\t": "";
  for (auto id_var_pair: var_table) {
        auto var = id_var_pair.second;
        if (var->is_arr_)
          fprintf(f_out, "%svar %d %s\n",
            indent.c_str(), var->width_, var->eeyore_id_.c_str());
        else
          fprintf(f_out, "%svar %s\n",
            indent.c_str(), var->eeyore_id_.c_str());
      }
}

/* Print eeyore codes stored in e_sym_tab according to eeyore ast. */
void Newpiler::eeyore_ast_debug() {
  FILE* f_out = f_out_;
  assert(e_sym_tab);
  assert(f_out);
  for (auto func_id: e_sym_tab->func_ids_) {
    auto func = e_sym_tab->find_func(func_id);
    if (func->func_id_ == "global") {
      gen_var_decls(f_out, func->locals_, false);
      for (auto stmt: func->stmts_)
        fprintf(f_out, "%s\n", stmt->debug_print().c_str());
    } else {
      fprintf(f_out, "%s [%d]\n", func->func_id_.c_str(), func->param_num_);
      gen_var_decls(f_out, func->locals_, true);
      for (auto stmt: func->stmts_) {
        string stmt_str = stmt->debug_print();
        // if has label prefix, no need to add heading \t
        if (!start_with(stmt_str, "l")) stmt_str = "\t" + stmt_str;
        fprintf(f_out, "%s\n", stmt_str.c_str());
      }
      fprintf(f_out, "end %s\n\n", func->func_id_.c_str());
    }
  }
}

/* Print eeyore codes after basic block division.
 * have cleared dead codes and redundant labels
 */
void Newpiler::eeyore_block_debug() {
  FILE* f_out = f_out_;
  assert(e_sym_tab);
  assert(f_out);
  for (auto func_id: e_sym_tab->func_ids_) {
    auto func = e_sym_tab->find_func(func_id);
    if (func->func_id_ == "global") {
      gen_var_decls(f_out, func->locals_, false);
      for (auto stmt: func->stmts_)
        fprintf(f_out, "%s\n", stmt->debug_print().c_str());
    } else {
      fprintf(f_out, "%s [%d]\n", func->func_id_.c_str(), func->param_num_);
      gen_var_decls(f_out, func->locals_, true);
      for (auto b_blk: func->blocks_) {
        for (auto stmt: b_blk->stmts_) {
          string stmt_str = stmt->debug_print();
          // if has label prefix, no need to add heading \t
          if (!start_with(stmt_str, "l")) stmt_str = "\t" + stmt_str;
          fprintf(f_out, "%s\n", stmt_str.c_str());
        }
      }
      fprintf(f_out, "end %s\n\n", func->func_id_.c_str());
    }
  }
}

/* Print liveness information for debug */
void Newpiler::liveness_debug() {
  FILE* f_out = f_log_;
  assert(e_sym_tab);
  assert(f_out);
  for (auto func_id: e_sym_tab->func_ids_) {
    auto func = e_sym_tab->find_func(func_id);
    if (func->func_id_ == "global") continue;
    fprintf(f_out, "FUNCTION %s BEGIN\n", func->func_id_.c_str());
    for (auto& li: func->live_intervals_)
      fprintf(f_out, "var %4s [%d, %d]\n", li.var_id_.c_str(), li.start_, li.end_);
    for (auto stmt: func->stmts_) {
      string stmt_str = stmt->debug_print();
      if (!start_with(stmt_str, "l")) stmt_str = "\t" + stmt_str;
      string info_str = format("\t\t\t\tstmt-%d: ", stmt->stmtno_);
      info_str.append("\tdef: ");
      for (auto var: stmt->def_) info_str.append(format("%s,", var));
      info_str.append("\tuse: ");
      for (auto var: stmt->use_) info_str.append(format("%s,", var));
      info_str.append("\tin: ");
      for (auto var: stmt->live_in_) info_str.append(format("%s,", var));
      info_str.append("\tout: ");
      for (auto var: stmt->live_out_) info_str.append(format("%s,", var));
      fprintf(f_out, "%s\n", stmt_str.append(info_str).c_str());
    }
    fprintf(f_out, "FUNCTION %s END\n", func->func_id_.c_str());
  }
}

/* Print results of Linear Scan Register Allocation for debug */
void Newpiler::linear_scan_debug() {
  FILE* f_out = f_log_;
  assert(e_sym_tab);
  assert(f_out);
  for (auto func_id: e_sym_tab->func_ids_) {
    auto func = e_sym_tab->find_func(func_id);
    if (func->func_id_ == "global") continue;
    fprintf(f_out, "%s [%d]\n", func->func_id_.c_str(), func->param_num_);
    gen_var_decls(f_out, func->locals_, true);
    for (auto& li: func->live_intervals_) {
      fprintf(f_out, "var %4s [%d, %d] | ", li.var_id_.c_str(), li.start_, li.end_);
      if (func->var2reg.count(li.var_id_))
        fprintf(f_out, "=> %s | ", func->var2reg[li.var_id_].c_str());
      if (func->var2stack.count(li.var_id_))
        fprintf(f_out, "-> stk %d", func->var2stack[li.var_id_]);
      fprintf(f_out, "\n");
    }
    fprintf(f_out, "end %s\n\n", func->func_id_.c_str());
  }
}
