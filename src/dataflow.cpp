#include <cassert>
#include <queue>
#include "newpiler.hpp"

void CFG_dfs(EBBlkPtr cur_blk) {
  if (cur_blk->is_vis_) return;
  cur_blk->is_vis_ = true;
  for (auto succ: cur_blk->succs_)
    CFG_dfs(succ);
}

void Newpiler::gen_control_flow_graph() {
  assert(e_sym_tab);
  for (auto func_id: e_sym_tab->func_ids_) {
    // there is no control flow in the global scope
    if (func_id == "global") continue;
    auto func = e_sym_tab->find_func(func_id);

    // TODO: rewrite CFG generation
    // 1. set preds and succs for stmts
    // 2. do dfs among stmts to clear unreachable codes
    // 3. set is_entry_'s and is_exit_'s
    // 4. generate basic bocks according to entry/exit flags
    // 5. assign new labels to blocks and stmts
    // compile 065_sort_test5 as a check

    assert(!func->stmts_.empty());
    // first trvarsal, set all is_entry_'s and explicit is_exit_'s for nodes
    for (auto it = func->stmts_.begin(); it != func->stmts_.end(); it++) {
      auto stmt = *it;
      // first statement is an entry
      if (it == func->stmts_.begin()) stmt->is_entry_ = true;
      switch (stmt->line_type_) {
        case lGOTO: {
          stmt->is_exit_ = true;
          auto to_stmt = func->label_to_node_[(EGotoPtr(stmt))->where_];
          to_stmt->is_entry_ = true;
          break;
        }
        case lIF: {
          stmt->is_exit_ = true;
          auto to_stmt = func->label_to_node_[(EIfPtr(stmt))->where_];
          to_stmt->is_entry_ = true;
          auto next_it = it + 1;
          if (next_it != func->stmts_.end())
            (*next_it)->is_entry_ = true;
          break;
        }
        case lRET:
          stmt->is_exit_ = true;
          break;
        default: break;
      }
    }
    // second traversal, set implicit is_exit_'s
    for (auto it = func->stmts_.begin(); it != func->stmts_.end(); it++) {
      auto stmt = *it;
      auto next_it = it + 1;
      if (next_it == func->stmts_.end()) stmt->is_exit_ = true;
      else if ((*next_it)->is_entry_) stmt->is_exit_ = true;
    }
    // third traversal, create blocks and a mapping from node to block
    int blk_cnt = 0;
    unordered_map<ENodePtr, EBBlkPtr> node_to_blk;
    for (auto stmt: func->stmts_)
      if (stmt->is_entry_) {
        node_to_blk[stmt] = NEW(EBasicBlock)(blk_cnt++);
        func->append_block(node_to_blk[stmt]);
      }
    // fourth traversal, fill all blocks
    for (auto it = func->stmts_.begin(); it != func->stmts_.end(); it++) {
      auto stmt = *it;
      if (!stmt->is_entry_) continue;

      EBBlkPtr b_blk = node_to_blk[stmt];
      while (!(*it)->is_exit_) {
        b_blk->append_stmt(*it);
        it++;
      }
      // deal with block exit
      b_blk->append_stmt(*it);
      auto exit_node = *it;
      switch (exit_node->line_type_) {
        case lGOTO: {
          EBBlkPtr to_blk =
              node_to_blk[func->label_to_node_[(EGotoPtr(exit_node))->where_]];
          b_blk->add_succ(to_blk);
          to_blk->add_pred(b_blk);
          break;
        }
        case lIF: {
          EBBlkPtr true_blk =
              node_to_blk[func->label_to_node_[(EIfPtr(exit_node))->where_]];
          b_blk->add_succ(true_blk);
          true_blk->add_pred(b_blk);
          auto next_it = it + 1;
          if (next_it != func->stmts_.end()) {
            EBBlkPtr false_blk = node_to_blk[*next_it];
            b_blk->add_succ(false_blk);
            false_blk->add_pred(b_blk);
          }
          break;
        }
        case lRET: break;
        default: // destination of 'goto' instructions
        {
          auto next_it = it + 1;
          if (next_it != func->stmts_.end()) {
            EBBlkPtr to_blk = node_to_blk[*next_it];
            b_blk->add_succ(to_blk);
            to_blk->add_pred(b_blk);
          }
        }
      }
    }
    // do dfs among blocks to clear unreachable codes (blocks)
    BlockList without_dead_block;
    blk_cnt = 0;
    CFG_dfs(func->blocks_[0]);
    for (auto b_blk: func->blocks_)
      if (b_blk->is_vis_) {
        b_blk->blk_id_ = blk_cnt++;
        without_dead_block.push_back(b_blk);
      }
    func->blocks_ = without_dead_block;
  }

  // assign each block one label (= the block number before it counting in global)
  // rename labels and redirect 'goto' structures
  int total_blk_cnt = 0;
  for (auto func_id: e_sym_tab->func_ids_) {
    // there is no label in the global scope
    if (func_id == "global") continue;
    auto func = e_sym_tab->find_func(func_id);
    for (auto b_blk: func->blocks_) {
      for (auto stmt: b_blk->stmts_) {
        stmt->labels_.clear();
        if (!b_blk->preds_.empty() && stmt->is_entry_) {
          // if the only pred is of the form 'if x goto x' and its false succ is b_blk
          // there is no need to assign a label for stmt
          auto pred = b_blk->preds_[0];
          if (b_blk->preds_.size() != 1 || pred->succs_.size() != 2
                                        || pred->succs_[1] != b_blk)
            stmt->labels_.push_back(total_blk_cnt + b_blk->blk_id_);
        }
        if (!stmt->is_exit_) continue;
        if (!b_blk->succs_.empty()) {
          // for if ... goto ..., succs_[0] is true label
          auto succ = *b_blk->succs_.begin();
          switch (stmt->line_type_) {
            case lGOTO: 
              (EGotoPtr(stmt))->where_ = total_blk_cnt + succ->blk_id_;
              break;
            case lIF:
              (EIfPtr(stmt))->where_ = total_blk_cnt + succ->blk_id_;
              break;
            default: break;
          }
        }
      }
    }
    total_blk_cnt += (int)func->blocks_.size();
  }
}

