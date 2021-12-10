#ifndef E_SYMTAB_H
#define E_SYMTAB_H

#include <list>
#include <unordered_set>
#include <map>
#include "c_symtab.hpp"

class EEntryVarTable;
class EEntryFuncTable;
class ESymbolTable;
class ENode;

using EEnVTabPtr = EEntryVarTable*;
using EEnFTabPtr = EEntryFuncTable*;
typedef unordered_map<string, EEnVTabPtr> EVarTable;
typedef unordered_map<string, EEnFTabPtr> EFuncTable;
typedef list<ENode*> EStmtList;
typedef unordered_set<string> VarSet;

class EBasicBlock {
  int blk_id_;
  EStmtList stmts_;
  vector<EBasicBlock*> pred_;
  vector<EBasicBlock*> succ_;
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

class EEntryFuncTable {
public:
  string func_id_;
  int param_num_;
  EStmtList stmts_;
  EVarTable locals_;
  unordered_map<int, ENode*> label_to_node_;
  // vector<Block>

  EEntryFuncTable() = default;
  EEntryFuncTable(string eeyore_id,
                  int param_num);
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
    void add_stmt(ENode* stmt);
    EEnFTabPtr get_cur_func();
};

#endif
