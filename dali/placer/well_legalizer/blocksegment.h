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

/** Horizontal segment of one or more blocks for local legalization. */
struct BlockSegment {
 private:
  int lx_;
  int width_;

 public:
  BlockSegment(Block* blk_ptr, int loc) : lx_(loc), width_(blk_ptr->Width()) {
    blk_ptrs.push_back(blk_ptr);
    initial_loc.push_back(loc);
  }
  std::vector<Block*> blk_ptrs;
  std::vector<double> initial_loc;

  /** Return lower x in Dali grid units. */
  int LX() const { return lx_; }

  /** Return upper x in Dali grid units. */
  int UX() const { return lx_ + width_; }

  /** Return segment width in Dali grid units. */
  int Width() const { return width_; }

  /** Return true when sc overlaps or touches this segment from the left. */
  bool IsNotOnLeft(BlockSegment& sc) const { return sc.LX() < UX(); }

  /** Merge another segment while respecting legal bounds. */
  void Merge(BlockSegment& sc, int lower_bound, int upper_bound);

  /** Write segment locations back to contained blocks. */
  void UpdateBlockLocation();

  /** Log segment state for debugging. */
  void Report() const;
};

}  // namespace dali

#endif  // DALI_PLACER_WELLLEGALIZER_BLOCKSEGMENT_H_
