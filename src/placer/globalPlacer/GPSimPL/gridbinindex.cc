//
// Created by Yihang Yang on 2019-08-07.
//

#include "gridbinindex.h"

GridBinIndex::GridBinIndex() {
  x = 0;
  y = 0;
}

GridBinIndex::GridBinIndex(int x0, int y0) {
  x = x0;
  y = y0;
}

bool GridBinIndex::operator<(const GridBinIndex &rhs) const{
  bool is_less = (x < rhs.x) || ((x == rhs.x) && (y < rhs.y));
  return is_less;
}

bool GridBinIndex::operator>(const GridBinIndex &rhs) const{
  bool is_great = (x > rhs.x) || ((x == rhs.x) && (y > rhs.y));
  return is_great;
}

bool GridBinIndex::operator==(const GridBinIndex &rhs) const{
  return ((x == rhs.x) && (y == rhs.y));
}