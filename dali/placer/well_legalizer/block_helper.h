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
#ifndef DALI_PLACER_WELL_LEGALIZER_BLOCK_HELPER_H_
#define DALI_PLACER_WELL_LEGALIZER_BLOCK_HELPER_H_

#include "dali/circuit/block.h"

namespace dali {

struct BlockRegion {
  BlockRegion(Block* block_init, int id) : block(block_init), region_id(id) {}
  Block* block = nullptr;
  int region_id = 0;
};

/****
 * @brief A structure containing information of a block for minimizing
 * displacement
 */
struct BlockDisplacementVariable {
  int w;                     // width of this block
  double x_0;                // initial location
  double e;                  // weight of initial location
  double x_a;                // anchor location
  double a;                  // weight of anchor location
  double x;                  // place to store final location
  BlockRegion block_region;  // pointer to the block or dummy block
  double segment_weight_;
  BlockDisplacementVariable(int width, double x_init, double weight = 1.0)
      : w(width),
        x_0(x_init),
        e(weight),
        x_a(0.0),
        a(0.0),
        x(0.0),
        block_region(nullptr, 0),
        segment_weight_(1.0) {}

  int Width() { return w; }
  double InitX() { return x_0; }
  double Weight() { return e; }
  double AnchorX() { return x_a; }
  double AnchorWeight() { return a; };
  double Solution() { return x; }
  double SegmentWeight() { return segment_weight_; }
  BlockRegion Region() { return block_region; }

  void SetAnchor(double anchor, double anchor_weight) {
    x_a = anchor;
    a = anchor_weight;
    double sum_weight_anchor = e * x_0 + a * x_a;
    double sum_weight = e + a;
    e = sum_weight;
    x_0 = sum_weight_anchor / sum_weight;
  }

  void SetWeight(double weight) { e = weight; }
  void SetSolution(double x_new) { x = x_new; }
  void SetClusterWeight(double c_weight) { segment_weight_ = c_weight; }
  void UpdateBlockLocation() {
    if (block_region.block != nullptr) {
      block_region.block->SetLLX(x);
    }
  }

  bool IsMultideckCell() const {
    return block_region.block->TypePtr()->RegionCount() > 1;
  }
  bool TendToRight() {
    const double epsilon = 0.01;
    return Solution() < InitX() - epsilon;
  }
  bool TendToLeft() {
    const double epsilon = 0.01;
    return Solution() > InitX() + epsilon;
  }
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_BLOCK_HELPER_H_
