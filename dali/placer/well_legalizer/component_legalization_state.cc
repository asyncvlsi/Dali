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
#include "component_legalization_state.h"

namespace dali {

ComponentLegalizationState::ComponentLegalizationState(Component* component_ptr)
    : ComponentAux(component_ptr) {
  DaliExpects(component_ptr->MacroPtr()->HasWellInfo(),
              "A component has no wellptr?");
  int region_cnt = component_ptr->MacroPtr()->RegionCount();
  sub_locs_.resize(region_cnt, component_ptr->LLX());
  weights_.resize(region_cnt, 1.0);
  average_loc_ = component_ptr->LLX();
}

void ComponentLegalizationState::StoreCurLocAsInitLoc() {
  init_loc_.x = component_ptr_->LLX();
  init_loc_.y = component_ptr_->LLY();
}

void ComponentLegalizationState::StoreCurLocAsGreedyLoc() {
  greedy_loc_.x = component_ptr_->LLX();
  greedy_loc_.y = component_ptr_->LLY();
}

void ComponentLegalizationState::StoreCurLocAsQPLoc() {
  qp_loc_.x = component_ptr_->LLX();
  qp_loc_.y = component_ptr_->LLY();
}

void ComponentLegalizationState::StoreCurLocAsConsLoc() {
  cons_loc_.x = component_ptr_->LLX();
  cons_loc_.y = component_ptr_->LLY();
}

void ComponentLegalizationState::RecoverInitLoc() {
  component_ptr_->SetLLX(init_loc_.x);
  component_ptr_->SetLLY(init_loc_.y);
}

void ComponentLegalizationState::RecoverGreedyLoc() {
  component_ptr_->SetLLX(greedy_loc_.x);
  component_ptr_->SetLLY(greedy_loc_.y);
}

void ComponentLegalizationState::RecoverQPLoc() {
  component_ptr_->SetLLX(qp_loc_.x);
  component_ptr_->SetLLY(qp_loc_.y);
}

void ComponentLegalizationState::RecoverConsLoc() {
  component_ptr_->SetLLX(cons_loc_.x);
  component_ptr_->SetLLY(cons_loc_.y);
}

void ComponentLegalizationState::RecoverInitLocX() {
  component_ptr_->SetLLX(init_loc_.x);
}

void ComponentLegalizationState::RecoverGreedyLocX() {
  component_ptr_->SetLLX(greedy_loc_.x);
}

void ComponentLegalizationState::RecoverQPLocX() {
  component_ptr_->SetLLX(qp_loc_.x);
}

void ComponentLegalizationState::RecoverConsLocX() {
  component_ptr_->SetLLX(cons_loc_.x);
}

void ComponentLegalizationState::SetSubCellLoc(int id, double loc,
                                               double weight) {
  sub_locs_[id] = loc;
  weights_[id] = weight;
}

void ComponentLegalizationState::ComputeAverageLoc() {
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

std::vector<double>& ComponentLegalizationState::SubLocs() { return sub_locs_; }

double ComponentLegalizationState::AverageLoc() const { return average_loc_; }

double2d ComponentLegalizationState::InitLoc() const { return init_loc_; }

double2d ComponentLegalizationState::GreedyLoc() const { return greedy_loc_; }

double2d ComponentLegalizationState::QPLoc() const { return qp_loc_; }

double2d ComponentLegalizationState::ConsLoc() const { return cons_loc_; }

}  // namespace dali
