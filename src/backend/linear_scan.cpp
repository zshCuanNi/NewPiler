#include "newpiler.hpp"

/* Calcaulate live intervals for locals and params, not including globals.
 * live interval [start, end]
 * start = -1 means live at the entry of funcion.
 * end = -2 means live till the exit of function (but not used by return
 * stmt), will be manually changed to the stmtno of the last stmt.
 * If start = -2, the var is never used (usually a constant, which is
 * replaced by its value) TODO: clear dead vars
 */
void Newpiler::cal_live_intervals() {
  for (auto func_id: e_sym_tab->func_ids_) {
    if (func_id == "global") continue;
    auto func = e_sym_tab->find_func(func_id);
    // non-array local vars
    // local arrays will get its head addr through loadaddr
    // TODO: use an allocated reg to keep the arr head addr, and a tmp one
    // to keep calculated result of arr element addr used by current stmt
    for (auto id_var_pair: func->locals_) {
      // if (id_var_pair.second->is_arr_) continue;
      auto var = id_var_pair.first;
      int start = -2, end = -2;
      for (auto stmt: func->stmts_) {
        if (start == -2 && stmt->live_out_.count(var))
          start = stmt->stmtno_;
        if (stmt->live_in_.count(var))
          end = stmt->stmtno_;
      }
      func->insert_live_interval(LiveInterval(var, start, end));
    }
    // params, always start from -1
    for (int i = 0; i < func->param_num_; i++) {
      string var = format("p%d", i);
      int start = -1, end = -2;
      for (auto stmt: func->stmts_) {
        if (stmt->live_in_.count(var))
          end = stmt->stmtno_;
      }
      func->insert_live_interval(LiveInterval(var, start, end));
    }
    // manually change end == -2 to end == last_stmt->stmtno_
    for (auto li: func->live_intervals_)
      if (li.end_ == -2) {
        auto new_li = li;
        new_li.end_ = (*func->stmts_.rbegin())->stmtno_;
        func->live_intervals_.erase(li);
        func->live_intervals_.insert(new_li);
      }
  }
}

struct CmpEnd {
  bool operator()(const LiveInterval& a, const LiveInterval& b) const {
    if (a.end_ != b.end_) return a.end_ < b.end_;
    return a.var_id_ < b.var_id_;
  }
};

/* By default, is_prefer_callee is set to true.
 * For functions that have few call stmts, caller saved regs is
 * preferred; for other functions, callee saved regs is preferred.
 * Default behavior:
 * First get callee saved regs, then caller saved regs, last param regs.
 * Every time select the max-id reg to increase its re-use time.
 * a7 is preferred than a0 because the latter is more dangerous when
 * dealing with return value of a function and in param passing process.
 */
string get_free_reg(std::set<string>& free_regs, bool is_prefer_callee = true) {
  int callee_no = -1, caller_no = -1, param_no = -1;
  for (auto free_reg: free_regs) {
    switch (free_reg[0]) {
      case 's':
        callee_no = std::max(callee_no, stoi(free_reg.substr(1, 2))); break;
      case 't':
        caller_no = std::max(caller_no, stoi(free_reg.substr(1, 2))); break;
      case 'a':
        param_no = std::max(param_no, stoi(free_reg.substr(1, 2))); break;
      default: assert(false);
    }
  }
  string ret_reg;
  if (is_prefer_callee) {
    if (callee_no != -1) ret_reg = format("s%d", callee_no);
    else if (caller_no != -1) ret_reg = format("t%d", caller_no);
    else if (param_no != -1) ret_reg = format("a%d", param_no);
    else assert(false);
  } else {
    if (caller_no != -1) ret_reg = format("t%d", caller_no);
    else if (param_no != -1) ret_reg = format("a%d", param_no);
    else if (callee_no != -1) ret_reg = format("s%d", callee_no);
    else assert(false);
  }
  return ret_reg;
}

