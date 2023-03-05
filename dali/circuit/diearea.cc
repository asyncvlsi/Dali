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

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "dali/common/helper.h"

namespace dali {

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
 * @param rectilinear_die_area
 */
void DieArea::SetRawRectilinearDieArea(
    std::vector<int2d> &rectilinear_die_area
) {
  rectilinear_die_area_ = rectilinear_die_area;
  MaybeExpandTwoPointsToFour();
  CheckRectilinearLines();
  CheckIntersectingLines();
  CheckAndRemoveRedundantPoints();
  DetectMinimumBoundingBox();
  ShrinkOffGridBoundingBox();
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
  region_left_ = rectilinear_die_area_[0].x;
  region_right_ = region_left_;
  region_bottom_ = rectilinear_die_area_[0].y;
  region_top_ = region_bottom_;

  for (auto &point : rectilinear_die_area_) {
    region_left_ = std::min(region_left_, point.x);
    region_right_ = std::max(region_right_, point.x);
    region_bottom_ = std::min(region_bottom_, point.y);
    region_top_ = std::max(region_top_, point.y);
  }
}

void DieArea::ShrinkOffGridBoundingBox() {
  double f_left = region_left_ / static_cast<double>(distance_scale_factor_x_);
  if (AbsResidual(f_left, 1) > 1e-5) {
    region_left_ = std::ceil(f_left);
    BOOST_LOG_TRIVIAL(info)
      << "left placement boundary is not on placement grid: \n"
      << "  shrink left from " << region_left_ << " to "
      << region_left_ * distance_scale_factor_x_ << "\n";
  } else {
    region_left_ = static_cast<int>(std::round(f_left));
  }

  double
      f_right = region_right_ / static_cast<double>(distance_scale_factor_x_);
  if (AbsResidual(f_right, 1) > 1e-5) {
    region_right_ = std::floor(f_right);
    BOOST_LOG_TRIVIAL(info)
      << "right placement boundary is not on placement grid: \n"
      << "  shrink right from " << region_right_ << " to "
      << region_right_ * distance_scale_factor_x_
      << "\n";
  } else {
    region_right_ = static_cast<int>(std::round(f_right));
  }

  double
      f_bottom = region_bottom_ / static_cast<double>(distance_scale_factor_y_);
  if (AbsResidual(f_bottom, 1) > 1e-5) {
    region_bottom_ = std::ceil(f_bottom);
    BOOST_LOG_TRIVIAL(info)
      << "bottom placement boundary is not on placement grid: \n"
      << "  shrink bottom from " << region_bottom_ << " to "
      << region_bottom_ * distance_scale_factor_y_
      << "\n";
  } else {
    region_bottom_ = static_cast<int>(std::round(f_bottom));
  }

  double f_top = region_top_ / static_cast<double>(distance_scale_factor_y_);
  if (AbsResidual(f_top, 1) > 1e-5) {
    region_top_ = std::floor(f_top);
    BOOST_LOG_TRIVIAL(info)
      << "top placement boundary is not on placement grid: \n"
      << "  shrink top from " << region_top_ << " to "
      << region_top_ * distance_scale_factor_y_ << "\n";
  } else {
    region_top_ = static_cast<int>(std::round(f_top));
  }
}

/****
 * Determine if a point is inside the die area.
 * We will only use this function to examine the center of rectangles from the
 * irregular rectangular grid. This means this point will never be on a
 * polygon edge.
 *
 * If we extend a line rightward to the infinity starting from this point,
 * this line will intersect with odd number of vertical edges if inside the
 * die area; otherwise, outside the die area.
 *
 * @param point: center of a rectangle formed from polygon edges
 * @return whether it is inside the die area
 */
bool DieArea::IsPointInDieArea(double2d point) const {
  int number_of_intersections = 0;

  size_t num_points = rectilinear_die_area_.size();
  for (size_t i = 0; i < num_points; ++i) {
    // Modulo makes this vector a "circle"
    size_t first_index = i;
    size_t second_index = (i + 1) % num_points;
    int2d first_point = rectilinear_die_area_[first_index];
    int2d second_point = rectilinear_die_area_[second_index];
    if (first_point.y == second_point.y) {
      // skip horizontal edges
      continue;
    }
    int x = first_point.x;
    if (x < point.x) {
      // skip vertical edges on the left side of this point
      continue;
    }
    int low_y = std::min(first_point.y, second_point.y);
    int high_y = std::max(first_point.y, second_point.y);
    if ((point.y > low_y) && (point.y < high_y)) {
      ++number_of_intersections;
    }
  }

  return (number_of_intersections % 2 == 1);
}

void DieArea::CreatePlacementBlockages() {
  // get all vertical lines and horizontal lines
  std::unordered_set<int> x_lines;
  std::unordered_set<int> y_lines;
  for (auto &point : rectilinear_die_area_) {
    x_lines.insert(point.x);
    y_lines.insert(point.y);
  }

  // get sorted vertical lines
  std::vector<int> sort_x_lines;
  for (auto &line : x_lines) {
    sort_x_lines.emplace_back(line);
  }
  std::sort(
      sort_x_lines.begin(),
      sort_x_lines.end(),
      [](const int &line_0, const int &line_1) {
        return line_0 < line_1;
      }
  );

  // get sorted horizontal lines
  std::vector<int> sort_y_lines;
  for (auto &line : y_lines) {
    sort_y_lines.emplace_back(line);
  }
  std::sort(
      sort_y_lines.begin(),
      sort_y_lines.end(),
      [](const int &line_0, const int &line_1) {
        return line_0 < line_1;
      }
  );

  // These vertical lines and horizontal lines form an irregular rectangular
  // grid. This grid divides the whole region into many rectangles.
  // Some rectangles are completely inside the die area, while others are
  // completely out of the die area.
  // These out-of-die-area rectangles are placement blockages.
  placement_blockages_.clear();
  for (size_t i = 0; i < sort_x_lines.size() - 1; ++i) {
    for (size_t j = 0; j < sort_y_lines.size() - 1; ++j) {
      int lx = sort_x_lines[i];
      int ux = sort_x_lines[i + 1];
      int ly = sort_y_lines[j];
      int uy = sort_y_lines[j + 1];
      auto center = double2d((lx + ux) / 2.0, (ly + uy) / 2.0);
      if (!IsPointInDieArea(center)) {
        placement_blockages_.emplace_back(lx, ly, ux, uy);
      }
    }
  }
}

} // dali
