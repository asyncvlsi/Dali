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
#ifndef DALI_PLACER_WELLLEGALIZER_BLOCKSEGMENT_H_
#define DALI_PLACER_WELLLEGALIZER_BLOCKSEGMENT_H_

#include "dali/circuit/block.h"

namespace dali {

struct BlockSegment {
 private:
  int lx_;
  int width_;
 public:
  BlockSegment(Block *blk_ptr, int loc) : lx_(loc), width_(blk_ptr->Width()) {
    blk_ptrs.push_back(blk_ptr);
    initial_loc.push_back(loc);
  }
  std::vector<Block *> blk_ptrs;
  std::vector<double> initial_loc;

  int LX() const { return lx_; }
  int UX() const { return lx_ + width_; }
  int Width() const { return width_; }

  bool IsNotOnLeft(BlockSegment &sc) const {
    return sc.LX() < UX();
  }
  void Merge(BlockSegment &sc, int lower_bound, int upper_bound);
  void UpdateBlockLocation();

  void Report() const;
};

}

#endif //DALI_PLACER_WELLLEGALIZER_BLOCKSEGMENT_H_
