/*******************************************************************************
 *
 * Copyright (c) 2022 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#include "lgblkaux.h"

namespace dali {

void LgBlkAux::StoreCurLocAsInitLoc() {
  init_loc_.x = blk_ptr_->LLX();
  init_loc_.y = blk_ptr_->LLY();
}

void LgBlkAux::StoreCurLocAsGreedyLoc() {
  greedy_loc_.x = blk_ptr_->LLX();
  greedy_loc_.y = blk_ptr_->LLY();
}

void LgBlkAux::StoreCurLocAsQPLoc() {
  qp_loc_.x = blk_ptr_->LLX();
  qp_loc_.y = blk_ptr_->LLY();
}

void LgBlkAux::StoreCurLocAsConsLoc() {
  cons_loc_.x = blk_ptr_->LLX();
  cons_loc_.y = blk_ptr_->LLY();
}

void LgBlkAux::StoreCurLoc(size_t index) {
  DaliExpects(index < cached_locs_.size(), "Out of bound");
  cached_locs_[index].x = blk_ptr_->LLX();
  cached_locs_[index].y = blk_ptr_->LLY();
}

void LgBlkAux::AllocateCacheLocs(size_t sz) {
  double2d tmp(0,0);
  cached_locs_.resize(sz, tmp);
}

void LgBlkAux::RecoverInitLoc() {
  blk_ptr_->SetLLX(init_loc_.x);
  blk_ptr_->SetLLY(init_loc_.y);
}

void LgBlkAux::RecoverGreedyLoc() {
  blk_ptr_->SetLLX(greedy_loc_.x);
  blk_ptr_->SetLLY(greedy_loc_.y);
}

void LgBlkAux::RecoverQPLoc() {
  blk_ptr_->SetLLX(qp_loc_.x);
  blk_ptr_->SetLLY(qp_loc_.y);
}

void LgBlkAux::RecoverConsLoc() {
  blk_ptr_->SetLLX(cons_loc_.x);
  blk_ptr_->SetLLY(cons_loc_.y);
}

void LgBlkAux::RecoverLoc(size_t index) {
  DaliExpects(index < cached_locs_.size(), "Out of bound");
  blk_ptr_->SetLLX(cached_locs_[index].x);
  blk_ptr_->SetLLY(cached_locs_[index].y);
}

double2d LgBlkAux::InitLoc() {
  return init_loc_;
}

double2d LgBlkAux::GreedyLoc() {
  return greedy_loc_;
}

double2d LgBlkAux::QPLoc() {
  return qp_loc_;
}

double2d LgBlkAux::ConsLoc() {
  return cons_loc_;
}

std::vector<double2d> &LgBlkAux::CachedLocs() {
  return cached_locs_;
}

}