#include "e_ast.hpp"

ENode::ENode(LineType line_type): line_type_(line_type) {}

EExpr::EExpr(EExprType expr_type, string op, EExprPtr lhs, EExprPtr rhs)
:ENode(lEXPR), expr_type_(expr_type), op_(op), lhs_(lhs), rhs_(rhs) {
  if (expr_type_ == eeCALL) line_type_ = lCALL;
}

ESymbol::ESymbol(string id): EExpr(eeSB), id_(id) {}

ENumber::ENumber(int val): EExpr(eeNUM), val_(val) {}

ECall::ECall(string func_id, EExprPtrList params):
EExpr(eeCALL), func_id_(func_id), params_(params) {}

EAssign::EAssign(EExprPtr lhs, EExprPtr rhs):
ENode(lASN), lhs_(lhs), rhs_(rhs) {}

EReturn::EReturn(EExprPtr ret_val): ENode(lRET), ret_val_(ret_val) {}

EIf::EIf(EExprPtr cond, int where):
ENode(lIF), cond_(cond), where_(where) {}

EGoto::EGoto(int where): ENode(lGOTO), where_(where) {}
