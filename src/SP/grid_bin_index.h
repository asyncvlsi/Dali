//
// Created by yihan on 7/11/2019.
//

#ifndef HPCC_GRID_BIN_INDEX_H
#define HPCC_GRID_BIN_INDEX_H

#include <iostream>
#include <set>

class grid_bin_index {
public:
  grid_bin_index();
  grid_bin_index(size_t x0, size_t y0);
  size_t x;
  size_t y;
  void init() {x = 0; y = 0;};
  bool operator <(const grid_bin_index &rhs) const;
  bool operator >(const grid_bin_index &rhs) const;
  bool operator ==(const grid_bin_index &rhs) const;
  friend std::ostream& operator<<(std::ostream& os, const grid_bin_index &index)
  {
    os << "(" << index.x << ", " << index.y << ")";
    return os;
  }
};

struct GridBinCluster {
  double total_cell_area = 0;
  double total_white_space = 0;
  std::set< grid_bin_index > grid_bin_index_set;
  explicit GridBinCluster(int initCellArea=0, int initWhiteSpace=0): total_cell_area(initCellArea), total_white_space(initWhiteSpace){}
};



#endif //HPCC_GRID_BIN_INDEX_H
