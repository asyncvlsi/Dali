/*******************************************************************************
 *
 * Copyright (c) 2023 Yihang Yang
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
#ifndef DALI_CIRCUIT_DIEAREA_H_
#define DALI_CIRCUIT_DIEAREA_H_

#include "dali/common/misc.h"

namespace dali {

class DieArea {
  friend class Circuit;
  friend class Design;
 public:
  DieArea() = default;

  void SetRawRectilinearDieArea(std::vector<int2d> &raw_rectilinear_die_area);
 private:
  int distance_scale_factor_x_ = 0;
  int distance_scale_factor_y_ = 0;

  int region_left_ = 0; // unit is grid value x
  int region_right_ = 0; // unit is grid value x
  int region_bottom_ = 0; // unit is grid value y
  int region_top_ = 0; // unit is grid value y
  bool die_area_set_ = false;

  std::vector<int2d> rectilinear_die_area_;

  // if left boundary is not on grid, then how much distance should we shift it to make it on grid
  int die_area_offset_x_ = 0; // unit is manufacturing grid
  // if right boundary is still not on grid after shifting, this number is the residual
  int die_area_offset_x_residual_ = 0; // unit is manufacturing grid
  // if bottom boundary is not on grid, then how much distance should we shift it to make it on grid
  int die_area_offset_y_ = 0; // unit is manufacturing grid
  // if top boundary is still not on grid after shifting, this number is the residual
  int die_area_offset_y_residual_ = 0; // unit is manufacturing grid

  int recursion_limit_ = 1000;
  int current_recursion_ = 0;

  void MaybeExpandTwoPointsToFour();
  void CheckRectilinearLines();
  void CheckIntersectingLines();
  bool IsPointOffGrid(int2d point);
  int2d GetNearestOnGridInDieAreaPoint(int2d point);
  void ShrinkOffGridDieArea();
  void ConvertToGridValueUnit();
  void CheckAndRemoveRedundantPoints();
  void CheckAndRemoveRedundantPointsImp();
  void DetectMinimumBoundingBox();
  void CreatePlacementBlockages();
};

} // dali

#endif //DALI_CIRCUIT_DIEAREA_H_
