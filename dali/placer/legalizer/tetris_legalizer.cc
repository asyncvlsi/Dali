/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
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
#include "tetris_legalizer.h"

#include <algorithm>

#include "dali/common/misc.h"

namespace dali {

TetrisLegalizer::TetrisLegalizer()
    : Placer(), max_iteration_(5), current_iteration_(0), flipped_(false) {}

void TetrisLegalizer::InitLegalizer() {
  ComponentInitialLocation init_pair(nullptr, 0, 0);
  index_loc_list_.resize(ckt_ptr_->Components().size(), init_pair);
}

void TetrisLegalizer::SetMaxItr(int max_iteration) {
  DaliExpects(max_iteration > 0,
              "Invalid max_iteration value, value must be greater than 0");
  max_iteration_ = max_iteration;
}

void TetrisLegalizer::FastShift(int failure_point) {
  /****
   * This method is to FastShiftLeft() the components following the
   * failure_point (included) to reasonable locations in order to keep component
   * orders
   *    1. tetrisSpace.IsSpaceAvail() fails to place a component when the
   * current location of this component is illegal
   *    2. tetrisSpace.FindComponentLoc() fails to place a component when there
   * is no possible legal location on the right hand side of this component, if
   * failure_point is the first component we shift the bounding box of all
   * components to the placement region, the bounding box left and bottom
   * boundaries touch the left and bottom boundaries of placement region, note
   * that the bounding box might be larger than the placement region, but should
   * not be much larger else: we shift the bounding box of the remaining
   * components to the right hand side of the component just placed the bottom
   * boundary of the bounding box will not be changed only the left boundary of
   * the bounding box will be shifted to the right hand side of the component
   * just placed
   * ****/
  std::vector<Component>& components = ckt_ptr_->Components();
  double bounding_left;
  if (failure_point == 0) {
    double bounding_bottom;
    bounding_left = components[0].LLX();
    bounding_bottom = components[0].LLY();
    for (auto& component : components) {
      if (component.LLY() < bounding_bottom) {
        bounding_bottom = component.LLY();
      }
    }
    for (auto& component : components) {
      component.IncreaseX(left_ - bounding_left);
      component.IncreaseY(bottom_ - bounding_bottom);
    }
  } else {
    double init_diff =
        index_loc_list_[failure_point - 1].x - index_loc_list_[failure_point].x;
    Component* failed_component = index_loc_list_[failure_point].component_ptr;
    bounding_left = failed_component->LLX();
    Component* last_placed_component =
        index_loc_list_[failure_point - 1].component_ptr;
    int left_new = (int)std::round(last_placed_component->LLX());
    // LOG(info)   << left_new << "  " << bounding_left << "\n";
    for (size_t i = failure_point; i < index_loc_list_.size(); ++i) {
      Component* component_ptr = index_loc_list_[i].component_ptr;
      component_ptr->IncreaseX(left_new + init_diff - bounding_left);
    }
  }
}

/****
 * flip_axis = (left_ + right_)/2;
 * component_x = component.X();
 * flipped_x = -(component_x - flip_axis) + flip_axis = 2*flip_axis -
 * component_x; flipped_llx = flipped_x - component.Width()/2.0 = 2*flip_axis -
 * (component.X() + component.Width()/2.0) = 2*flip_axis - component.URX() =
 * (left_ + right_) - component.URX() = sum_left_right - component.URX()
 * component.SetLLX(flipped_llx);
 *
 * ****/
void TetrisLegalizer::FlipPlacement() {
  flipped_ = !flipped_;
  int sum_left_right = left_ + right_;
  std::vector<Component>& components = ckt_ptr_->Components();
  for (auto& component : components) {
    component.SetLLX(sum_left_right - component.URX());
  }
}

bool TetrisLegalizer::TetrisLegal() {
  std::vector<Component>& components = ckt_ptr_->Components();
  // 1. move all components into placement region
  /*for (auto &component: components) {
    if (component.LLX() < Left()) {
      component.SetLLX(Left());
    }
    if (component.LLY() < Bottom()) {
      component.SetLLY(Bottom());
    }
    if (component.URX() > Right()) {
      component.SetURX(Right());
    }
    if (component.URY() > Top()) {
      component.SetURY(Top());
    }
  }*/

  // 2. sort components based on their lower Left corners. Further optimization
  // is doable here.

  for (size_t i = 0; i < index_loc_list_.size(); ++i) {
    index_loc_list_[i].component_ptr = &(components[i]);
    index_loc_list_[i].x = components[i].LLX();
    index_loc_list_[i].y = components[i].LLY();
  }
  std::sort(index_loc_list_.begin(), index_loc_list_.end(),
            [](const ComponentInitialLocation& pair0,
               const ComponentInitialLocation& pair1) {
              return (pair0.x < pair1.x) ||
                     ((pair0.x == pair1.x) && (pair0.y < pair1.y));
            });

  /*for (auto &pair: index_loc_list_) {
    LOG(info)   << components[pair.num].LLX() << "\n";
  }*/

  // 3. initialize the data structure to store row usage
  // int maxHeight = GetCircuitRef().MaxComponentHeight();
  int minWidth = ckt_ptr_->MinComponentWidth();
  // int minHeight = GetCircuitRef().MinComponentHeight();

  LOG(info) << "Building Tetris legalizer space" << std::endl;
  TetrisSpace tetrisSpace(RegionLeft(), RegionRight(), RegionBottom(),
                          RegionTop(), 1, minWidth);
  int llx, lly;
  int width, height;
  // int count = 0;
  for (size_t i = 0; i < index_loc_list_.size(); ++i) {
    auto bk_ptr = index_loc_list_[i].component_ptr;
    width = bk_ptr->Width();
    height = bk_ptr->Height();
    /****
     * After "integerization" of the current location from "double" to "int":
     * 1. if the current location is legal, the location of this component don't
     * have to be changed, IsSpaceAvail() will mark the space occupied by this
     * component to be "used", and for sure this space is no more available
     * 2. if the current location is illegal,
     *  FindComponentLoc() will find a legal location for this component, and
     * mark that space used.
     * 3. If FindBlocLoc() fails to find a legal location,
     *  FastShiftLeft() the remaining components to the right hand side of the
     * last placed component, in order to keep component orders FlipPlacement()
     * will flip the placement in the x-direction if current_iteration does not
     * reach the maximum allowed number, then do the legalization in a reverse
     * order
     * ****/
    llx = (int)std::round(bk_ptr->LLX());
    lly = (int)std::round(bk_ptr->LLY());
    bool is_current_loc_legal =
        tetrisSpace.IsSpaceAvail(llx, lly, width, height);
    if (is_current_loc_legal) {
      bk_ptr->SetLoc(llx, lly);
    } else {
      int2d result_loc(0, 0);
      bool is_found =
          tetrisSpace.FindComponentLoc(llx, lly, width, height, result_loc);
      if (is_found) {
        bk_ptr->SetLoc(result_loc.x, result_loc.y);
      } else {
        FastShift(i);
        LOG(info) << "Tetris legalization iteration...\n";
        return false;
      }
    }
    // count++;
  }
  return true;
}

bool TetrisLegalizer::StartPlacement() {
  PrintStartStatement("Tetris legalization");
  InitLegalizer();
  bool is_successful = false;
  for (current_iteration_ = 0; current_iteration_ < max_iteration_;
       ++current_iteration_) {
    // if a legal location is not found, need to reverse the legalization
    // process
    is_successful = TetrisLegal();
    if (!is_successful) {
      FlipPlacement();
    } else {
      break;
    }
  }
  if (flipped_) {
    FlipPlacement();
  }

  PrintEndStatement("Tetris legalization", is_successful);

  return is_successful;
}

}  // namespace dali
