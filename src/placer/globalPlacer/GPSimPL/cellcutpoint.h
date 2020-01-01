//
// Created by Yihang Yang on 2019-08-07.
//

#ifndef HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_CELLCUTPOINT_H_
#define HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_CELLCUTPOINT_H_

#include <iostream>

class CellCutPoint {
 public:
  CellCutPoint() : x(0), y(0) {}
  CellCutPoint(double x0, double y0) : x(x0), y(y0) {}
  double x;
  double y;
  void init() {
    x = 0;
    y = 0;
  };
  bool operator<(const CellCutPoint &rhs) const {
    return (x < rhs.x) || ((x == rhs.x) && (y < rhs.y));
  }
  bool operator>(const CellCutPoint &rhs) const {
    return (x > rhs.x) || ((x == rhs.x) && (y > rhs.y));
  }
  bool operator==(const CellCutPoint &rhs) const {
    return (x == rhs.x) && (y == rhs.y);
  }
};

#endif //HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_CELLCUTPOINT_H_
