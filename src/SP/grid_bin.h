//
// Created by yihan on 7/11/2019.
//

#ifndef HPCC_GRID_BIN_H
#define HPCC_GRID_BIN_H

#include <vector>
#include "grid_bin_index.h"

class grid_bin {
public:
  grid_bin();
  grid_bin_index index;
  int bottom;
  int top;
  int left;
  int right;
  int area;
  int white_space;
  int cell_area;
  double filling_rate;
  bool all_terminal;
  bool over_fill; // a grid bin is over-filled, if filling rate is larger than the target, or cells locate on terminals
  bool cluster_visited;
  bool global_placed;
  std::vector< size_t > cell_list;
  std::vector< size_t > terminal_list;
  std::vector< grid_bin_index > adjacent_bin_index;
  int llx() { return left; }
  int lly() { return bottom; }
  int urx() { return right; }
  int ury() { return top; }
  bool is_all_terminal() { return all_terminal; }
  bool is_over_fill() { return over_fill; }
  void create_adjacent_bin_list(int GRID_NUM);
};


#endif //HPCC_GRID_BIN_H
