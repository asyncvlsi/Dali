//
// Created by Yihang Yang on 2019-08-07.
//

#ifndef DALI_SRC_PLACER_GLOBALPLACER_GPSIMPL_GRIDBIN_H_
#define DALI_SRC_PLACER_GLOBALPLACER_GPSIMPL_GRIDBIN_H_

#include <vector>

#include "gridbinindex.h"

class GridBin {
 public:
  GridBin();
  GridBinIndex index;
  int bottom;
  int top;
  int left;
  int right;
  unsigned long int white_space;
  unsigned long int cell_area;
  double filling_rate;
  bool all_terminal;
  bool over_fill; // a grid bin is over-filled, if filling rate is larger than the target, or cells locate on terminals
  bool cluster_visited;
  bool global_placed;
  std::vector<int> cell_list;
  std::vector<int> terminal_list;
  std::vector<GridBinIndex> adjacent_bin_index;


  int LLX() { return left; }
  int LLY() { return bottom; }
  int URX() { return right; }
  int URY() { return top; }
  int Height() { return top - bottom; }
  int Width() { return right - left; }
  unsigned long int Area() { return (unsigned long int) (top - bottom) * (unsigned long int) (right - left); }
  bool IsAllFixedBlk() { return all_terminal; }
  bool OverFill() { return over_fill; }
  void create_adjacent_bin_list(int grid_cnt_x, int grid_cnt_y);
  void Report();
};

#endif //DALI_SRC_PLACER_GLOBALPLACER_GPSIMPL_GRIDBIN_H_
