#include <cassert>
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

void EEntryFuncTable::append_block(EBBlkPtr basic_block) {
  blocks_.push_back(basic_block);
}

void EEntryFuncTable::insert_live_interval(LiveInterval l) {
  live_intervals_.insert(l);
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

EEnFTabPtr ESymbolTable::find_func(string func_id) {
  return func_tab_[func_id];
}

EEnVTabPtr ESymbolTable::find_var(string var_id, string func_id) {
  assert(func_tab_.count(func_id));
  if (func_tab_[func_id]->locals_.count(var_id))
    return func_tab_[func_id]->locals_[var_id];
  else {
    assert(func_tab_["global"]->locals_.count(var_id));
    return func_tab_["global"]->locals_[var_id];
  }
}

EBasicBlock::EBasicBlock(int blk_id)
: blk_id_(blk_id) {}

void EBasicBlock::append_stmt(ENode* stmt) { stmts_.push_back(stmt); }

void EBasicBlock::add_pred(EBBlkPtr pred) { preds_.push_back(pred); }

void EBasicBlock::add_succ(EBBlkPtr succ) { succs_.push_back(succ); }

ENode* EBasicBlock::get_entry_stmt() { return *stmts_.begin(); }

ENode* EBasicBlock::get_exit_stmt() { return *stmts_.rbegin(); }

LiveInterval::LiveInterval(string var_id, int start, int end)
: var_id_(var_id), start_(start), end_(end) {}
