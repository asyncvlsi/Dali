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
#ifndef DALI_PLACER_GLOBAL_PLACER_GRID_BIN_H_
#define DALI_PLACER_GLOBAL_PLACER_GRID_BIN_H_

#include <vector>

#include "dali/circuit/block.h"
#include "grid_bin_index.h"

namespace dali {

class GridBin {
 public:
  GridBin();
  GridBinIndex index;
  int bottom;
  int top;
  int left;
  int right;
  unsigned long long white_space;
  unsigned long long cell_area;
  double filling_rate;
  bool all_terminal;
  // a grid bin is over-filled, if filling rate is larger than the target, or cells locate on terminals
  bool over_fill;
  bool cluster_visited;
  bool global_placed;
  std::vector<Block *> cell_list;
  std::vector<Block *> fixed_blocks;
  std::vector<GridBinIndex> adjacent_bin_index;

  int LLX() { return left; }
  int LLY() { return bottom; }
  int URX() { return right; }
  int URY() { return top; }
  int Height() { return top - bottom; }
  int Width() { return right - left; }
  unsigned long long Area() {
    return (unsigned long long) (top - bottom)
        * (unsigned long long) (right - left);
  }
  bool IsAllFixedBlk() { return all_terminal; }
  bool OverFill() { return over_fill; }
  void create_adjacent_bin_list(int grid_cnt_x, int grid_cnt_y);
  void Report();
};

}

#endif //DALI_PLACER_GLOBAL_PLACER_GRID_BIN_H_
