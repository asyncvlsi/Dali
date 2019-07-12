//
// Created by yihan on 7/11/2019.
//

#ifndef HPCC_CELL_CUT_POINT_H
#define HPCC_CELL_CUT_POINT_H

#include <iostream>

class cell_cut_point {
public:
  cell_cut_point();
  cell_cut_point(double x0, double y0);
  double x;
  double y;
  void init() {x = 0; y = 0;};
  bool operator <(const cell_cut_point &rhs) const;
  bool operator >(const cell_cut_point &rhs) const;
  bool operator ==(const cell_cut_point &rhs) const;
  friend std::ostream& operator<<(std::ostream& os, const cell_cut_point &cut_point)
  {
    os << "(" << cut_point.x << ", " << cut_point.y << ")";
    return os;
  }
};

#endif //HPCC_CELL_CUT_POINT_H
