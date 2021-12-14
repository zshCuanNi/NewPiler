/* Basic Instructions of EEYORE:
 * SB = RVal
 * SB = call FUNC
 * SB = UnOp RVal
 * SB = RVal BinOp RVal
 * SB = SB[RVal]
 * SB[RVal] = RVal
 * if RVal Op RVal goto L
 * goto L
 * param RVal
 * call FUNC
 * return (RVal)
 * L:
 * 
 * var (NUM) SB
 * FUNC [NUM]
 * end FUNC
 */

#ifndef E_AST_H
#define E_AST_H

#include "e_symtab.hpp"

class ENode;
class EExpr;
class ESymbol;
class ENumber;
class ECall;
class EAssign;
class EReturn;
class EGoto;
class EIf;

using ENodePtr = ENode*;
using EExprPtr = EExpr*;
using ECallPtr = ECall*;
using EAsnPtr = EAssign*;
using ERetPtr = EReturn*;
using EGotoPtr = EGoto*;
using EIfPtr = EIf*;
typedef vector<EExprPtr> EExprPtrList;

enum LineType { lASN, lGOTO, lIF, lCALL, lRET, lEXPR };
enum EExprType { eeNUM, eeSB, eeCALL, eeOP, eeARR };

class ENode {
public:
  bool is_entry_ = false, is_exit_ = false;
  vector<int> labels_ = vector<int>();
  
  LineType line_type_;
  int lineno_;
  int stmtno_;
  EStmtList preds_, succs_;
  VarSet def_, use_, live_in_, live_out_;
  
  ENode(LineType line_type);
  virtual string debug_print();
  void add_pred(ENodePtr pred);
  void add_succ(ENodePtr succ);
};

class EExpr: public ENode {
public:
  EExprType expr_type_;
  string op_;
  EExprPtr lhs_, rhs_;

  EExpr(EExprType expr_type,
        string op = "",
        EExprPtr lhs = nullptr,
        EExprPtr rhs = nullptr); 
  string debug_print();
};

class ESymbol: public EExpr {
public:
  string id_;

  ESymbol(string id);
  string debug_print();
};

class ENumber: public EExpr {
public:
  int val_;

  ENumber(int val);
  string debug_print();
};

class ECall: public EExpr {
public:
  string func_id_;
  EExprPtrList params_; // ENumber or ESymbol

  ECall(string func_id, EExprPtrList params);
  string debug_print();
};

class EAssign: public ENode {
public:
  EExprPtr lhs_, rhs_; // lhs_: SYMBOL or SYMBOL[RVal]
  
  EAssign(EExprPtr lhs, EExprPtr rhs);
  string debug_print();
};

class EReturn: public ENode {
public:
  EExprPtr ret_val_; // SYMBOL or NUM

  EReturn(EExprPtr ret_val = nullptr);
  string debug_print();
};

class EIf: public ENode {
public:
  EExprPtr cond_;
  int where_;

  EIf(EExprPtr cond, int where);
  string debug_print();
};

class EGoto: public ENode {
public:
  int where_;

  EGoto(int where);
  string debug_print();
};

#endif
