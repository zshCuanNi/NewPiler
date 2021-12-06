#ifndef C_SYMTAB_H
#define C_SYMTAB_H

#include <unordered_map>
#include <string>
#include <memory>
#include <vector>

#define NEW(x) new x
#define INT_SIZE 4
#define GLOBAL_BLOCK 0

using namespace std;

enum DataType {
    datINT, datVOID
};

class CEntryVarTable;
class CEntryFuncTable;
class CSymbolTable;

using CEnVTabPtr = CEntryVarTable*;
using CEnFTabPtr = CEntryFuncTable*;
using CVarTable = unordered_map<string, CEnVTabPtr>;
using CFuncTable = unordered_map<string, CEnFTabPtr>;
using CVarList = vector<CEnVTabPtr>;

class CEntryVarTable {
public:
  string sysy_id_;
  vector<int> widths_;
  vector<int> values_;
  bool is_const_;
  bool is_param_;
  bool is_ptr_;
  bool is_arr_;
  bool is_temp_;
  string eeyore_id_;

  CEntryVarTable() = default;
  CEntryVarTable(string sysy_id,
                 vector<int> widths,
                 vector<int> values,
                 bool is_const,
                 bool is_param,
                 bool is_ptr,
                 bool is_arr,
                 bool is_temp);
};

class CEntryFuncTable {
public:
  DataType ret_type_;
  string sysy_id_;
  CVarList params_;
  CVarList locals_;
  string eeyore_id_;

  CEntryFuncTable() = default;
  CEntryFuncTable(DataType ret_type,
                  string sysy_id);
};

class CSymbolTable {
public:
  int blk_id_=-1;
  int cur_var_cnt_=0;
  int cur_param_cnt_=0;
  string cur_func_;
  vector<CVarTable> blk_stack_;
  CFuncTable func_tab_;

  CSymbolTable();
  CEnVTabPtr register_var(string sysy_id,
                          vector<int> widths=vector<int>(),
                          vector<int> values=vector<int>(),
                          bool is_const=false,
                          bool is_param=false,
                          bool is_ptr=false,
                          bool is_arr=false,
                          bool is_temp=true);
  CEnFTabPtr register_func(DataType ret_type,
                           string sysy_id);
  CEnVTabPtr find_var(string sysy_id);
  CEnFTabPtr find_func(string sysy_id);
  void new_blk();
  void delete_blk();
};

#endif