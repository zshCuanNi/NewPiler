#include <cassert>
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
    auto func = e_sym_tab->func_tab_[func_id];

    assert(!func->stmts_.empty());
    // first trvarsal, set all is_entry_'s and explicit is_exit_'s for nodes
    for (auto it = func->stmts_.begin(); it != func->stmts_.end(); it++) {
      ENodePtr stmt = *it;
      // first statement is an entry
      if (it == func->stmts_.begin()) stmt->is_entry_ = true;
      switch (stmt->line_type_) {
        case lGOTO: {
          stmt->is_exit_ = true;
          ENodePtr to_stmt = func->label_to_node_[(EGotoPtr(stmt))->where_];
          to_stmt->is_entry_ = true;
          break;
        }
        case lIF: {
          stmt->is_exit_ = true;
          ENodePtr to_stmt = func->label_to_node_[(EIfPtr(stmt))->where_];
          to_stmt->is_entry_ = true;
          auto next_it = it; next_it++;
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
      ENodePtr stmt = *it;
      auto next_it = it; next_it++;
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
      ENodePtr stmt = *it;
      if (!stmt->is_entry_) continue;

      EBBlkPtr b_blk = node_to_blk[stmt];
      while (!(*it)->is_exit_) {
        b_blk->append_stmt(*it);
        it++;
      }
      // deal with block exit
      b_blk->append_stmt(*it);
      ENodePtr exit_node = *it;
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
          auto next_it = it; next_it++;
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
          auto next_it = it; next_it++;
          if (next_it != func->stmts_.end()) {
            EBBlkPtr to_blk = node_to_blk[*next_it];
            b_blk->add_succ(to_blk);
            to_blk->add_pred(b_blk);
          }
        }
      }
    }
    // do dfs among blocks to clear dead codes (blocks)
    BlockList without_dead_block;
    blk_cnt = 0;
    CFG_dfs(func->blocks_[0]);
    for (auto b_blk: func->blocks_)
      if (b_blk->is_vis_) {
        b_blk->blk_id_ = blk_cnt++;
        without_dead_block.push_back(b_blk);
      } else 
        delete b_blk;
    func->blocks_ = without_dead_block;
  }

  // assign each block one label (= the block number before it counting in global)
  // rename labels and redirect 'goto' structures
  int total_blk_cnt = 0;
  for (auto func_id: e_sym_tab->func_ids_) {
    // there is no label in the global scope
    if (func_id == "global") continue;
    auto func = e_sym_tab->func_tab_[func_id];
    for (auto b_blk: func->blocks_) {
      for (ENodePtr stmt: b_blk->stmts_) {
        stmt->labels_.clear();
        if (!b_blk->preds_.empty() && stmt->is_entry_)
          stmt->labels_.push_back(total_blk_cnt + b_blk->blk_id_);
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