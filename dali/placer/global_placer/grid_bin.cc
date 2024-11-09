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

#include "grid_bin.h"

#include "dali/common/logging.h"

namespace dali {

GridBin::GridBin() {
  index.init();
  bottom = 0;
  top = 0;
  left = 0;
  right = 0;
  white_space = 0;
  cell_area = 0;
  filling_rate = 0;
  all_terminal = false;
  over_fill = false;
  cluster_visited = false;
  global_placed = false;
}

void GridBin::create_adjacent_bin_list(int grid_cnt_x, int grid_cnt_y) {
  adjacent_bin_index.clear();
  GridBinIndex tmp_index;
  if (index.x > 0) {
    tmp_index.x = index.x - 1;
    tmp_index.y = index.y;
    adjacent_bin_index.push_back(tmp_index);
  }
  if (index.x < grid_cnt_x - 1) {
    tmp_index.x = index.x + 1;
    tmp_index.y = index.y;
    adjacent_bin_index.push_back(tmp_index);
  }
  if (index.y > 0) {
    tmp_index.x = index.x;
    tmp_index.y = index.y - 1;
    adjacent_bin_index.push_back(tmp_index);
  }
  if (index.y < grid_cnt_y - 1) {
    tmp_index.x = index.x;
    tmp_index.y = index.y + 1;
    adjacent_bin_index.push_back(tmp_index);
  }
  /*for (auto &neighbor: adjacent_bin_index) {
    BOOST_LOG_TRIVIAL(info)   << neighbor.x << "  " << neighbor.y << ",  ";
  }
  BOOST_LOG_TRIVIAL(info)   << "\n";*/
}

void GridBin::Report() {
  BOOST_LOG_TRIVIAL(info) << "  block count: " << cell_list.size() << "\n"
                          << "  block area:  " << cell_area << "\n"
                          << "  filling rate: " << filling_rate << "\n"
                          << "  white space: " << white_space << "\n"
                          << "  over fill:   " << over_fill << "\n";
}

}  // namespace dali
