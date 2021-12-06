#ifndef C_AST_H
#define C_AST_H

#include <list>
#include "sysysymtab.hpp"

#define D_CAST(x) dynamic_cast<x>

enum OpType {
  opADD, opSUB, opMUL, opDIV, opMOD,
  opGE, opLE, opGEQ, opLEQ, opEQ, opNEQ,
  opPOS, opNEG, opNOT, opLAND, opLOR,
  opLOAD, opNONE
};

enum ExprType {
  eNUM, eVAR, eARR, eINIT, eARITH, eCOND, eCALL, ePTR
};

class CNode;
class CDeclList;
class CVarDecl;
class CFuncDef;
class CBlock;

class CStmt;
class CAssignStmt;
class CBlockStmt;
class CExprStmt;
class CIfElseStmt;
class CWhileStmt;
class CBreakStmt;
class CContStmt;
class CRetStmt;

class CExpr;
class CLValExpr;
class CArithExpr;
class CCallExpr;
class CCondExpr;

using CNodePtr = CNode*;
using CVarDPtr = CVarDecl*;
using CFuncDPtr = CFuncDef*;
using CBlkPtr = CBlock*;
using CExprPtr = CExpr*;
using CStmtPtr = CStmt*;
using CCondPtr = CCondExpr*;
using CNodePtrList = vector<CNodePtr>;
using CVarDPtrList = vector<CVarDPtr>;
using CExprPtrList = vector<CExprPtr>;
using CodeList = list<string>;

class CNode {
public:
  CNodePtr next_=nullptr;
  CNodePtr child_=nullptr;
  bool is_ret_stmt_=false;
  CodeList codes_;

  void set_next(CNodePtr next);
   void set_child(CNodePtr child);
  virtual void code_gen();
};

class CDeclList: public CNode {
public:
  bool is_const_;
  CVarDPtrList decl_list_;
  
  CDeclList(bool is_const);
  void add_decl(CVarDPtr decl);
  void code_gen();
};

class CVarDecl: public CNode {
public:
  string id_;
  CExprPtr first_width_;
  CExprPtr first_value_;
  /* for int var = val, val
   * for int arr[][] = {v0, v1, {v2, v3, v4}},
   * tree 
   *  init
   *   /
   *  /
   * v0-->v1-->init
   *            /
   *           /
   *          v2-->v3-->v4
   */
  bool is_const_;
  bool is_param_;
  bool is_ptr_;
  bool is_arr_;
  int size_;
  vector<int> widths_;
  CExprPtrList values_;
  
  CVarDecl(string id,
           CExprPtr first_width,
           CExprPtr first_value=nullptr,
           bool is_const=false,
           bool is_param=false,
           bool is_ptr=false);
  void set_widths();
  CExprPtrList set_values(vector<int> widths,
                          CExprPtr first_value,
                          int filled=0);
  void gen_local_var_init(CEnVTabPtr entry);
  void code_gen();
};

class CFuncDef: public CNode {
public:
  DataType ret_type_;
  string id_;
  CBlkPtr func_body_;
  CVarDPtrList params_;

  CFuncDef(DataType ret_type,
           string id,
           CBlkPtr func_body,
           CVarDPtrList params=CVarDPtrList());
  void code_gen();
};

class CBlock: public CNode {
public:
  CNodePtrList item_list_;
  bool is_func_body_=false;

  void set_func_body();
  void add_item(CNodePtr blk_item);
  void code_gen();
};

class CStmt: public CNode {};

class CAssignStmt: public CStmt {
public:
  CExprPtr lhs_var_;
  CExprPtr rhs_expr_;

  CAssignStmt(CExprPtr lhs_var, CExprPtr rhs_expr_);
  void code_gen();
};

class CBlockStmt: public CStmt {
public:
  CBlkPtr block_;

  CBlockStmt(CBlkPtr block);
  void code_gen();
};

class CExprStmt: public CStmt {
public:
  CExprPtr expr_;

  CExprStmt(CExprPtr expr);
  void code_gen();
};

class CIfElseStmt: public CStmt {
public:
  CCondPtr cond_expr_;
  CStmtPtr if_stmt_;
  CStmtPtr else_stmt_;
  int if_label_;
  int else_label_;
  int old_fall_label_;
  bool has_else_;

  CIfElseStmt(CCondPtr cond_expr,
              CStmtPtr if_stmt,
              CStmtPtr else_stmt=nullptr);
  void code_gen();
};

class CWhileStmt: public CStmt {
public:
  CCondPtr cond_expr_;
  CStmtPtr loop_stmt_;
  int loop_label_;
  int old_start_label_;
  int old_next_label_;
  int old_fall_label_;

  CWhileStmt(CCondPtr cond_expr, CStmtPtr loop_stmt);
  void code_gen();
};

class CBreakStmt: public CStmt {
public:
  void code_gen();
};

class CContStmt: public CStmt {
public:
  void code_gen();
};

class CRetStmt: public CStmt {
public:
  CExprPtr ret_val_;
  DataType ret_type_;

  explicit CRetStmt(CExprPtr ret_val=nullptr);
  void code_gen();
};

class CExpr: public CNode {
public:
  ExprType expr_type_;
  int val_;
  OpType op_type_;
  string sysy_id_;
  string eeyore_id_;
  bool has_folded_=false;
  bool has_temped_=false;

  CExpr(ExprType expr_type,
        int val=0,
        OpType op_type=opNONE,
        string sysy_id="");
  virtual void const_fold();
  virtual void gen_temp();
};

class CLValExpr: public CExpr {
public:
  string lval_name_;
  CExprPtr first_dim_index_;
  CExprPtr index_expr_;
  CExprPtrList index_;

  CLValExpr(string lval_name, CExprPtr first_dim_index);
  void const_fold();
};

class CArithExpr: public CExpr {
public:
  CExprPtr lhs_expr_;
  CExprPtr rhs_expr_;

  CArithExpr(OpType op_type,
             CExprPtr lhs_expr,
             CExprPtr rhs_expr=nullptr);
  void const_fold();
};

class CCallExpr: public CExpr {
public:
  string func_name_;
  CExprPtrList params_;

  CCallExpr(string func_name,
            CExprPtrList params=CExprPtrList());
  void const_fold();
  void gen_temp();
};

class CCondExpr: public CExpr {
public:
  CExprPtr lhs_expr_;
  CExprPtr rhs_expr_;
  int true_label_;
  int false_label_;

  CCondExpr(OpType op_type,
            CExprPtr lhs_expr,
            CExprPtr rhs_expr=nullptr);
  void traverse();
};

#endif