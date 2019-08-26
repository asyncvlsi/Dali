//
// Created by Yihang Yang on 2019-08-07.
//

#include "gridbin.h"

GridBin::GridBin() {
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

void GridBin::create_adjacent_bin_list(int GRID_NUM) {
  adjacent_bin_index.clear();
  GridBinIndex tmp_index;
  if (index.x > 0) {
    tmp_index.x = index.x - 1;
    tmp_index.y = index.y;
    adjacent_bin_index.push_back(tmp_index);
  }
  if (index.x < GRID_NUM-1) {
    tmp_index.x = index.x + 1;
    tmp_index.y = index.y;
    adjacent_bin_index.push_back(tmp_index);
  }
  if (index.y > 0) {
    tmp_index.x = index.x;
    tmp_index.y = index.y - 1;
    adjacent_bin_index.push_back(tmp_index);
  }
  if (index.y < GRID_NUM-1) {
    tmp_index.x = index.x;
    tmp_index.y = index.y + 1;
    adjacent_bin_index.push_back(tmp_index);
  }
}

void GridBin::Report() {
  std::cout << "  block count: " << cell_list.size() << std::endl;
  std::cout << "  block area:  " << cell_area        << std::endl;
  std::cout << "  filling rate: "<< filling_rate     << std::endl;
  std::cout << "  white space: " << white_space      << std::endl;
  std::cout << "  over fill:   " << over_fill        << std::endl;
}