void alloc_stack_pos(EVarTable& vars,
                     VarStackMap& var2stack,
                     string var_id,
                     int& stk_sz) {
  if (is_param(var_id))
    var2stack[var_id] = stk_sz++;
  else {
    assert(vars.count(var_id));
    if (vars[var_id]->is_arr_) {
      var2stack[var_id] = stk_sz;
      stk_sz += vars[var_id]->width_;
    } else
      var2stack[var_id] = stk_sz++;
  }
}

/* scan the whole function linearlly and allocate registers
 * implementation details refer to the paper:
 * Linear Scan Register Allocation, by MASSIMILIANO POLETTO & VIVEK SARKAR
 */
void Newpiler::linear_scan() {
  for (auto func_id: e_sym_tab->func_ids_) {
    if (func_id == "global") continue;
    auto func = e_sym_tab->find_func(func_id);

    bool has_call = false; // mark whether this func has call stmts
    for (auto stmt: func->stmts_)
      if (stmt->line_type_ == lCALL || (stmt->line_type_ == lASN
          && ((EAsnPtr)stmt)->rhs_->expr_type_ == eeCALL)) {
        has_call = true; break;
      }
    
    std::set<string> free_regs;
    for (auto reg: callee_saved_regs) free_regs.insert(reg);
    for (auto reg: caller_saved_regs) free_regs.insert(reg);
    for (auto reg: param_regs) free_regs.insert(reg);
    int R = free_regs.size();

    auto& var2reg = func->var2reg;
    auto& var2stack = func->var2stack;
    auto& reg2stack = func->reg2stack;
    auto& used_regs = func->used_regs;
    int stk_sz = 0;
    // linear scan
    std::set<LiveInterval, CmpEnd> active;
    for (auto li_i: func->live_intervals_) {
      // expire old intervals ending before li starts
      std::set<LiveInterval, CmpEnd> to_expire;
      for (auto li_j: active) {
        if (li_j.end_ > li_i.start_)
          break;
        to_expire.insert(li_j);
        free_regs.insert(var2reg[li_j.var_id_]);
      }
      for (auto li_j: to_expire)
        active.erase(li_j);
      if ((int)active.size() == R) {
      // spill at interval li_i
        auto spill = *active.rbegin();
        // heuristically spill the interval that ends last
        if (spill.end_ > li_i.end_) {
          var2reg[li_i.var_id_] = var2reg[spill.var_id_];
          var2reg.erase(spill.var_id_);
          active.erase(spill);
          active.insert(li_i);
          // allocate stack space for spill
          alloc_stack_pos(func->locals_, var2stack, spill.var_id_, stk_sz);
        } else
        // allocate stack space for li_i
          alloc_stack_pos(func->locals_, var2stack, li_i.var_id_, stk_sz);
      } else {
        string free_reg = get_free_reg(free_regs, has_call);
        var2reg[li_i.var_id_] = free_reg;
        free_regs.erase(free_reg);
        used_regs.insert(free_reg);
        active.insert(li_i);
      }
    }

    // allocate stack space for allocated arrays and allocated params
    for (auto& var_reg_pair: var2reg) {
      string var_id = var_reg_pair.first;
      if (is_param(var_id)) {
        alloc_stack_pos(func->locals_, var2stack, var_id, stk_sz);
      } else {
        auto var = func->locals_[var_id];
        if (!var->is_arr_) continue;
        alloc_stack_pos(func->locals_, var2stack, var_id, stk_sz);
      }
    }
    // now each local array / param owns a banch of stack space

    // allocate stack space for used regs
    // (needed when we want to push callee/caller saved regs to the stack)
    for (auto reg: callee_saved_regs)
      if (used_regs.count(reg))
        reg2stack[reg] = stk_sz++;
    for (auto reg: caller_saved_regs)
      if (used_regs.count(reg))
        reg2stack[reg] = stk_sz++;
    for (auto reg: param_regs)
      if (used_regs.count(reg))
        reg2stack[reg] = stk_sz++;
    
    func->stack_size_ = stk_sz;
  }
}
