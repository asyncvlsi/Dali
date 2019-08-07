//
// Created by Yihang Yang on 2019-08-07.
//

#ifndef HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_CELLCUTPOINT_H_
#define HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_CELLCUTPOINT_H_

#include <iostream>

class CellCutPoint {
 public:
  CellCutPoint();
  CellCutPoint(double x0, double y0);
  double x;
  double y;
  void init() {x = 0; y = 0;};
  bool operator <(const CellCutPoint &rhs) const;
  bool operator >(const CellCutPoint &rhs) const;
  bool operator ==(const CellCutPoint &rhs) const;
  friend std::ostream& operator<<(std::ostream& os, const CellCutPoint &cut_point) {
    os << "(" << cut_point.x << ", " << cut_point.y << ")";
    return os;
  }
};

#endif //HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_CELLCUTPOINT_H_
