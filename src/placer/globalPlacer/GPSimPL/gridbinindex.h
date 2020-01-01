//
// Created by Yihang Yang on 2019-08-07.
//

#ifndef HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_GRIDBININDEX_H_
#define HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_GRIDBININDEX_H_

#include <iostream>
#include <set>

struct GridBinIndex {
 public:
  GridBinIndex() : x(0), y(0) {}
  GridBinIndex(int x0, int y0) : x(x0), y(y0) {}
  int x;
  int y;
  void init() {
    x = 0;
    y = 0;
  };
  bool operator<(const GridBinIndex &rhs) const {
    bool is_less = (x < rhs.x) || ((x == rhs.x) && (y < rhs.y));
    return is_less;
  }
  bool operator>(const GridBinIndex &rhs) const {
    bool is_great = (x > rhs.x) || ((x == rhs.x) && (y > rhs.y));
    return is_great;
  }
  bool operator==(const GridBinIndex &rhs) const {
    return ((x == rhs.x) && (y == rhs.y));
  }
};

struct GridBinCluster {
 public:
  GridBinCluster() : total_cell_area(0), total_white_space(0) {}
  unsigned long int total_cell_area;
  unsigned long int total_white_space;
  std::set<GridBinIndex> bin_set;
  bool operator<(const GridBinCluster &rhs) const {
    return (total_cell_area < rhs.total_cell_area);
  }
  bool operator>(const GridBinCluster &rhs) const {
    return (total_cell_area > rhs.total_cell_area);
  }
};

#endif //HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_GRIDBININDEX_H_
