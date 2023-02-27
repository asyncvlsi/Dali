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
  // TODO: to be implemented
  return false;
}

void CheckCommonSegment(
    std::pair<int2d, int2d> &seg_0,
    std::pair<int2d, int2d> &seg_1
) {
  bool is_horizontal_0 = (seg_0.first.y == seg_0.second.y);
  bool is_horizontal_1 = (seg_1.first.y == seg_1.second.y);

  if (is_horizontal_0 && is_horizontal_1) {
    DaliExpects(
        (seg_0.first.y != seg_1.first.y),
        "Die area contains intersecting lines\n"
            << "line: " << seg_0.first << " " << seg_0.second << "\n"
            << "line: " << seg_1.first << " " << seg_1.second
    );
  } else if ((!is_horizontal_0) && (!is_horizontal_1)) {
    DaliExpects(
        (seg_0.first.x != seg_1.first.x),
        "Die area contains intersecting lines\n"
            << "line: " << seg_0.first << " " << seg_0.second << "\n"
            << "line: " << seg_1.first << " " << seg_1.second
    );
  } else if (is_horizontal_0 && (!is_horizontal_1)) {
    int x_1 = seg_1.first.x;
    int low_0 = std::min(seg_0.first.x, seg_0.second.x);
    int high_0 = std::max(seg_0.first.x, seg_0.second.x);
    DaliExpects(
        (low_0 >= x_1) || (x_1 >= high_0),
        "Die area contains intersecting lines\n"
            << "line: " << seg_0.first << " " << seg_0.second << "\n"
            << "line: " << seg_1.first << " " << seg_1.second
    );
  } else { // !is_horizontal_0 && is_horizontal_1
    int x_0 = seg_0.first.x;
    int low_1 = std::min(seg_1.first.x, seg_1.second.x);
    int high_1 = std::max(seg_1.first.x, seg_1.second.x);
    DaliExpects(
        (low_1 >= x_0) || (x_0 >= high_1),
        "Die area contains intersecting lines\n"
            << "line: " << seg_0.first << " " << seg_0.second << "\n"
            << "line: " << seg_1.first << " " << seg_1.second
    );
  }
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

  // basic sanity checks before converting to grid value unit
  CheckRectilinearLines();
  CheckIntersectingLines();
  CheckAndRemoveRedundantPoints();

  // convert to grid value unit
  ShrinkOffGridDieArea();
  ConvertToGridValueUnit();

  // basic sanity checks after converting to grid value unit
  CheckRectilinearLines();
  CheckIntersectingLines();
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
            << first_point << " " << second_point
    );
  }
}

void DieArea::CheckIntersectingLines() {
  // separate horizontal lines and vertical lines
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
  size_t num_horizontal_lines = horizontal_lines.size();
  for (size_t i = 0; i < num_horizontal_lines - 1; ++i) {
    for (size_t j = i + 1; j < num_horizontal_lines; ++j) {
      CheckCommonSegment(horizontal_lines[i], horizontal_lines[j]);
    }
  }

  // check if two vertical lines share the same segment
  size_t num_vertical_lines = vertical_lines.size();
  for (size_t i = 0; i < num_vertical_lines - 1; ++i) {
    for (size_t j = i + 1; j < num_vertical_lines; ++j) {
      CheckCommonSegment(vertical_lines[i], vertical_lines[j]);
    }
  }

  // check if a horizontal line intersect with a vertical line
  for (size_t i = 0; i < num_horizontal_lines; ++i) {
    for (size_t j = 0; j < num_vertical_lines; ++j) {
      CheckCommonSegment(horizontal_lines[i], vertical_lines[j]);
    }
  }
}

bool DieArea::IsPointOffGrid(int2d point) {
  // TODO: to be implemented
  return false;
}

int2d DieArea::GetNearestOnGridInDieAreaPoint(int2d point) {
  // TODO: to be implemented
  return int2d{0, 0};
}

void DieArea::ShrinkOffGridDieArea() {
  size_t num_points = rectilinear_die_area_.size();
  for (size_t i = 0; i < num_points; ++i) {
    auto &point = rectilinear_die_area_[i];
    if (IsPointOffGrid(point)) {
      auto new_location = GetNearestOnGridInDieAreaPoint(point);
      rectilinear_die_area_[i] = new_location;
    }
  }
}

void DieArea::ConvertToGridValueUnit() {
  for (auto &point: rectilinear_die_area_) {
    point.x = point.x / distance_scale_factor_x_;
    point.y = point.y / distance_scale_factor_y_;
  }
}

void DieArea::CheckAndRemoveRedundantPoints() {
  current_recursion_ = 0;
  CheckAndRemoveRedundantPointsImp();
}

/****
 * Remove redundant points when more than three consecutive points are in the
 * same line.
 */
void DieArea::CheckAndRemoveRedundantPointsImp() {
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
    CheckAndRemoveRedundantPointsImp();
  }
}

void DieArea::DetectMinimumBoundingBox() {

}

void DieArea::CreatePlacementBlockages() {

}

} // dali
