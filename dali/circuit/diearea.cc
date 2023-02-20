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
#include "diearea.h"

namespace dali {

bool IsPointInDieArea(int2d point, std::vector<int2d> &rectilinear_die_area) {
  return false;
}

/****
 * This function takes rectilinear die area in the unit of manufacturing grid,
 * and converts it to a new rectilinear die area in the unit of grid values.
 * The new rectilinear die area is inside the raw die area if there are any
 * off-grid boundaries.
 *
 * @param raw_rectilinear_die_area
 */
void DieArea::SetRawRectilinearDieArea(
    std::vector<int2d> &raw_rectilinear_die_area
) {
  rectilinear_die_area_ = raw_rectilinear_die_area;
  MaybeExpandTwoPointsToFour();

  // basic sanity checks
  CheckRectilinearLines();
  CheckIntersectingLines();

  ShrinkOffGridDieArea();
  ConvertToGridValueUnit();

  current_recursion_ = 0;
  CheckAndRemoveRedundantPoints();

  DetectMinimumBoundingBox();
  CreatePlacementBlockages();
}

void DieArea::MaybeExpandTwoPointsToFour() {
  size_t num_points = rectilinear_die_area_.size();
  DaliExpects(num_points > 1, "Only one point to specify die area?");
  DaliExpects(
      num_points != 3, "Not sure how to handle three points as the die area"
  );

  if (num_points > 2) {
    return;
  }

  int x0 = rectilinear_die_area_[0].x;
  int y0 = rectilinear_die_area_[0].y;
  int x1 = rectilinear_die_area_[1].x;
  int y1 = rectilinear_die_area_[1].y;

  int min_x = std::min(x0, x1);
  int max_x = std::max(x0, x1);
  int min_y = std::min(y0, y1);
  int max_y = std::max(y0, y1);

  rectilinear_die_area_.clear();
  rectilinear_die_area_.emplace_back(min_x, min_y);
  rectilinear_die_area_.emplace_back(min_x, max_y);
  rectilinear_die_area_.emplace_back(max_x, max_y);
  rectilinear_die_area_.emplace_back(max_x, min_y);
}

/****
 * Every pair of adjacent points should share either the same x or y coordinate
 */
void DieArea::CheckRectilinearLines() {
  size_t num_points = rectilinear_die_area_.size();
  for (size_t i = 0; i < num_points; ++i) {
    // Modulo makes this vector a "circle"
    size_t first_index = i;
    size_t second_index = (i + 1) % num_points;

    int2d first_point = rectilinear_die_area_[first_index];
    int2d second_point = rectilinear_die_area_[second_index];

    // these two points should share either x or y coordinate
    bool is_same_x_coordinate = (first_point.x == second_point.x);
    bool is_same_y_coordinate = (first_point.y == second_point.y);
    DaliExpects(
        is_same_x_coordinate != is_same_y_coordinate,
        "Die area contains illegal non-rectilinear points "
            << first_point << second_point
    );
  }
}

void DieArea::CheckIntersectingLines() {
  std::vector<std::pair<int2d, int2d>> horizontal_lines;
  std::vector<std::pair<int2d, int2d>> vertical_lines;
  size_t num_points = rectilinear_die_area_.size();
  for (size_t i = 0; i < num_points; ++i) {
    // Modulo makes this vector a "circle"
    size_t first_index = i;
    size_t second_index = (i + 1) % num_points;

    int2d first_point = rectilinear_die_area_[first_index];
    int2d second_point = rectilinear_die_area_[second_index];

    if (first_point.x == second_point.x) {
      vertical_lines.emplace_back(first_point, second_point);
    }
    if (first_point.y == second_point.y) {
      horizontal_lines.emplace_back(first_point, second_point);
    }
  }

  // check if two horizontal lines share the same segment

  // check if two vertical lines share the same segment

  // check if a horizontal line intersect with a vertical line
}


void DieArea::ShrinkOffGridDieArea() {

}

void DieArea::ConvertToGridValueUnit() {

}

/****
 * Remove redundant points when more than three consecutive points are in the
 * same line.
 */
void DieArea::CheckAndRemoveRedundantPoints() {
  current_recursion_ += 1;
  DaliExpects(
      current_recursion_ < recursion_limit_,
      "Too many redundant points? Something wrong in the code?"
  );

  size_t num_points = rectilinear_die_area_.size();
  std::vector redundancy(num_points, false);
  for (size_t i = 0; i < num_points; ++i) {
    // Modulo makes this vector a "circle"
    size_t first_index = i;
    size_t second_index = (i + 1) % num_points;
    size_t third_index = (i + 2) % num_points;

    int2d first_point = rectilinear_die_area_[first_index];
    int2d second_point = rectilinear_die_area_[second_index];
    int2d third_point = rectilinear_die_area_[third_index];
    // if these three points share the same x or y coordinate, the middle one
    // is redundant and can be removed
    if ((first_point.x == second_point.x) &&
        (second_point.x == third_point.x)) {
      redundancy[second_index] = true;
    }
    if ((first_point.y == second_point.y) &&
        (second_point.y == third_point.y)) {
      redundancy[second_index] = true;
    }
  }

  // remove redundant points
  std::vector<int2d> new_rectilinear_die_area_;
  for (size_t i = 0; i < num_points; ++i) {
    if (!redundancy[i]) {
      new_rectilinear_die_area_.emplace_back(
          rectilinear_die_area_[i].x,
          rectilinear_die_area_[i].y
      );
    }
  }

  // now we have a new die area with a shorter length
  rectilinear_die_area_.swap(new_rectilinear_die_area_);

  // if there is redundancy, we may need to check again
  if (new_rectilinear_die_area_.size() != num_points) {
    CheckAndRemoveRedundantPoints();
  }
}

void DieArea::DetectMinimumBoundingBox() {

}

void DieArea::CreatePlacementBlockages() {

}

} // dali
