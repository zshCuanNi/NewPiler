#include <cassert>
#include "newpiler.hpp"

/* provided by iFreilicht
 * https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
 */
template<typename ... Args>
string format( const string& format, Args ... args )
{
    int size_s = snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    auto size = static_cast<size_t>( size_s );
    auto buf = std::make_unique<char[]>( size );
    snprintf( buf.get(), size, format.c_str(), args ... );
    return string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

void Newpiler::compile_eeyore() {
  // global variables
  func = e_sym_tab->find_func("global");
  for (auto& id_var_pair: func->locals_) {
    auto var = id_var_pair.second;
    if (var->is_arr_)
      tigger_codes_.push_back(
        format("%s = malloc %d",
               var->tigger_id_.c_str(),
               var->width_)
      );
    else {
    // non-array var may have a non-zero initializer, find it in stmts
      for (auto stmt: func->stmts_) {
        assert(stmt->line_type_ == lASN);
        auto cast_stmt = (EAsnPtr)stmt;
        assert(cast_stmt->lhs_->expr_type_ == eeSB);
        assert(cast_stmt->rhs_->expr_type_ == eeNUM);
        if (((ESymbol*)(cast_stmt->lhs_))->id_ == var->eeyore_id_) {
          tigger_codes_.push_back(
            format("%s = %d",
                   var->tigger_id_.c_str(),
                   ((ENumber*)cast_stmt->rhs_)->val_)
          );
          break;
        }
      }
    }
  }

  // functions
  for (auto func_id: e_sym_tab->func_ids_) {
    if (func_id == "global") continue;
    func = e_sym_tab->find_func(func_id);
    var2reg = func->var2reg;
    var2stack = func->var2stack;
    reg2stack = func->reg2stack;
    used_regs = func->used_regs;
    
    // function header
    tigger_codes_.push_back(
      format("%s [%d] [%d]",
             func_id.c_str(),
             func->param_num_,
             func->stack_size_)
    );
    
    // save callee saved regs, excluding s0, s1, s2
    // because these three regs will never be used to store var value
    save_regs(callee_saved_regs);

    for (int i = 0; i < func->param_num_; i++) {
      // move params from param regs to proposed regs
      string param_id = format("p%d", i);
      if (var2reg.count(param_id)) {
        string proposed_reg = var2reg[param_id];
        if (proposed_reg == param_regs[i]) continue;
        if (!start_with(proposed_reg, "a") || stoi(proposed_reg.substr(1, 1)) < i) {
        // a1 = a2 is allowed, because when moving param reg a2, param reg
        // a1 has passed its param value to corresponding reg
          tigger_codes_.push_back(
            format("\t%s = %s",
                  proposed_reg.c_str(),
                  param_regs[i].c_str())
          );
        } else {
        // should avoid a2 = a1, store param value to stack first
          tigger_codes_.push_back(
            format("\tstore %s %d", param_regs[i].c_str(), var2stack[param_id])
          );
        }
      }
    }
    // reload param value from stack to proposed reg
    for (int i = 0; i < func->param_num_; i++) {
      string param_id = format("p%d", i);
      if (var2reg.count(param_id)) {
        string proposed_reg = var2reg[param_id];
        if (start_with(proposed_reg, "a") && stoi(proposed_reg.substr(1, 1)) > i) {
          tigger_codes_.push_back(
            format("\tload %d %s", var2stack[param_id], proposed_reg.c_str())
          );
        }
      }
    }
    // Currently live_in of every array starts from -1, which means I can
    // load arr head addr to corresponding reg at the very beginning.
    // We need to explicit generate codes for assigning arr head addr to 
    // allocated regs, because in all stmts the arr is always 'used' but
    // never 'defined'.
    for (auto var_reg_pair: var2reg) {
      string var_id = var_reg_pair.first;
      if (is_param(var_id)) continue;
      auto var = func->locals_[var_id];
      if (!var->is_arr_) continue;
      tigger_codes_.push_back(
        format("\tloadaddr %d %s",
               var2stack[var_id],
               var2reg[var_id].c_str())
      );
    }

    // start to translate eeyore to tigger stmt by stmt
    for (auto stmt: func->stmts_) compile_eeyore_stmt(stmt);
    
    // function tail
    tigger_codes_.push_back(format("end %s\n", func_id.c_str()));
  }
}

void Newpiler::compile_eeyore_stmt(ENodePtr stmt) {
  if (!stmt->labels_.empty()) {
    // redundant labels shall be cleared
    assert(stmt->labels_.size() == 1);
    tigger_codes_.push_back(format("l%d:", stmt->labels_[0]));
  }

  // a temporary patch
  // If a var is def by the stmt but never used after,
  // liveness analysis regard it non-live at this point,
  // hence may allocate its register to another var.
  // In this case, the useless def stmt should not be translated,
  // otherwise the register will be covered by a wrong value.
  // TODO: do dead code elimination when do dataflow analysis
  // ??? adding it makes more case unpassed ...
  // if (stmt->line_type_ == lASN) {
  //   auto cast_stmt = (EAsnPtr)stmt;
  //   if (cast_stmt->lhs_->expr_type_ == eeSB) {
  //     auto cast_lhs = (ESymbol*)cast_stmt->lhs_;
  //     if (func->locals_.count(cast_lhs->id_)) {
  //       LiveInterval li_i = LiveInterval("", 0, 0);
  //       for (auto& li_j: func->live_intervals_) {
  //         if (li_j.var_id_ == cast_lhs->id_) {
  //           li_i = li_j; break;
  //         }
  //       }
  //       if (li_i.end_ < cast_stmt->stmtno_) {
  //         if (cast_stmt->rhs_->expr_type_ == eeCALL)
  //           stmt = cast_stmt->rhs_;
  //         else return;
  //       }
  //     }
  //   }
  // }
  switch (stmt->line_type_) {
    case lASN: {
      auto cast_stmt = (EAsnPtr)stmt;
      auto lhs = cast_stmt->lhs_;
      auto rhs = cast_stmt->rhs_;
      if (lhs->expr_type_ == eeARR) {
      // SYMBOL[RVal] = RVal
        string reg_ll = get_reg(lhs->lhs_, "s1");
        if (lhs->rhs_->expr_type_ == eeNUM) {
        // SB[NUM] = RVal ==>
        // loadaddr SB s1, load RVal s2, RVal s1[NUM] = s2
          string reg_r = get_reg(rhs, "s2");
          tigger_codes_.push_back(
            format("\t%s[%d] = %s",
                   reg_ll.c_str(),
                   ((ENumber*)lhs->rhs_)->val_,
                   reg_r.c_str())
          );
        } else {
        // SB1[SB2] = RVal ==>
        // loadaddr SB1 s1, load SB2 s2, s1 = s1 + s2, load RVal s2, s1[0] = s2
          string reg_lr = get_reg(lhs->rhs_, "s2");
          tigger_codes_.push_back(
            format("\ts1 = %s + %s", reg_ll.c_str(), reg_lr.c_str())
          );
          string reg_r = get_reg(rhs, "s2");
          tigger_codes_.push_back(
            format("\ts1[0] = %s", reg_r.c_str())
          );
        }
      } else {
      // SYMBOL = RVal | SYMBOL[RVal] | UnOp RVal | RVal BinOp RVal | call FUNC
        string lhs_id = ((ESymbol*)lhs)->id_;
        switch (rhs->expr_type_) {
          case eeNUM: {
          // SYMBOL = NUM ==> load SYMBOL s1, s1 = NUM
            string reg = get_reg(lhs);
            tigger_codes_.push_back(
              format("\t%s = %d", reg.c_str(), ((ENumber*)rhs)->val_)
            );
            if (var2reg.count(lhs_id) == 0)
              store_into_stack(reg, lhs_id);
            break;
          }
          case eeSB: {
          // SB1 = SB2 ==> load SB1 s1, load SB2 s2, s1 = s2
            string reg_l = get_reg(lhs, "s1");
            string reg_r = get_reg(rhs, "s2");
            tigger_codes_.push_back(
              format("\t%s = %s", reg_l.c_str(), reg_r.c_str())
            );
            if (var2reg.count(lhs_id) == 0)
              store_into_stack(reg_l, lhs_id);
            break;
          }
          case eeARR: {
          // SYMBOL = SYMBOL[RVal]
            string reg_l = get_reg(lhs, "s1");
            string reg_rl = get_reg(rhs->lhs_, "s2");
            if (rhs->rhs_->expr_type_ == eeNUM) {
            // SB1 = SB2[NUM] ==>
            // loadaddr SB2 s2, load SB1 s1, s1 = s2[NUM]
              tigger_codes_.push_back(
                format("\t%s = %s[%d]",
                       reg_l.c_str(),
                       reg_rl.c_str(),
                       ((ENumber*)rhs->rhs_)->val_)
              );
            } else {
            // SB1 = SB2[SB3] ==>
            // loadaddr SB2 s2, load SB3 s1, s2 = s2 + s1, load SB1 s1, s1 = s2[0]
              string reg_rr = get_reg(rhs->rhs_, "s1");
              tigger_codes_.push_back(
                format("\ts2 = %s + %s", reg_rl.c_str(), reg_rr.c_str())
              );
              tigger_codes_.push_back(
                format("\t%s = s2[0]", reg_l.c_str())
              );
            }
            if (var2reg.count(lhs_id) == 0)
              store_into_stack(reg_l, lhs_id);
            break;
          }
          case eeOP: {
            string reg_l = get_reg(lhs, "s1");
            if (rhs->rhs_) {
            // SYMBOL = RVal BinOp RVal
            // TODO: strength reduction
              if (rhs->rhs_->expr_type_ == eeNUM) {
              // SYMBOL = RVal BinOp NUM ==> s1 = s2 BinOp NUM
                string reg_rl = get_reg(rhs->lhs_, "s2");
                tigger_codes_.push_back(
                  format("\t%s = %s %s %d",
                         reg_l.c_str(),
                         reg_rl.c_str(),
                         rhs->op_.c_str(),
                         ((ENumber*)rhs->rhs_)->val_)
                );
              } else {
              // SB1 = SB2 BinOp SB3 ==> s1 = s2 BinOp s1
                string reg_rl = get_reg(rhs->lhs_, "s2");
                string reg_rr = get_reg(rhs->rhs_, "s1");
                tigger_codes_.push_back(
                  format("\t%s = %s %s %s",
                         reg_l.c_str(),
                         reg_rl.c_str(),
                         rhs->op_.c_str(),
                         reg_rr.c_str())
                );
              }
            } else {
            // SYMBOL = UnOp RVal
              if (rhs->lhs_->expr_type_ == eeNUM) {
              // SYMBOL = UnOp NUM ==> s1 = NUM' (= UnOp NUM)
                int val = ((ENumber*)rhs->lhs_)->val_;
                if (rhs->op_ == "-") val = -val;
                else if (rhs->op_ == "!") val = !val;
                tigger_codes_.push_back(
                  format("\t%s = %d", reg_l.c_str(), val)
                );
              } else {
              // SB1 = UnOp SB2 => s1 = UnOp s2
                string reg_r = get_reg(rhs->lhs_, "s2");
                tigger_codes_.push_back(
                  format("%s = %s %s",
                         reg_l.c_str(),
                         rhs->op_.c_str(),
                         reg_r.c_str())
                );
              }
            }
            if (var2reg.count(lhs_id) == 0)
              store_into_stack(reg_l, lhs_id);
            break;
          }
          case eeCALL: {
          // param RVal, SB = call FUNC ==>
          // load RVal a0, s1 = call FUNC
            auto cast_rhs = (ECallPtr)rhs;
            // save param regs before set params
            save_regs(param_regs);
            // save caller saved regs before call
            save_regs(caller_saved_regs);
            // move params into param regs
            for (int i = 0; i < (int)cast_rhs->params_.size(); i++) {
              auto param = cast_rhs->params_[i];
              if (param->expr_type_ == eeNUM)
                tigger_codes_.push_back(
                  format("\t%s = %d",
                        param_regs[i].c_str(),
                        ((ENumber*)param)->val_)
                );
              else {
                string var_id = ((ESymbol*)param)->id_;
                if (var2reg.count(var_id) && start_with(var2reg[var_id], "a")) {
                // the reg allocated to this param is a param reg
                // directly load the value from stack
                  tigger_codes_.push_back(
                    format("\tload %d %s",
                          reg2stack[var2reg[var_id]],
                          param_regs[i].c_str())
                  );
                } else {
                  tigger_codes_.push_back(
                    format("\t%s = %s",
                          param_regs[i].c_str(),
                          get_reg(var_id).c_str())
                  );
                }
              }
            }
            // call the function
            tigger_codes_.push_back(
              format("\tcall %s", cast_rhs->func_id_.c_str())
            );
            // restore caller saved regs
            restore_regs(caller_saved_regs);
            // assign return value to s1 if a0 is used
            bool is_a0_used = (int)used_regs.count("a0");
            if (is_a0_used) {
              tigger_codes_.push_back("\ts1 = a0");
            }
            // restore param regs
            restore_regs(param_regs);
            // assign return value to the variable
            string ret_val = is_a0_used? "s1": "a0";
            if (var2reg.count(lhs_id)){
              tigger_codes_.push_back(
                format("\t%s = %s", var2reg[lhs_id].c_str(), ret_val.c_str())
              );
            } else
              store_into_stack(ret_val, lhs_id);
            
            break;
          }
          default: assert(false);
        }
      }
      break;
    }
    case lGOTO: {
      auto cast_stmt = (EGotoPtr)stmt;
      tigger_codes_.push_back(format("\tgoto l%d", cast_stmt->where_));
      break;
    }
    case lIF: {
      // currently only 'if RVal op 0 goto ld' will be generated
      // TODO: do optimization for something like this below:
      // t1 = t0 <= NUM
      // if t1 == 0 goto l1
      // ------>
      // if t0 > NUM goto l1
      // when translating it to tigger, NUM should be kept by a temp reg
      // [NOT essential]
      auto cast_stmt = (EIfPtr)stmt;
      auto cond = cast_stmt->cond_;
      // if NUM op 0 goto ld
      // TODO: replace this stmt with a goto (or fall through)
      // [NOT essential]
      string reg = get_reg(cond->lhs_);
      tigger_codes_.push_back(
        format("\tif %s %s x0 goto l%d",
               reg.c_str(),
               cond->op_.c_str(),
               cast_stmt->where_)
      );
      break;
    }
    case lCALL: {
      auto cast_stmt = (ECallPtr)stmt;
      // save param regs before set params
      save_regs(param_regs);
      // save caller saved regs before call
      save_regs(caller_saved_regs);
      // move params into param regs
      // TODO: for un-allocated vars, directly load the value from stack
      // should distinguish array or not
      for (int i = 0; i < (int)cast_stmt->params_.size(); i++) {
        auto param = cast_stmt->params_[i];
        if (param->expr_type_ == eeNUM)
          tigger_codes_.push_back(
            format("\t%s = %d",
                   param_regs[i].c_str(),
                   ((ENumber*)param)->val_)
          );
        else {
          string var_id = ((ESymbol*)param)->id_;
          if (var2reg.count(var_id) && start_with(var2reg[var_id], "a")) {
          // the reg allocated to this param is a param reg
          // directly load the value from stack
            tigger_codes_.push_back(
              format("\tload %d %s",
                     reg2stack[var2reg[var_id]],
                     param_regs[i].c_str())
            );
          } else {
            tigger_codes_.push_back(
              format("\t%s = %s",
                     param_regs[i].c_str(),
                     get_reg(var_id).c_str())
            );
          }
        }
      }
      // call the function
      tigger_codes_.push_back(
        format("\tcall %s", cast_stmt->func_id_.c_str())
      );
      // restore caller saved regs
      restore_regs(caller_saved_regs);
      restore_regs(param_regs);
      break;
    }
    case lRET: {
      auto cast_stmt = (ERetPtr)stmt;
      if (cast_stmt->ret_val_) {
        auto ret_val = cast_stmt->ret_val_;
        if (ret_val->expr_type_ == eeNUM) {
          tigger_codes_.push_back(
            format("\ta0 = %d", ((ENumber*)ret_val)->val_)
          );
        } else {
          tigger_codes_.push_back(
            format("\ta0 = %s", get_reg(((ESymbol*)ret_val)->id_).c_str())
          );
        }
      }
      // restore callee saved regs
      restore_regs(callee_saved_regs);
      tigger_codes_.push_back("\treturn");
      break;
    }
    default: assert(false); // lEXPR should not be appeared here
  }
}

bool Newpiler::is_global(string var_id) {
  return e_sym_tab->func_tab_["global"]->locals_.count(var_id);
}

bool Newpiler::is_arr(string var_id) {
  if (is_global(var_id))
    return e_sym_tab->func_tab_["global"]->locals_[var_id]->is_arr_;
  else
    return func->locals_[var_id]->is_arr_;
}

void Newpiler::store_into_stack(string reg, string var_id) {
  assert(var2reg.count(var_id) == 0);
  if (is_global(var_id)) {
    tigger_codes_.push_back(
      format("\tloadaddr %s s2",
             e_sym_tab->func_tab_["global"]->locals_[var_id]->tigger_id_.c_str())
    );
    tigger_codes_.push_back("\ts2[0] = " + reg);
  } else {
    tigger_codes_.push_back(
      format("\tstore %s %d", reg.c_str(), var2stack[var_id])
    );
  }
}

string Newpiler::get_reg(string var_id, string specify_reg) {
  // Regs which are allocated to arrays have been assigned the arr head
  // addr in advance.
  if (var2reg.count(var_id)) return var2reg[var_id];
  string tmp_reg = specify_reg == ""? "s1": specify_reg;
  int is_g = (int)is_global(var_id);
  int is_a = (int)is_arr(var_id);
  switch ((is_g << 1) + is_a) {
    case 0: { // local non-array
      tigger_codes_.push_back(
        format("\tload %d %s",
                var2stack[var_id],
                tmp_reg.c_str())
      );
      break;
    }
    case 1: { // local array 
      tigger_codes_.push_back(
        format("\tloadaddr %d %s",
                var2stack[var_id],
                tmp_reg.c_str())
      );
      break;
    }
    case 2: { // global non-array
      tigger_codes_.push_back(
        format("\tload %s %s",
                e_sym_tab->func_tab_["global"]->locals_[var_id]->tigger_id_.c_str(),
                tmp_reg.c_str())
      );
      break;
    }
    case 3: { // global array
      tigger_codes_.push_back(
        format("\tloadaddr %s %s",
                e_sym_tab->func_tab_["global"]->locals_[var_id]->tigger_id_.c_str(),
                tmp_reg.c_str())
      );
      break;
    }
    default : assert(false);
  }
  return tmp_reg;
}

string Newpiler::get_reg(int number, string specify_reg) {
  if (number) {
    string tmp_reg = specify_reg == ""? "s1": specify_reg;
    tigger_codes_.push_back(
      format("\t%s = %d", tmp_reg.c_str(), number)
    );
    return tmp_reg;
  } else return "x0";
}

string Newpiler::get_reg(EExprPtr rval, string specify_reg) {
  assert(rval->expr_type_ == eeNUM || rval->expr_type_ == eeSB);
  if (rval->expr_type_ == eeNUM)
    return get_reg(((ENumber*)rval)->val_, specify_reg);
  else
    return get_reg(((ESymbol*)rval)->id_, specify_reg);
}

void Newpiler::save_regs(vector<string> regs) {
  for (auto reg: regs) {
    if (used_regs.count(reg) == 0) continue;
    tigger_codes_.push_back(
      format("\tstore %s %d", reg.c_str(), reg2stack[reg])
    );
  }
}

void Newpiler::restore_regs(vector<string> regs) {
  for (auto reg: regs) {
    if (used_regs.count(reg) == 0) continue;
    tigger_codes_.push_back(
      format("\tload %d %s", reg2stack[reg], reg.c_str())
    );
  }
}
