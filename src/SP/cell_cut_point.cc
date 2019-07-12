//
// Created by yihan on 7/11/2019.
//

#include "cell_cut_point.h"

cell_cut_point::cell_cut_point() {
  x = 0;
  y = 0;
}

cell_cut_point::cell_cut_point(double x0, double y0) {
  x = x0;
  y = y0;
}


bool cell_cut_point::operator<(const cell_cut_point &rhs) const{
  if (x < rhs.x) {
    return true;
  }
  return (x == rhs.x) && (y < rhs.y);
}

bool cell_cut_point::operator>(const cell_cut_point &rhs) const{
  if (x > rhs.x) {
    return true;
  }
  return (x == rhs.x) && (y > rhs.y);
}

bool cell_cut_point::operator==(const cell_cut_point &rhs) const{
  return (x == rhs.x) && (y == rhs.y);
}
