#include "sysyast.hpp"
#include <cstdarg>
#include <cassert>
#include <unistd.h>

extern FILE* f_log;
extern CSymbolTable* sym_tab;
int temp_cnt;
int fall_label=0;
int while_start_label=0;
int while_next_label=0;
int label_cnt=0;
static const string op_str[] =
  { "+", "-", "*", "/", "%", ">", "<", ">=", "<=", "==", "!=",
    "+", "-", "!" };

inline int new_label() { return label_cnt++; }

/* implemented by LianYueBiao, but only supported by Microsoft
 * https://blog.csdn.net/alvinlyb/article/details/90039254
 */
// static string format(const char* pszFmt, ...) {
// 	string str;
// 	va_list args;
// 	va_start(args, pszFmt);
// 	{
// 		int nLength = _vscprintf(pszFmt, args); MS version
// 		nLength += 1;
// 		vector<char> vectorChars(nLength);
// 		_vsnprintf(vectorChars.data(), nLength, pszFmt, args); MS version
// 		str.assign(vectorChars.data());
// 	}
// 	va_end(args);
// 	return str;
// }

/* provided by iFreilicht
 * https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
 */
template<typename ... Args>
string format( const string& format, Args ... args )
{
    int size_s = snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    auto size = static_cast<size_t>( size_s );
    auto buf = std::make_unique<char[]>( size );
    snprintf( buf.get(), size, format.c_str(), args ... );
    return string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

void CNode::code_gen() {}

void CDeclList::code_gen() {
  for (CVarDPtr decl: decl_list_)
    decl->code_gen();
  for (CVarDPtr decl: decl_list_)
    codes_.splice(codes_.end(), decl->codes_);
}

void CVarDecl::set_widths() {
  // int var[][w1][w2]...
  if (is_ptr_)
    widths_.push_back(0);
  
  CExprPtr cur_width = first_width_;
  size_ = 1;
  while (cur_width) {
    cur_width->const_fold();
    size_ *= cur_width->val_;
    widths_.push_back(cur_width->val_);
    cur_width = D_CAST(CExprPtr)(cur_width->next_);
  }
}

/* helper but not member function,
 * actually dealing with params widths and first_value
 */
CExprPtrList CVarDecl::set_values(vector<int> widths,
                                  CExprPtr first_value,
                                  int filled) {
  CExprPtrList ret;
  // non-array
  if (widths.empty()) {
    first_value->const_fold();
    first_value->gen_temp();
    ret.push_back(first_value);
    return ret;
  }
  // array
  bool is_to_fill = (filled == 0);
  int to_fill = 1;
  for (int width: widths) to_fill *= width;
  CExprPtr cur_node = first_value;
  // flatten first_value's tree structure
  while (cur_node) {
    if (cur_node->expr_type_ == eINIT) {
    // non-leaf node
      vector<int> child_widths = vector<int>(widths.begin()+1, widths.end());
      int child_to_fill = to_fill / widths[0];
      int child_filled = filled % child_to_fill;
      CExprPtrList child_values = set_values(
          child_widths,
          D_CAST(CExprPtr)(cur_node->child_),
          child_filled);
      filled += child_values.size();
      for (CExprPtr value: child_values)
        ret.push_back(value);
    } else {
    // leaf node
      cur_node->const_fold();
      cur_node->gen_temp();
      ret.push_back(cur_node);
      filled++;
    }
    cur_node = D_CAST(CExprPtr)(cur_node->next_);
  }
  // insert 0's behind
  if (is_to_fill) {
    CExprPtr zero = NEW(CExpr)(eNUM);
    zero->const_fold();
    assert(to_fill >= filled);
    ret.insert(ret.end(), to_fill - filled, zero);
  }
  return ret;
}

/* helper function to generate var declaration according to entry */
static string gen_var_decl(CEnVTabPtr entry, bool is_indent) {
  string indent = is_indent? "\t": "";
  if (!entry->is_arr_)
    return format("%svar %s",
                   indent.c_str(),
                   entry->eeyore_id_.c_str());
  int sz = INT_SIZE;
  for (int width: entry->widths_) sz *= width;
  return format("%svar %d %s",
                 indent.c_str(),
                 sz, 
                 entry->eeyore_id_.c_str());
}

void CVarDecl::gen_local_var_init(CEnVTabPtr entry) {
  if (is_arr_)
    for (int index = 0; index < (int)values_.size(); ++index) {
      codes_.splice(codes_.end(), values_[index]->codes_);
      codes_.push_back(format("\t%s[%d] = %s",
                              entry->eeyore_id_.c_str(),
                              index * INT_SIZE,
                              values_[index]->eeyore_id_.c_str()));
    }
  else {
    codes_.splice(codes_.end(), values_[0]->codes_);
    codes_.push_back(format("\t%s = %s",
                            entry->eeyore_id_.c_str(),
                            values_[0]->eeyore_id_.c_str()));
  }
}

void CVarDecl::code_gen() {
  set_widths(); 

  // can be initialized
  if (first_value_) {
    if (is_arr_) values_ = set_values(widths_, D_CAST(CExprPtr)(first_value_->child_));
    else values_ = set_values(widths_, first_value_);
  }

  // register variable in symbol table
  vector<int> c_values;
  for (CExprPtr value: values_) c_values.push_back(value->val_);
  // set 0 for implicitly initialized vars (useful for global vars)
  c_values.insert(c_values.end(), size_ - values_.size(), 0);
  CEnVTabPtr entry = sym_tab->register_var(
      id_, widths_, c_values, is_const_,
      is_param_, is_ptr_, is_arr_, false);

  // no need to generate code for function parameters
  if (is_param_) return;
  
  // declare global variables
  if (sym_tab->blk_id_ == GLOBAL_BLOCK) {
      codes_.push_back(gen_var_decl(entry, false));
      // initialize non-array global variables in global scope
      if (!is_arr_) 
        codes_.push_back(format("%s = %d",
                                entry->eeyore_id_.c_str(),
                                entry->values_[0]));
      return;
  }
  // initialize local variables
  // Declarations will be generated in FuncDef,
  // and be put at the beginning.
  if (values_.empty()) return;
  gen_local_var_init(entry);
}

void CFuncDef::code_gen() {
  temp_cnt = 0;
  // add a man-made return if no return statement exists
  CRetStmt* man_made_ret;
  if (ret_type_ == datINT)
    man_made_ret = NEW(CRetStmt)(NEW(CExpr(eNUM)));
  else
    man_made_ret = NEW(CRetStmt)();
  if (func_body_->item_list_.empty() ||
      !(*func_body_->item_list_.rbegin())->is_ret_stmt_)
    func_body_->item_list_.push_back(man_made_ret);
  
  // register function in symbol table
  CEnFTabPtr entry = sym_tab->register_func(ret_type_, id_);
  
  sym_tab->new_blk();
  // fisrt do registration and code generation of parameters
  for (CVarDPtr param: params_)
    param->code_gen();
  // next generate code for function body
  func_body_->code_gen();

  // function header
  codes_.push_back(format("%s [%d]",
                          entry->eeyore_id_.c_str(),
                          entry->params_.size()));
  // declare local variables
  for (CEnVTabPtr var: entry->locals_)
    // no need to declare parameters
    if (!var->is_param_)
      codes_.push_back(gen_var_decl(var, true));
  // initialize global arrays at the beginning of main()
  if (id_ == "main")
    for (auto& id_var_pair: sym_tab->blk_stack_[GLOBAL_BLOCK]) {
      CEnVTabPtr var = id_var_pair.second;
      if (var->is_arr_)
        for (int index = 0; index < (int)var->values_.size(); ++index)
          if (var->values_[index])
          // zero elements will be referenced directly
            codes_.push_back(format("\t%s[%d] = %d",
                                    var->eeyore_id_.c_str(),
                                    index * INT_SIZE,
                                    var->values_[index]));
    }
  // add the main function body
  codes_.splice(codes_.end(), func_body_->codes_);
  // function tail
  codes_.push_back(format("end %s", entry->eeyore_id_.c_str()));
  codes_.push_back("");
  sym_tab->delete_blk();
}

void CBlock::code_gen() {
  if (item_list_.empty()) return;
  // block will be created in FuncDef if the block is a function body
  if (!is_func_body_)
    sym_tab->new_blk();
  for (CNodePtr item: item_list_)
    item->code_gen();
  for (CNodePtr item: item_list_)
    codes_.splice(codes_.end(), item->codes_);
  if (!is_func_body_)
    sym_tab->delete_blk();
}

void CAssignStmt::code_gen() {
  // ???
  rhs_expr_->const_fold();
  lhs_var_->const_fold();
  if (lhs_var_->expr_type_ == eARR) rhs_expr_->gen_temp();

  codes_.splice(codes_.end(), rhs_expr_->codes_);
  codes_.splice(codes_.end(), lhs_var_->codes_);
  // ??? omit cast for lhs_var_ here
  codes_.push_back(format("\t%s = %s",
                          lhs_var_->eeyore_id_.c_str(),
                          rhs_expr_->eeyore_id_.c_str()));
}

void CBlockStmt::code_gen() {
  block_->code_gen();
  codes_.splice(codes_.end(), block_->codes_);
}

void CExprStmt::code_gen() {
  expr_->const_fold();
  // can we switch const_fold() and eCALL judgement???
  if (expr_->expr_type_ != eCALL) return;
  // find this function in function table and get its ret_type
  CCallExpr* func_call = (CCallExpr*) expr_;
  CEnFTabPtr entry = sym_tab->find_func(func_call->func_name_);
  if (entry->ret_type_ == datVOID) {
    codes_.splice(codes_.end(), func_call->codes_);
    codes_.push_back(format("\t%s", func_call->eeyore_id_.c_str()));
  } else {
    func_call->gen_temp();
    codes_.splice(codes_.end(), func_call->codes_);
  }
}

void CIfElseStmt::code_gen() {
  old_fall_label_ = fall_label;
  fall_label = new_label();

  if_stmt_->code_gen();
  if (has_else_) else_stmt_->code_gen();

  // set labels and traverse condition expression
  cond_expr_->false_label_ = else_label_ = new_label();
  if (has_else_) cond_expr_->true_label_ = if_label_ = new_label();
  else cond_expr_->true_label_ = if_label_ = fall_label;
  cond_expr_->traverse();
  
  codes_.splice(codes_.end(), cond_expr_->codes_);
  if (has_else_) {
    codes_.push_back(format("l%d:", if_label_));
    codes_.splice(codes_.end(), if_stmt_->codes_);
    int next_label = new_label();
    codes_.push_back(format("\tgoto l%d", next_label));
    codes_.push_back(format("l%d:", else_label_));
    codes_.splice(codes_.end(), else_stmt_->codes_);
    codes_.push_back(format("l%d:", next_label));
  } else {
    codes_.splice(codes_.end(), if_stmt_->codes_);
    codes_.push_back(format("l%d:", else_label_));
  }

  fall_label = old_fall_label_;
}

void CWhileStmt::code_gen() {
  old_start_label_ = while_start_label;
  old_next_label_ = while_next_label;
  old_fall_label_ = fall_label;
  while_start_label = new_label();
  while_next_label = new_label();
  fall_label = new_label();

  loop_stmt_->code_gen();

  //set labels and traverse condition expressions
  cond_expr_->true_label_ = loop_label_ = fall_label;
  cond_expr_->false_label_ = while_next_label;
  cond_expr_->traverse();

  codes_.push_back(format("l%d:", while_start_label));
  codes_.splice(codes_.end(), cond_expr_->codes_);
  codes_.push_back(format("l%d:", loop_label_));
  codes_.splice(codes_.end(), loop_stmt_->codes_);
  codes_.push_back(format("\tgoto l%d", while_start_label));
  codes_.push_back(format("l%d:", while_next_label));

  while_start_label = old_start_label_;
  while_next_label = old_next_label_;
  fall_label = old_fall_label_;
}

/* only while loop is supoorted */
void CBreakStmt::code_gen() {
  codes_.push_back(format("\tgoto l%d", while_next_label));
}

/* only while loop is supoorted */
void CContStmt::code_gen() {
  codes_.push_back(format("\tgoto l%d", while_start_label));
}

void CRetStmt::code_gen() {
  if (ret_type_ == datVOID) {
    codes_.push_back("\treturn");
    return;
  }
  ret_val_->const_fold();
  ret_val_->gen_temp();

  codes_.splice(codes_.end(), ret_val_->codes_);
  codes_.push_back(format("\treturn %s", ret_val_->eeyore_id_.c_str()));
}

void CExpr::const_fold() {
  if (has_folded_) { /* printf("folded\n"); */ return; }
  switch (expr_type_) {
    case eNUM:
      eeyore_id_ = to_string(val_); break;
    case eVAR: {
      CEnVTabPtr entry = sym_tab->find_var(sysy_id_);
      eeyore_id_ = entry->eeyore_id_;
      if (entry->is_const_ && entry->widths_.empty()) {
        expr_type_ = eNUM;
        eeyore_id_ = to_string(entry->values_[0]);
      }
      break;
    }
    default: break;
  }
  has_folded_ = true;
}

void CExpr::gen_temp() {
  if (has_temped_ || expr_type_ == eNUM || expr_type_ == eVAR) return;
  codes_.push_back(format("\tt%d = %s",
                          temp_cnt,
                          eeyore_id_.c_str()));
  // #t is prefix of sysy_id for temped expressions
  sysy_id_ = format("#t%d", temp_cnt);
  eeyore_id_ = format("t%d", temp_cnt++);
  sym_tab->register_var(sysy_id_);
  has_temped_ = true;
}

/* generate expr node for calculating offset from 'index' */
static CExprPtr index_unfold(const CExprPtrList index,
                             const CEnVTabPtr arr_entry) {
  if (index.empty()) return NEW(CExpr)(eNUM); /* if exists ??? */

  CExprPtr ret_expr = index[0];
  for (int i = 1; i < (int)index.size(); ++i) {
    CExprPtr scale_expr = NEW(CExpr)(eNUM, arr_entry->widths_[i]);
    ret_expr =
        NEW(CArithExpr)(
            opADD,
            NEW(CArithExpr)(
                opMUL,
                ret_expr,
                scale_expr
            ),
            index[i]
        );
  }
  return ret_expr;
}

void CLValExpr::const_fold() {
  if (has_folded_) { /* printf("folded\n"); */ return; }
  CEnVTabPtr entry = sym_tab->find_var(lval_name_);
  if (entry->is_arr_) {
  // set indexes for array
    CExprPtr cur_node = first_dim_index_;
    while (cur_node) {
      cur_node->const_fold();
      cur_node->gen_temp();
      index_.push_back(cur_node);
      cur_node = D_CAST(CExprPtr)(cur_node->next_);
    }
  } else
    expr_type_ = eVAR;

  /* omit a if-return struct ??? */
  switch (expr_type_) {
    case eNUM: break;  /* if exists??? */
    case eVAR:
      if (entry->is_const_) {
        expr_type_ = eNUM;
        val_ = entry->values_[0];
        eeyore_id_ = to_string(val_);
      } else 
        eeyore_id_ = entry->eeyore_id_;
      break;
    default: {
    // array, generate expr nodes to calculate offset
      CExprPtr arr_name = NEW(CExpr)(eVAR, 0, opNONE, lval_name_);
      index_expr_ = index_unfold(index_, entry);
      if (index_.size() == entry->widths_.size())
      // value of lval (array)
        index_expr_ = 
            NEW(CArithExpr)(
                opLOAD,
                arr_name,
                NEW(CArithExpr)(
                    opMUL,
                    index_expr_,
                    NEW(CExpr)(eNUM, INT_SIZE)
                )
            );
      else {
      // pointer, address of an array
        expr_type_ = ePTR;
        int size = INT_SIZE;
        for (int i = index_.size(); i < (int)entry->widths_.size(); ++i)
          size *= entry->widths_[i];
        index_expr_ = 
            NEW(CArithExpr)(
                opADD,
                arr_name,
                NEW(CArithExpr)(
                    opMUL,
                    index_expr_,
                    NEW(CExpr)(eNUM, size)
                )
            );
      }
      // arithmetic expressions do not need to generate temp variables
      index_expr_->const_fold();
      codes_.splice(codes_.end(), index_expr_->codes_);
      eeyore_id_ = index_expr_->eeyore_id_;
      /* omit sysy_id_ assignment ??? */
      break;
    }
  }
  has_folded_ = true;
}

void CArithExpr::const_fold() {
  if (has_folded_) { /* printf("folded\n"); */ return; }
  /* omit a if-return struct ??? */
  if (rhs_expr_) {
  // binary op
    /* add according to my understanding ??? */
    lhs_expr_->const_fold(); lhs_expr_->gen_temp();
    rhs_expr_->const_fold(); rhs_expr_->gen_temp();
    if (lhs_expr_->expr_type_ == eNUM && rhs_expr_->expr_type_ == eNUM)
    {
    // get constant result after constant lhs and rhs expr
      switch (op_type_) {
        case opADD:
          val_ = lhs_expr_->val_ + rhs_expr_->val_; break;
        case opSUB:
          val_ = lhs_expr_->val_ - rhs_expr_->val_; break;
        case opMUL:
          val_ = lhs_expr_->val_ * rhs_expr_->val_; break;
        case opDIV:
          val_ = lhs_expr_->val_ / rhs_expr_->val_; break;
        case opMOD:
          val_ = lhs_expr_->val_ % rhs_expr_->val_; break;
        case opGE:
          val_ = lhs_expr_->val_ > rhs_expr_->val_; break;
        case opLE:
          val_ = lhs_expr_->val_ < rhs_expr_->val_; break;
        case opGEQ:
          val_ = lhs_expr_->val_ >= rhs_expr_->val_; break;
        case opLEQ:
          val_ = lhs_expr_->val_ <= rhs_expr_->val_; break;
        case opEQ:
          val_ = lhs_expr_->val_ == rhs_expr_->val_; break;
        case opNEQ:
          val_ = lhs_expr_->val_ != rhs_expr_->val_; break;
        default: break;
      }
      expr_type_ = eNUM;
      eeyore_id_ = to_string(val_);
    } else {
    // expicitly generate code for non-constants
      assert(op_type_ == opLOAD || int(op_type_) <= 10);
      if (int(op_type_) <= 10 && int(op_type_) >= 0)
        eeyore_id_ = format("%s %s %s",
                            lhs_expr_->eeyore_id_.c_str(),
                            op_str[int(op_type_)].c_str(),
                            rhs_expr_->eeyore_id_.c_str());
      if (op_type_ == opLOAD)
        eeyore_id_ = format("%s[%s]",
                            lhs_expr_->eeyore_id_.c_str(),
                            rhs_expr_->eeyore_id_.c_str());
      codes_.splice(codes_.end(), lhs_expr_->codes_);
      codes_.splice(codes_.end(), rhs_expr_->codes_);
    }
  } else {
  // unuary op
    /* add according to my understanding ??? */
    lhs_expr_->const_fold(); lhs_expr_->gen_temp();
    if (lhs_expr_->expr_type_ == eNUM) {
    // get constant result after constant lhs and rhs expr
      switch (op_type_) {
        case opPOS:
          val_ = +lhs_expr_->val_; break;
        case opNEG:
          val_ = -lhs_expr_->val_; break;
        case opNOT:
          val_ = !lhs_expr_->val_; break;
        default: break;
      }
      expr_type_ = eNUM;
      eeyore_id_ = to_string(val_);
    } else {
    // expicitly generate code for non-constants
      assert(int(op_type_) >= 11 && int(op_type_) <= 13);
      eeyore_id_ = format("%s%s",
                          op_str[int(op_type_)].c_str(),
                          lhs_expr_->eeyore_id_.c_str());
      codes_.splice(codes_.end(), lhs_expr_->codes_);
    }
  }
  /* omit sysy_id_ assignment ??? */
  has_folded_ = true;
}

void CCallExpr::const_fold() {
  if (has_folded_) { /* printf("folded\n"); */ return; }
  /* omit a if-return struct ??? */
  // prepare parameters
  for (CExprPtr param: params_) {
    param->const_fold();
    param->gen_temp();
  }
  for (CExprPtr param: params_)
    codes_.splice(codes_.end(), param->codes_);
  // call the function
  for (CExprPtr param: params_)
    codes_.push_back(format("\tparam %s", param->eeyore_id_.c_str()));
  eeyore_id_ = format("call f_%s", func_name_.c_str());
  /* omit sysy_id_ assignment ??? */
  has_folded_ = true;
}

void CCallExpr::gen_temp() {
  if (has_temped_ || expr_type_ == eNUM || expr_type_ == eVAR) return;
  codes_.push_back(format("\tt%d = %s", temp_cnt, eeyore_id_.c_str()));
  // #t is prefix of sysy_id for temped expressions
  sysy_id_ = format("#t%d", temp_cnt);
  eeyore_id_ = format("t%d", temp_cnt++);
  /* BIG DEAL ??? */
  // codes_.push_back(format("\t%s = call f_%s",
  //                         eeyore_id_.c_str(),
  //                         func_name_.c_str()));
  sym_tab->register_var(sysy_id_);
  has_temped_ = true;
}

void CCondExpr::traverse() {
  if (rhs_expr_ == nullptr) {
  // unary op or just a lval: if (y>4) | if (x)
  /* maybe work but doubt ??? */
    lhs_expr_->const_fold();
    lhs_expr_->gen_temp();
    codes_.splice(codes_.end(), lhs_expr_->codes_);

    // labels have been set before calling travverse()
    // if-else struct can be rewritten ???
    if (true_label_ != fall_label)
      if (false_label_ == fall_label)
        codes_.push_back(format("\tif %s != 0 goto l%d",
                                lhs_expr_->eeyore_id_.c_str(),
                                true_label_));
      else
        codes_.push_back(format("\tif %s == 0 goto l%d",
                                lhs_expr_->eeyore_id_.c_str(),
                                false_label_));
    else if (false_label_ != fall_label)
      codes_.push_back(format("\tif %s == 0 goto l%d",
                                lhs_expr_->eeyore_id_.c_str(),
                                false_label_));
  } else {
  // binary op, traverse both side of expressions
    CCondPtr lhs_cond = (CCondPtr) lhs_expr_;
    CCondPtr rhs_cond = (CCondPtr) rhs_expr_;

    switch (op_type_) {
      case opLAND:
        lhs_cond->true_label_ = fall_label;
        lhs_cond->false_label_ =
            false_label_ == fall_label? new_label(): false_label_;
        rhs_cond->true_label_ = true_label_;
        rhs_cond->false_label_ = false_label_;
        lhs_cond->traverse(); rhs_cond->traverse();

        codes_.splice(codes_.end(), lhs_cond->codes_);
        codes_.splice(codes_.end(), rhs_cond->codes_);
        if (false_label_ == fall_label)
          codes_.push_back(format("l%d:", lhs_cond->false_label_));
        break;
      case opLOR:
        lhs_cond->true_label_ =
            true_label_ == fall_label? new_label(): true_label_;
        lhs_cond->false_label_ = fall_label;
        rhs_cond->true_label_ = true_label_;
        rhs_cond->false_label_ = false_label_;
        lhs_cond->traverse(); rhs_cond->traverse();
        
        codes_.splice(codes_.end(), lhs_cond->codes_);
        codes_.splice(codes_.end(), rhs_cond->codes_);
        if (true_label_ == fall_label)
          codes_.push_back(format("l%d:", lhs_cond->true_label_));
        break;
      default: break;
    }
  }
}