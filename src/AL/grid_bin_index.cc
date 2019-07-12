//
// Created by yihan on 7/11/2019.
//

#include "grid_bin_index.h"

grid_bin_index::grid_bin_index() {
  x = 0;
  y = 0;
}

grid_bin_index::grid_bin_index(size_t x0, size_t y0) {
  x = x0;
  y = y0;
}

bool grid_bin_index::operator<(const grid_bin_index &rhs) const{
  if (x < rhs.x) {
    return true;
  }
  return (x == rhs.x) && (y < rhs.y);
}

bool grid_bin_index::operator>(const grid_bin_index &rhs) const{
  if (x > rhs.x) {
    return true;
  }
  return (x == rhs.x) && (y > rhs.y);
}

bool grid_bin_index::operator==(const grid_bin_index &rhs) const{
  return (x == rhs.x) && (y == rhs.y);
}