void Newpiler::set_stmt_pred_succ() {
  for (auto func_id: e_sym_tab->func_ids_) {
    // there is no need to set preds and succs for stmts in the global scope
    if (func_id == "global") continue;
    auto func = e_sym_tab->find_func(func_id);
    for (auto b_blk: func->blocks_) {
      // relations between blocks
      auto entry_stmt = b_blk->get_entry_stmt();
      auto exit_stmt = b_blk->get_exit_stmt();
      for (auto pred: b_blk->preds_)
        entry_stmt->add_pred(pred->get_exit_stmt());
      for (auto succ: b_blk->succs_)
        exit_stmt->add_succ(succ->get_entry_stmt());
      // relations in the block
      for (auto it = b_blk->stmts_.begin(); it != b_blk->stmts_.end(); it++) {
        if (it != b_blk->stmts_.begin()) (*it)->preds_.push_back(*(it-1));
        if (it != b_blk->stmts_.end()-1) (*it)->succs_.push_back(*(it+1));
      }
    }
  }
}

void Newpiler::order_stmts() {
  for (auto func_id: e_sym_tab->func_ids_) {
    if (func_id == "global") continue;
    auto func = e_sym_tab->find_func(func_id);
    int stmt_cnt = 0;
    func->stmts_.clear();
    for (auto b_blk: func->blocks_)
      for (auto stmt: b_blk->stmts_) {
        stmt->stmtno_ = stmt_cnt++;
        func->stmts_.push_back(stmt);
      }
  }
}

void set_def_use(ENodePtr stmt, EExprPtr expr, bool is_def = false) {
  VarSet& def = stmt->def_;
  VarSet& use = stmt->use_;
  switch (expr->expr_type_) {
    case eeNUM: break;
    case eeSB:
      if (is_def) def.insert(((ESymbol*)expr)->id_);
      else use.insert(((ESymbol*)expr)->id_);
      break;
    case eeCALL: assert(!is_def);
      for (auto param: (((ECall*)expr)->params_))
        set_def_use(stmt, param);
      break;
    case eeOP: assert(!is_def);
      set_def_use(stmt, expr->lhs_);
      if (expr->rhs_) set_def_use(stmt, expr->rhs_);
      break;
    case eeARR:
    // for T0[t0] = xx, t0 and T0 are both used but not defined
    // for xx = T0[t0], to and T0 are apparently all used
      set_def_use(stmt, expr->lhs_);
      // set_def_use(stmt, expr->lhs_, true);
      set_def_use(stmt, expr->rhs_);
      break;
    default: assert(false);
  }
}

/* https://www.seas.upenn.edu/~cis341/current/lectures/lec22.pdf
 * Use a FIFO queue of nodes that might need to be updated.
 * ========== pseudo code ==========
 * for all n, in[n] := Ø, out[n] := Ø
 * w = new queue with all nodes
 * repeat until w is empty
 *   let n = w.pop()                      // pull a node off the queue
 *   old_in = in[n]                       // remember old in[n]
 *   out[n] := ∪_{n'∈succ[n]} in[n']
 *   in[n] := use[n] ∪ (out[n] - def[n])
 *   if (old_in != in[n])                 // if in[n] has changed
 *     for all m in pred[n]               // add to worklist
 *       w.push(m)
 * end
 */
void Newpiler::liveness_analysis() {
  for (auto func_id: e_sym_tab->func_ids_) {
    deque<ENodePtr> work_list;
    if (func_id == "global") continue;
    // calculate def and use for every node in advance
    auto func = e_sym_tab->find_func(func_id);
    for (auto stmt: func->stmts_) {
      work_list.push_front(stmt);
      switch (stmt->line_type_) {
        case lASN:
          set_def_use(stmt, (EAsnPtr(stmt))->lhs_, true);
          set_def_use(stmt, (EAsnPtr(stmt))->rhs_);
          break;
        case lIF:
          set_def_use(stmt, (EIfPtr(stmt))->cond_);
          break;
        case lCALL:
          for (auto param: (ECallPtr(stmt))->params_)
            set_def_use(stmt, param);
          break;
        case lRET:
          if ((ERetPtr(stmt))->ret_val_)
            set_def_use(stmt, (ERetPtr(stmt))->ret_val_);
          break;
        case lGOTO: break;
        default: assert(false);
      }
    }

    // backward dataflow analysis
    while (!work_list.empty()) {
      auto cur_stmt = work_list.front();
      work_list.pop_front();
      VarSet old_in = move(cur_stmt->live_in_);
      for (auto succ: cur_stmt->succs_)
        for (auto in_var: succ->live_in_)
          cur_stmt->live_out_.insert(in_var);
      cur_stmt->live_in_ = cur_stmt->use_;
      for (auto out_var: cur_stmt->live_out_)
        if (cur_stmt->def_.count(out_var) == 0)
          cur_stmt->live_in_.insert(out_var);
      if (old_in != cur_stmt->live_in_)
        for (auto pred: cur_stmt->preds_)
          work_list.push_back(pred);
    }

    // update liveness information of blocks
    for (auto b_blk: func->blocks_) {
      b_blk->live_in_ = b_blk->get_entry_stmt()->live_in_;
      b_blk->live_out_ = b_blk->get_exit_stmt()->live_out_;
    }
  }
}
