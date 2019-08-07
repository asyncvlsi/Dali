//
// Created by Yihang Yang on 2019-08-07.
//

#include "cellcutpoint.h"


CellCutPoint::CellCutPoint() {
  x = 0;
  y = 0;
}

CellCutPoint::CellCutPoint(double x0, double y0) {
  x = x0;
  y = y0;
}

bool CellCutPoint::operator<(const CellCutPoint &rhs) const{
  bool is_less = (x < rhs.x) || ((x == rhs.x) && (y < rhs.y));
  return is_less;
}

bool CellCutPoint::operator>(const CellCutPoint &rhs) const{
  bool is_great = (x > rhs.x) || ((x == rhs.x) && (y > rhs.y));
  return is_great;
}

bool CellCutPoint::operator==(const CellCutPoint &rhs) const{
  return (x == rhs.x) && (y == rhs.y);
}
