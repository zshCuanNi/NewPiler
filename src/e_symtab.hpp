#ifndef E_SYMTAB_H
#define E_SYMTAB_H

#include <list>
#include <unordered_set>
#include <set>
#include <map>
#include "c_symtab.hpp"

class EBasicBlock;
class EEntryVarTable;
class EEntryFuncTable;
class ESymbolTable;
class ENode;

using EBBlkPtr = EBasicBlock*;
using EEnVTabPtr = EEntryVarTable*;
using EEnFTabPtr = EEntryFuncTable*;
typedef unordered_map<string, EEnVTabPtr> EVarTable;
typedef unordered_map<string, EEnFTabPtr> EFuncTable;
typedef vector<ENode*> EStmtList;
typedef unordered_set<string> VarSet;
typedef vector<EBBlkPtr> BlockList;
typedef unordered_map<string,string> VarRegMap;
typedef unordered_map<string, int> VarStackMap;
using RegVarMap = VarRegMap;
using RegStackMap = VarStackMap;

class EBasicBlock {
public:
  bool is_vis_ = false;
  int blk_id_;
  EStmtList stmts_;
  BlockList preds_;
  BlockList succs_;
  VarSet live_in_, live_out_;

  EBasicBlock(int blk_id);
  void append_stmt(ENode* stmt);
  void add_pred(EBBlkPtr pred);
  void add_succ(EBBlkPtr succ);
  ENode* get_entry_stmt();
  ENode* get_exit_stmt();
};

class LiveInterval {
public:
  string var_id_;
  int start_, end_; // see cal_live_intervals for accurate definition

  LiveInterval(string var_id, int start, int end);
};

class EEntryVarTable {
public:
  string eeyore_id_;
  bool is_arr_;
  int width_;
  bool is_global_;
  string tigger_id_ = ""; // if need ???
  
  EEntryVarTable() = default;
  EEntryVarTable(string eeyore_id,
                 bool is_arr,
                 int width);
};

struct CmpStart {
  bool operator()(const LiveInterval& a, const LiveInterval& b) const {
    if (a.start_ != b.start_) return a.start_ < b.start_;
    return a.var_id_ < b.var_id_;
  }
};

class EEntryFuncTable {
public:
  string func_id_;
  int param_num_;
  EStmtList stmts_;
  EVarTable locals_;
  unordered_map<int, ENode*> label_to_node_;
  BlockList blocks_;
  set<LiveInterval, CmpStart> live_intervals_;
  VarRegMap var2reg;
  VarStackMap var2stack;
  RegStackMap reg2stack;
  set<string> used_regs;
  int stack_size_;

  EEntryFuncTable() = default;
  EEntryFuncTable(string eeyore_id,
                  int param_num);
  void append_block(EBBlkPtr basic_block);
  void insert_live_interval(LiveInterval l);
};

class ESymbolTable {
public:
    int global_cnt_ = 0;
    string cur_func_;
    EFuncTable func_tab_;
    vector<string> func_ids_;

    ESymbolTable();
    void register_var(string eeyore_id,
                      bool is_arr = false,
                      int width = 0);
    void register_func(string func_id,
                       int param_num);
    EEnFTabPtr find_func(string func_id);
    EEnVTabPtr find_var(string var_id, string func_id);
    void add_stmt(ENode* stmt);
    EEnFTabPtr get_cur_func();
};

#endif
