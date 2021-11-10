/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#ifndef DALI_DALI_PLACER_GLOBALPLACER_CELLCUTPOINT_H_
#define DALI_DALI_PLACER_GLOBALPLACER_CELLCUTPOINT_H_

#include <iostream>

namespace dali {

struct CellCutPoint {
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
  friend std::ostream &operator<<(std::ostream &os, const CellCutPoint &p) {
    os << "(" << p.x << ", " << p.y << ") ";
    return os;
  }
};

}

#endif //DALI_DALI_PLACER_GLOBALPLACER_CELLCUTPOINT_H_
