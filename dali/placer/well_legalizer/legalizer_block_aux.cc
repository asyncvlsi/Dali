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
#include "legalizer_block_aux.h"

namespace dali {

LegalizerBlockAux::LegalizerBlockAux(Block* blk_ptr) : BlockAux(blk_ptr) {
  DaliExpects(blk_ptr->TypePtr()->HasWellInfo(), "A block has no wellptr?");
  int region_cnt = blk_ptr->TypePtr()->RegionCount();
  sub_locs_.resize(region_cnt, blk_ptr->LLX());
  weights_.resize(region_cnt, 1.0);
  average_loc_ = blk_ptr->LLX();
}

void LegalizerBlockAux::StoreCurLocAsInitLoc() {
  init_loc_.x = blk_ptr_->LLX();
  init_loc_.y = blk_ptr_->LLY();
}

void LegalizerBlockAux::StoreCurLocAsGreedyLoc() {
  greedy_loc_.x = blk_ptr_->LLX();
  greedy_loc_.y = blk_ptr_->LLY();
}

void LegalizerBlockAux::StoreCurLocAsQPLoc() {
  qp_loc_.x = blk_ptr_->LLX();
  qp_loc_.y = blk_ptr_->LLY();
}

void LegalizerBlockAux::StoreCurLocAsConsLoc() {
  cons_loc_.x = blk_ptr_->LLX();
  cons_loc_.y = blk_ptr_->LLY();
}

void LegalizerBlockAux::RecoverInitLoc() {
  blk_ptr_->SetLLX(init_loc_.x);
  blk_ptr_->SetLLY(init_loc_.y);
}

void LegalizerBlockAux::RecoverGreedyLoc() {
  blk_ptr_->SetLLX(greedy_loc_.x);
  blk_ptr_->SetLLY(greedy_loc_.y);
}

void LegalizerBlockAux::RecoverQPLoc() {
  blk_ptr_->SetLLX(qp_loc_.x);
  blk_ptr_->SetLLY(qp_loc_.y);
}

void LegalizerBlockAux::RecoverConsLoc() {
  blk_ptr_->SetLLX(cons_loc_.x);
  blk_ptr_->SetLLY(cons_loc_.y);
}

void LegalizerBlockAux::RecoverInitLocX() { blk_ptr_->SetLLX(init_loc_.x); }

void LegalizerBlockAux::RecoverGreedyLocX() { blk_ptr_->SetLLX(greedy_loc_.x); }

void LegalizerBlockAux::RecoverQPLocX() { blk_ptr_->SetLLX(qp_loc_.x); }

void LegalizerBlockAux::RecoverConsLocX() { blk_ptr_->SetLLX(cons_loc_.x); }

void LegalizerBlockAux::SetSubCellLoc(int id, double loc, double weight) {
  sub_locs_[id] = loc;
  weights_[id] = weight;
}

void LegalizerBlockAux::ComputeAverageLoc() {
  size_t sz = sub_locs_.size();
  double sum_weight_loc = 0;
  double sum_weight = 0;
  /*double max_weight = 0;
  for (size_t i = 0; i < sz; ++i) {
    max_weight = std::max(max_weight, weights_[i]);
  }
  for (size_t i = 0; i < sz; ++i) {
    weights_[i] /= max_weight;
  }*/
  for (size_t i = 0; i < sz; ++i) {
    sum_weight_loc += weights_[i] * sub_locs_[i];
    sum_weight += weights_[i];
  }
  average_loc_ = sum_weight_loc / sum_weight;
}

std::vector<double>& LegalizerBlockAux::SubLocs() { return sub_locs_; }

double LegalizerBlockAux::AverageLoc() { return average_loc_; }

double2d LegalizerBlockAux::InitLoc() { return init_loc_; }

double2d LegalizerBlockAux::GreedyLoc() { return greedy_loc_; }

double2d LegalizerBlockAux::QPLoc() { return qp_loc_; }

double2d LegalizerBlockAux::ConsLoc() { return cons_loc_; }

}  // namespace dali