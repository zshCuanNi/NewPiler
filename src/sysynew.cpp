#include "sysyast.hpp"

extern int yylineno;
extern FILE* f_log;

void CNode::set_next(CNodePtr next) { next_ = next; }
void CNode::set_child(CNodePtr child) { child_ = child; }

CDeclList::CDeclList(bool is_const):is_const_(is_const) {}

void CDeclList::add_decl(CVarDPtr var_decl) {fprintf(f_log, "%s::%s\n", typeid(*this).name(), __FUNCTION__);
  decl_list_.push_back(var_decl);
}

CVarDecl::CVarDecl(string id,
                   CExprPtr first_width,
                   CExprPtr first_value,
                   bool is_const,
                   bool is_param,
                   bool is_ptr):
id_(id), first_width_(first_width), first_value_(first_value),
is_const_(is_const), is_param_(is_param), is_ptr_(is_ptr) {
  is_arr_ = is_ptr_ || first_width;
}

CFuncDef::CFuncDef(DataType ret_type,
                   string id,
                   CBlkPtr func_body,
                   CVarDPtrList params):
ret_type_(ret_type), id_(id), func_body_(func_body), params_(move(params)) {fprintf(f_log, "%s::%s\n", typeid(*this).name(), __FUNCTION__);}

void CBlock::set_func_body() { is_func_body_ = true; }

void CBlock::add_item(CNodePtr blk_item) {fprintf(f_log, "%s::%s\n", typeid(*this).name(), __FUNCTION__);
  item_list_.push_back(blk_item);
}

CAssignStmt::CAssignStmt(CExprPtr lhs_var,
                         CExprPtr rhs_expr):
lhs_var_(lhs_var), rhs_expr_(rhs_expr) {}

CBlockStmt::CBlockStmt(CBlkPtr block): block_(block) {}

CExprStmt::CExprStmt(CExprPtr expr): expr_(expr) {}

CIfElseStmt::CIfElseStmt(CCondPtr cond_expr,
                         CStmtPtr if_stmt,
                         CStmtPtr else_stmt):
cond_expr_(cond_expr), if_stmt_(if_stmt), else_stmt_(else_stmt) {
  has_else_ = else_stmt_ != nullptr;
}

CWhileStmt::CWhileStmt(CCondPtr cond_expr,
                       CStmtPtr loop_stmt):
cond_expr_(cond_expr), loop_stmt_(loop_stmt) {}

CRetStmt::CRetStmt(CExprPtr ret_val): ret_val_(ret_val) {
  is_ret_stmt_ = true;
  ret_type_ = ret_val_? datINT: datVOID;
}

CExpr::CExpr(ExprType expr_type,
             int val,
             OpType op_type,
             string sysy_id):
expr_type_(expr_type), val_(val), op_type_(op_type), sysy_id_(sysy_id) 
{/* ??? */}

CLValExpr::CLValExpr(string lval_name,
                     CExprPtr first_dim_index):
CExpr(eARR), lval_name_(lval_name), first_dim_index_(first_dim_index) {}

CArithExpr::CArithExpr(OpType op_type,
                       CExprPtr lhs_expr,
                       CExprPtr rhs_expr):
CExpr(eARITH, 0, op_type), lhs_expr_(lhs_expr), rhs_expr_(rhs_expr) {}

CCallExpr::CCallExpr(string func_name,
                     CExprPtrList params):
CExpr(eCALL), func_name_(func_name), params_(move(params)) {fprintf(f_log, "%s::%s\n", typeid(*this).name(), __FUNCTION__);
  // tackle macro commands for time function
  if (func_name_ == "starttime" || func_name_ == "stoptime") {
    func_name_ = "_sysy_" + func_name_;
    params_.push_back(NEW(CExpr)(eNUM, yylineno));
  }
}

CCondExpr::CCondExpr(OpType op_type,
                     CExprPtr lhs_expr,
                     CExprPtr rhs_expr):
CExpr(eCOND, 0, op_type), lhs_expr_(lhs_expr), rhs_expr_(rhs_expr) {}