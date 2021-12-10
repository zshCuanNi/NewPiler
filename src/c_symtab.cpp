#include "c_symtab.hpp"
#include <cassert>

CEntryVarTable::CEntryVarTable(string sysy_id,
                               vector<int> widths,
                               vector<int> values,
                               bool is_const,
                               bool is_param,
                               bool is_ptr,
                               bool is_arr,
                               bool is_temp):
sysy_id_(sysy_id), widths_(widths), values_(values), is_const_(is_const),
is_param_(is_param), is_ptr_(is_ptr), is_arr_(is_arr), is_temp_(is_temp) {}

CEntryFuncTable::CEntryFuncTable(DataType ret_type, string sysy_id):
ret_type_(ret_type), sysy_id_(sysy_id) {
  eeyore_id_ = "f_" + sysy_id_;
}

CSymbolTable::CSymbolTable() {
  // register functions in sysy runtime library
  const string getter_func[] = { "getint", "getch", "getarray" };
  for (const string func_name: getter_func)
    register_func(datINT, func_name);
  func_tab_["getarray"]->params_.push_back(
      NEW(CEntryVarTable)("", vector<int>({0}), vector<int>(),
                          false, true, true, true, false)
  );

  const string putter_func[] = { "putint", "putch", "putarray" };
  for (const string func_name: putter_func) {
    register_func(datVOID, func_name);
    func_tab_[func_name]->params_.push_back(
        NEW(CEntryVarTable)("", vector<int>(), vector<int>(),
                            false, true, false, false, false)
    );
  }
  func_tab_["putarray"]->params_.push_back(
      NEW(CEntryVarTable)("", vector<int>({0}), vector<int>(),
                          false, true, true, true, false)
  );

  const string time_func[] = { "_sysy_starttime", "_sysy_stoptime" };
  for (const string func_name: time_func) {
    register_func(datVOID, func_name);
    func_tab_[func_name]->params_.push_back(
        NEW(CEntryVarTable)("", vector<int>(), vector<int>(),
                            false, true, false, false, false)
    );
  }
}

CEnVTabPtr CSymbolTable::register_var(string sysy_id,
                                      vector<int> widths,
                                      vector<int> values,
                                      bool is_const,
                                      bool is_param,
                                      bool is_ptr,
                                      bool is_arr,
                                      bool is_temp) {
  CEnVTabPtr new_var = NEW(CEntryVarTable)(
      sysy_id, widths, values, is_const, is_param,
      is_ptr, is_arr, is_temp);

  if (is_param) {
  // parameter of a function
    new_var->eeyore_id_ = "p" + to_string(cur_param_cnt_++);
    assert(blk_id_ != GLOBAL_BLOCK);
    func_tab_[cur_func_]->params_.push_back(new_var);
  } else if (is_temp)
  // temp variable, sysy_id has prefix "#t"
    new_var->eeyore_id_ = sysy_id.substr(1);
  else
    new_var->eeyore_id_ = "T" + to_string(cur_var_cnt_++);
  
  // register new_var as a local variable if it is in a function
  if (blk_id_ != GLOBAL_BLOCK)
    func_tab_[cur_func_]->locals_.push_back(new_var);
  // register new_var in this block
  blk_stack_[blk_id_][sysy_id] = new_var;
  return new_var;
}

CEnFTabPtr CSymbolTable::register_func(DataType ret_type,
                                       string sysy_id) {
  cur_func_ = sysy_id;
  func_tab_[sysy_id] = NEW(CEntryFuncTable)(ret_type, sysy_id);
  return func_tab_[sysy_id];
}

CEnVTabPtr CSymbolTable::find_var(string sysy_id) {
  for (int blk = blk_id_; blk >= 0; --blk)
    // if found its definition in a certain block...
    if (blk_stack_[blk].find(sysy_id) != blk_stack_[blk].end())
      return blk_stack_[blk][sysy_id];
  assert(false);
  return nullptr;
}
CEnFTabPtr CSymbolTable::find_func(string sysy_id) {
  // if found in function table...
  if (func_tab_.find(sysy_id) != func_tab_.end())
    return func_tab_[sysy_id];
  assert(false);
  return nullptr;
}

void CSymbolTable::new_blk() {
  ++blk_id_;
  blk_stack_.push_back(CVarTable());
}

void CSymbolTable::delete_blk() {
  --blk_id_;
  blk_stack_.pop_back();
  // getting into global scope means getting out of a function
  if (blk_id_ == GLOBAL_BLOCK) {
    cur_param_cnt_ = 0;
    cur_func_.clear();
  }
}
