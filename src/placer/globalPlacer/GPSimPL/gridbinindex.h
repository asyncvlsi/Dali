//
// Created by Yihang Yang on 2019-08-07.
//

#ifndef HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_GRIDBININDEX_H_
#define HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_GRIDBININDEX_H_

#include <iostream>
#include <set>

class GridBinIndex {
 public:
  GridBinIndex();
  GridBinIndex(int x0, int y0);
  int x;
  int y;
  void init() {x = 0; y = 0;};
  bool operator <(const GridBinIndex &rhs) const;
  bool operator >(const GridBinIndex &rhs) const;
  bool operator ==(const GridBinIndex &rhs) const;
  friend std::ostream& operator<<(std::ostream& os, const GridBinIndex &index) {
    os << "(" << index.x << ", " << index.y << ")";
    return os;
  }
};

class GridBinCluster {
 public:
  GridBinCluster();
  long int total_cell_area;
  long int total_white_space;
  std::set< GridBinIndex > grid_bin_index_set;
};

#endif //HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_GRIDBININDEX_H_
