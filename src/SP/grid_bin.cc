//
// Created by yihan on 7/11/2019.
//

#include "grid_bin.h"

grid_bin::grid_bin() {
  index.init();
  bottom =0;
  top = 0;
  left = 0;
  right = 0;
  area = 0;
  white_space = 0;
  cell_area = 0;
  filling_rate = 0;
  all_terminal = false;
  over_fill = false;
  cluster_visited = false;
  global_placed = false;
}

void grid_bin::create_adjacent_bin_list(int GRID_NUM) {
  adjacent_bin_index.clear();
  grid_bin_index tmp_index;
  if (index.x > 0) {
    tmp_index.x = index.x - 1;
    tmp_index.y = index.y;
    adjacent_bin_index.push_back(tmp_index);
  }
  if (index.x < (size_t)GRID_NUM-1) {
    tmp_index.x = index.x + 1;
    tmp_index.y = index.y;
    adjacent_bin_index.push_back(tmp_index);
  }
  if (index.y > 0) {
    tmp_index.x = index.x;
    tmp_index.y = index.y - 1;
    adjacent_bin_index.push_back(tmp_index);
  }
  if (index.y < (size_t)GRID_NUM-1) {
    tmp_index.x = index.x;
    tmp_index.y = index.y + 1;
    adjacent_bin_index.push_back(tmp_index);
  }
}

