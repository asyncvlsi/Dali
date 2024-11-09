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
#include "blocksegment.h"

namespace dali {

void BlockSegment::Merge(BlockSegment &sc, int lower_bound, int upper_bound) {
  int sz = (int)sc.blk_ptrs.size();
  DaliExpects(sz == (int)sc.initial_loc.size(),
              "Block number does not match initial location number");
  for (int i = 0; i < sz; ++i) {
    blk_ptrs.push_back(sc.blk_ptrs[i]);
    initial_loc.push_back(sc.initial_loc[i]);
  }
  width_ += sc.Width();

  std::vector<double> anchor;
  int accumulative_width = 0;
  sz = (int)blk_ptrs.size();
  for (int i = 0; i < sz; ++i) {
    anchor.push_back(initial_loc[i] - accumulative_width);
    accumulative_width += blk_ptrs[i]->Width();
  }
  DaliExpects(width_ == accumulative_width,
              "Something is wrong, width does not match");

  double sum = 0;
  for (auto &num : anchor) {
    sum += num;
  }
  lx_ = (int)std::round(sum / sz);
  if (lx_ < lower_bound) {
    lx_ = lower_bound;
  }
  if (lx_ + width_ > upper_bound) {
    lx_ = upper_bound - width_;
  }
}

void BlockSegment::UpdateBlockLocation() {
  int cur_loc = lx_;
  for (auto &blk : blk_ptrs) {
    blk->SetLLX(cur_loc);
    cur_loc += blk->Width();
  }
}

void BlockSegment::Report() const {
  int sz = (int)blk_ptrs.size();
  for (int i = 0; i < sz; ++i) {
    std::cout << blk_ptrs[i]->Name() << "  " << blk_ptrs[i]->LLX() << "  "
              << blk_ptrs[i]->Width() << "  " << initial_loc[i] << "\n";
  }
}

}  // namespace dali