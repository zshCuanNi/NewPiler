#include "e_symtab.hpp"

EEntryVarTable::EEntryVarTable(string eeyore_id,
                               bool is_arr,
                               int width):
eeyore_id_(eeyore_id), is_arr_(is_arr), width_(width) {}

EEntryFuncTable::EEntryFuncTable(string func_id,
                                 int param_num):
func_id_(func_id), param_num_(param_num) {}

ESymbolTable::ESymbolTable() {
  // register global "function" first
  register_func("global", 0);
}

void ESymbolTable::register_var(string eeyore_id,
                                bool is_arr,
                                int width) {
  EEnVTabPtr new_var = NEW(EEntryVarTable)(eeyore_id, is_arr, width);
  if (cur_func_ == "global") {
    new_var->is_global_ = true;
    new_var->tigger_id_ = "v" + to_string(global_cnt_++);
  }

  func_tab_[cur_func_]->locals_[eeyore_id] = new_var;
}

void ESymbolTable::register_func(string func_id,
                                 int param_num) {
  cur_func_ = func_id;
  func_tab_[func_id] = NEW(EEntryFuncTable)(func_id, param_num);
  func_ids_.push_back(func_id);
}

void ESymbolTable::add_stmt(ENode* stmt) {
  func_tab_[cur_func_]->stmts_.push_back(stmt);
}

EEnFTabPtr ESymbolTable::get_cur_func() {
  return func_tab_[cur_func_];
}