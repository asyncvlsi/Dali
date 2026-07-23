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

/**
 * @file
 * The recursive bisection that spreads components out of dense regions.
 *
 * A region holding more component area than it has room for is cut in two and
 * its components divided between the halves, in proportion to the whitespace
 * each half actually has rather than to its size -- so blockages and fixed
 * cells push components away rather than trapping them. Cutting continues until
 * a region is small enough or no longer overfull.
 *
 * Cut direction and position are chosen to balance whitespace, with a
 * preference for cutting along a macro edge when one lies close to the balanced
 * point, since a boundary already exists there.
 */

#include "spreading_region.h"

#include <cmath>
#include <cstdlib>
#include <limits>
#include <unordered_set>

#include "dali/common/helper.h"

namespace dali {

static double ComputeFillingRate(unsigned long long component_area,
                                 unsigned long long white_space) {
  if (white_space == 0) {
    return component_area == 0 ? 0.0 : std::numeric_limits<double>::infinity();
  }
  return double(component_area) / double(white_space);
}

SpreadingRegion::SpreadingRegion() {
  all_terminal = false;
  cut_direction_x = false;
  total_component_area = 0;
  total_white_space = 0;
  filling_rate = 0;
  capacity_target_utilization = 1.0;
  total_component_area_low = 0;
  total_component_area_high = 0;
  left = 0;
  right = 0;
  bottom = 0;
  top = 0;
}

void SpreadingRegion::update_all_terminal(
    std::vector<std::vector<GridBin>>& grid_bin_matrix) {
  GridBin* bin;
  for (int x = ll_index.x; x <= ur_index.x; x++) {
    for (int y = ll_index.y; y <= ur_index.y; y++) {
      bin = &grid_bin_matrix[x][y];
      if (!bin->IsAllFixedComponent()) {
        all_terminal = false;
        return;
      }
    }
  }
  all_terminal = true;
}

void SpreadingRegion::UpdateComponentArea() {
  total_component_area = 0;
  for (auto& component_ptr : component_ptrs) {
    total_component_area += component_ptr->Area();
  }
}

void SpreadingRegion::UpdateComponentAreaWhiteSpace(
    std::vector<std::vector<GridBin>>& grid_bin_matrix) {
  total_component_area = 0;
  total_white_space = 0;
  for (int x = ll_index.x; x <= ur_index.x; x++) {
    for (int y = ll_index.y; y <= ur_index.y; y++) {
      total_white_space += grid_bin_matrix[x][y].white_space;
      total_component_area += grid_bin_matrix[x][y].component_area;
    }
  }
  filling_rate = ComputeFillingRate(total_component_area, total_white_space);
}

void SpreadingRegion::UpdateComponentAreaWhiteSpaceFillingRate(
    std::vector<std::vector<unsigned long long>>& grid_bin_white_space_LUT,
    std::vector<std::vector<GridBin>>& grid_bin_matrix) {
  if (ll_index.x == 0) {
    if (ll_index.y == 0) {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y];
    } else {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y] -
                          grid_bin_white_space_LUT[ur_index.x][ll_index.y - 1];
    }
  } else {
    if (ll_index.y == 0) {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y] -
                          grid_bin_white_space_LUT[ll_index.x - 1][ur_index.y];
    } else {
      total_white_space =
          grid_bin_white_space_LUT[ur_index.x][ur_index.y] -
          grid_bin_white_space_LUT[ur_index.x][ll_index.y - 1] -
          grid_bin_white_space_LUT[ll_index.x - 1][ur_index.y] +
          grid_bin_white_space_LUT[ll_index.x - 1][ll_index.y - 1];
    }
  }
  total_component_area = 0;
  for (int x = ll_index.x; x <= ur_index.x; x++) {
    for (int y = ll_index.y; y <= ur_index.y; y++) {
      total_component_area += grid_bin_matrix[x][y].component_area;
    }
  }
  filling_rate = ComputeFillingRate(total_component_area, total_white_space);
}

void SpreadingRegion::ExpandBox(int grid_cnt_x, int grid_cnt_y) {
  if (ll_index.x == 0 && ll_index.y == 0 && ur_index.x == grid_cnt_x - 1 &&
      ur_index.y == grid_cnt_y - 1) {
    LOG(fatal) << "Reach maximum, cannot further expand\n";
  }
  if (ll_index.x > 0) --ll_index.x;
  if (ll_index.y > 0) --ll_index.y;
  if (ur_index.x < grid_cnt_x - 1) ++ur_index.x;
  if (ur_index.y < grid_cnt_y - 1) ++ur_index.y;
}

bool SpreadingRegion::write_box_boundary(std::string const& NameOfFile) {
  std::ofstream ost;
  ost.open(NameOfFile.c_str(), std::ios::app);
  if (ost.is_open() == 0) {
    LOG(info) << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  double low_x, low_y, width, height;
  width = right - left;
  height = top - bottom;
  low_x = left;
  low_y = bottom;
  int step = 20;
  for (int j = 0; j < height; j += step) {
    ost << low_x << "\t" << low_y + j << "\n";
    ost << low_x + width << "\t" << low_y + j << "\n";
  }
  for (int j = 0; j < width; j += step) {
    ost << low_x + j << "\t" << low_y << "\n";
    ost << low_x + j << "\t" << low_y + height << "\n";
  }
  ost.close();
  return true;
}

bool SpreadingRegion::WriteComponentRegion(std::string const& NameOfFile) {
  std::ofstream ost;
  ost.open(NameOfFile.c_str(), std::ios::app);
  if (ost.is_open() == 0) {
    LOG(info) << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  double low_x, low_y, width, height;
  width = ur_point.x - ll_point.x;
  height = ur_point.y - ll_point.y;
  low_x = ll_point.x;
  low_y = ll_point.y;
  int step = 30;
  for (int j = 0; j < height; j += step) {
    ost << low_x << "\t" << low_y + j << "\n";
    ost << low_x + width << "\t" << low_y + j << "\n";
  }
  for (int j = 0; j < width; j += step) {
    ost << low_x + j << "\t" << low_y << "\n";
    ost << low_x + j << "\t" << low_y + height << "\n";
  }
  ost.close();
  return true;
}

void SpreadingRegion::UpdateComponentList(
    std::vector<std::vector<GridBin>>& grid_bin_matrix) {
  component_ptrs.clear();
  for (int x = ll_index.x; x <= ur_index.x; x++) {
    for (int y = ll_index.y; y <= ur_index.y; y++) {
      for (auto& component_ptr : grid_bin_matrix[x][y].component_ptrs) {
        component_ptrs.push_back(component_ptr);
      }
      grid_bin_matrix[x][y].component_ptrs.clear();
      grid_bin_matrix[x][y].component_area = 0;
      grid_bin_matrix[x][y].over_fill = false;
    }
  }
}

void SpreadingRegion::UpdateBoundaries(
    std::vector<std::vector<GridBin>>& grid_bin_matrix) {
  left = grid_bin_matrix[ll_index.x][ll_index.y].left;
  bottom = grid_bin_matrix[ll_index.x][ll_index.y].bottom;
  right = grid_bin_matrix[ur_index.x][ur_index.y].right;
  top = grid_bin_matrix[ur_index.x][ur_index.y].top;
}

void SpreadingRegion::UpdatePlacementBlockages(
    std::vector<std::vector<GridBin>>& grid_bin_matrix) {
  placement_blockages_.clear();
  std::unordered_set<const PlacementBlockage*> seen;
  for (int x = ll_index.x; x <= ur_index.x; ++x) {
    for (int y = ll_index.y; y <= ur_index.y; ++y) {
      for (const PlacementBlockage* blockage_ptr :
           grid_bin_matrix[x][y].placement_blockages_) {
        if (seen.insert(blockage_ptr).second) {
          placement_blockages_.push_back(blockage_ptr);
        }
      }
    }
  }
}

void SpreadingRegion::UpdateWhiteSpaceAndFixedComponents(
    const std::vector<const PlacementBlockage*>& placement_blockages) {
  placement_blockages_.clear();
  total_white_space =
      (unsigned long long)(right - left) * (unsigned long long)(top - bottom);
  RectI bin_rect(left, bottom, right, top);

  std::vector<RectI> rects;
  for (auto& blockage_ptr : placement_blockages) {
    auto& blockage = *blockage_ptr;
    if (bin_rect.IsOverlap(blockage.GetRect())) {
      placement_blockages_.push_back(blockage_ptr);
      rects.push_back(bin_rect.GetOverlapRect(blockage.GetRect()));
    }
  }

  unsigned long long used_area = GetCoverArea(rects);
  if (total_white_space < used_area) {
    DaliExpects(false,
                "Fixed components takes more space than available space? "
                    << total_white_space << " " << used_area);
  }

  total_white_space -= used_area;
}

void SpreadingRegion::UpdateObsBoundary() {
  vertical_cutlines.clear();
  horizontal_cutlines.clear();
  if (placement_blockages_.empty()) {
    return;
  }
  for (auto& blockage_ptr : placement_blockages_) {
    const RectI& rect = blockage_ptr->GetRect();

    if ((left < rect.LLX()) && (right > rect.LLX())) {
      vertical_cutlines.push_back((int)rect.LLX());
    }
    if ((left < rect.URX()) && (right > rect.URX())) {
      vertical_cutlines.push_back((int)rect.URX());
    }
    if ((bottom < rect.LLY()) && (top > rect.LLY())) {
      horizontal_cutlines.push_back((int)rect.LLY());
    }
    if ((bottom < rect.URY()) && (top > rect.URY())) {
      horizontal_cutlines.push_back((int)rect.URY());
    }
  }
  /* sort boundaries in the ascending order */
  size_t min_index;
  int min_boundary, tmp_boundary;
  if (!horizontal_cutlines.empty()) {
    for (size_t i = 0; i < horizontal_cutlines.size(); i++) {
      min_index = i;
      min_boundary = horizontal_cutlines[i];
      for (size_t j = i + 1; j < horizontal_cutlines.size(); j++) {
        if (horizontal_cutlines[j] < min_boundary) {
          min_index = j;
          min_boundary = horizontal_cutlines[j];
        }
      }
      tmp_boundary = horizontal_cutlines[i];
      horizontal_cutlines[i] = horizontal_cutlines[min_index];
      horizontal_cutlines[min_index] = tmp_boundary;
    }
  }
  if (!vertical_cutlines.empty()) {
    for (size_t i = 0; i < vertical_cutlines.size(); i++) {
      min_index = i;
      min_boundary = vertical_cutlines[i];
      for (size_t j = i + 1; j < vertical_cutlines.size(); j++) {
        if (vertical_cutlines[j] < min_boundary) {
          min_index = j;
          min_boundary = vertical_cutlines[j];
        }
      }
      tmp_boundary = vertical_cutlines[i];
      vertical_cutlines[i] = vertical_cutlines[min_index];
      vertical_cutlines[min_index] = tmp_boundary;
    }
  }
  }

bool SpreadingRegion::IsMoreHorizontalCutlines() const {
  return horizontal_cutlines.size() > vertical_cutlines.size();
}

bool SpreadingRegion::WriteComponentsInBox(std::string const& NameOfFile) {
  std::ofstream ost;
  ost.open(NameOfFile.c_str(), std::ios::app);
  if (ost.is_open() == 0) {
    LOG(info) << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  for (auto& component_ptr : component_ptrs) {
    if (component_ptr->IsMovable()) {
      ost << component_ptr->X() << "\t" << component_ptr->Y() << "\n";
    }
  }
  ost.close();
  return true;
}

unsigned long long SpreadingRegion::white_space_LUT(
    std::vector<std::vector<unsigned long long>>& grid_bin_white_space_LUT,
    GridBinIndex& ll, GridBinIndex& ur) {
  unsigned long long white_space;
  if (ll.x == 0) {
    if (ll.y == 0) {
      white_space = grid_bin_white_space_LUT[ur.x][ur.y];
    } else {
      white_space = grid_bin_white_space_LUT[ur.x][ur.y] -
                    grid_bin_white_space_LUT[ur.x][ll.y - 1];
    }
  } else {
    if (ll.y == 0) {
      white_space = grid_bin_white_space_LUT[ur.x][ur.y] -
                    grid_bin_white_space_LUT[ll.x - 1][ur.y];
    } else {
      white_space = grid_bin_white_space_LUT[ur.x][ur.y] -
                    grid_bin_white_space_LUT[ur.x][ll.y - 1] -
                    grid_bin_white_space_LUT[ll.x - 1][ur.y] +
                    grid_bin_white_space_LUT[ll.x - 1][ll.y - 1];
    }
  }
  return white_space;
}

bool SpreadingRegion::update_cut_index_white_space(
    std::vector<std::vector<unsigned long long>>& grid_bin_white_space_LUT,
    std::vector<std::vector<GridBin>>& grid_bin_matrix,
    GlobalLalMacroBoundaryMode macro_boundary_mode) {
  DaliExpects(total_white_space > 0,
              "Cannot split a box without available white space");
  auto choose_cut_index = [&](int first_index, int last_index,
                              unsigned long long total_space,
                              const std::vector<int>& macro_boundaries,
                              bool split_y) {
    DaliExpects(first_index < last_index, "Invalid cut index range");

    auto white_space_error = [&](int index) {
      GridBinIndex cut_ur = ur_index;
      if (split_y) {
        cut_ur.y = index;
      } else {
        cut_ur.x = index;
      }
      unsigned long long low_space =
          white_space_LUT(grid_bin_white_space_LUT, ll_index, cut_ur);
      return std::fabs(double(low_space) / double(total_space) - 0.5);
    };

    int balanced_index = first_index;
    double balanced_error = std::numeric_limits<double>::infinity();
    for (int index = first_index; index < last_index; ++index) {
      double error = white_space_error(index);
      if (error < balanced_error) {
        balanced_error = error;
        balanced_index = index;
      }
    }

    if (macro_boundary_mode == GlobalLalMacroBoundaryMode::kOff) {
      return balanced_index;
    }

    int macro_index = balanced_index;
    double macro_error = std::numeric_limits<double>::infinity();
    for (int boundary : macro_boundaries) {
      int nearest_index = first_index;
      int nearest_distance = std::numeric_limits<int>::max();
      for (int index = first_index; index < last_index; ++index) {
        int grid_boundary = split_y ? grid_bin_matrix[ll_index.x][index].top
                                    : grid_bin_matrix[index][ll_index.y].right;
        int distance = std::abs(grid_boundary - boundary);
        if (distance < nearest_distance) {
          nearest_distance = distance;
          nearest_index = index;
        }
      }
      double error = white_space_error(nearest_index);
      if (error < macro_error) {
        macro_error = error;
        macro_index = nearest_index;
      }
    }

    // Prefer obstacle-aligned cuts when they are close to a balanced
    // whitespace split: a macro edge is already a natural boundary, so cutting
    // there costs little and avoids forcing pathological tiny regions.
    double macro_cut_extra_tolerance =
        macro_boundary_mode == GlobalLalMacroBoundaryMode::kPreferred ? 0.10
                                                                      : 0.0;
    if (!macro_boundaries.empty() &&
        macro_error <= balanced_error + macro_cut_extra_tolerance) {
      return macro_index;
    }
    return balanced_index;
  };

  if (cut_direction_x) {
    if (ll_index.y == ur_index.y) return false;
    cut_ur_index.x = ur_index.x;
    cut_ll_index.x = ll_index.x;
    cut_ur_index.y = choose_cut_index(ll_index.y, ur_index.y, total_white_space,
                                      horizontal_cutlines, /*split_y=*/true);
    cut_ll_index.y = cut_ur_index.y + 1;
    return true;
  } else {
    if (ll_index.x == ur_index.x) return false;
    cut_ur_index.y = ur_index.y;
    cut_ll_index.y = ll_index.y;
    cut_ur_index.x = choose_cut_index(ll_index.x, ur_index.x, total_white_space,
                                      vertical_cutlines, /*split_y=*/false);
    cut_ll_index.x = cut_ur_index.x + 1;
    return true;
  }
}

bool SpreadingRegion::UpdateCutPointComponentLists(
    unsigned long long& box1_total_white_space,
    unsigned long long& box2_total_white_space) {
  // this member function will be called only when two white spaces are not
  // different from each other for several magnitudes
  DaliExpects(box1_total_white_space > 0,
              "Cannot split component list against zero lower-box white space");
  DaliExpects(total_component_area > 0,
              "Cannot split an empty component list by component area");
  unsigned long long component_area_low = 0;
  double cut_line_low, cut_line_high;
  double cut_line = 0;
  double ratio =
      1 + double(box2_total_white_space) / double(box1_total_white_space);
  // TODO: this method can be accelerated
  if (cut_direction_x) {
    cut_ur_point.x = ur_point.x;
    cut_ll_point.x = ll_point.x;
    cut_line_low = ll_point.y;
    cut_line_high = ur_point.y;
    for (int i = 0; i < 20; i++) {
      component_area_low = 0;
      cut_line = (cut_line_low + cut_line_high) / 2;
      for (auto& component_ptr : component_ptrs) {
        if (component_ptr->Y() < cut_line) {
          component_area_low += component_ptr->Area();
        }
      }
      // "\n";
      double tmp_ratio =
          component_area_low == 0
              ? std::numeric_limits<double>::infinity()
              : double(total_component_area) / double(component_area_low);
      // TODO: this precision has some influence on the final result
      if (ratio > tmp_ratio) {
        cut_line_high = cut_line;
      } else if (ratio < tmp_ratio) {
        cut_line_low = cut_line;
      } else {
        break;
      }
    }
    total_component_area_low = component_area_low;
    total_component_area_high = total_component_area - total_component_area_low;
    cut_ll_point.y = cut_line;
    cut_ur_point.y = cut_line;
    // << ll_point.y << "\n";
    for (auto& component_ptr : component_ptrs) {
      if (component_ptr->Y() < cut_line) {
        component_ptrs_low.push_back(component_ptr);
      } else {
        component_ptrs_high.push_back(component_ptr);
      }
    }
  } else {
    cut_ur_point.y = ur_point.y;
    cut_ll_point.y = ll_point.y;
    cut_line_low = ll_point.x;
    cut_line_high = ur_point.x;
    for (int i = 0; i < 20; i++) {
      component_area_low = 0;
      cut_line = (cut_line_low + cut_line_high) / 2;
      for (auto& component_ptr : component_ptrs) {
        if (component_ptr->X() < cut_line) {
          component_area_low += component_ptr->Area();
        }
      }
      // "\n";
      double tmp_ratio =
          component_area_low == 0
              ? std::numeric_limits<double>::infinity()
              : double(total_component_area) / double(component_area_low);
      if (ratio > tmp_ratio) {
        cut_line_high = cut_line;
      } else if (ratio < tmp_ratio) {
        cut_line_low = cut_line;
      } else {
        break;
      }
    }
    total_component_area_low = component_area_low;
    total_component_area_high = total_component_area - total_component_area_low;
    cut_ll_point.x = cut_line;
    cut_ur_point.x = cut_line;
    // << ur_point.x << "\n";
    for (auto& component_ptr : component_ptrs) {
      if (component_ptr->X() < cut_line) {
        component_ptrs_low.push_back(component_ptr);
      } else {
        component_ptrs_high.push_back(component_ptr);
      }
    }
  }
  return true;
}

bool SpreadingRegion::UpdateCutPointComponentListsLeaf(
    int& cut_line_w, int average_component_height) {
  DaliExpects(total_component_area > 0,
              "Cannot split an empty leaf box by component area");
  unsigned long long component_area_low = 0;
  double cut_line = 0;
  // Cutlines are continuous placement coordinates, so keep them as doubles.
  double ratio = 2.0;
  // this ratio is to say that component_area_low should be close to
  // total_component_area/ratio
  double mini_error = 1;
  double component_area_low_percentage = 0;

  /* Sort components instead of using bisection. When cut_direction_x is true,
   * the white-space cutline is chosen between top and bottom. Otherwise the
   * cutline is chosen close to one half of the total component area. */
  Component *node, *node1;
  if (cut_direction_x) {
    int box_height = top - bottom;
    int row_num = box_height / average_component_height;
    double low_white_space_total_ratio = 0.5;
    // first part, find the cut-line of white space, which the the middle of top
    // and bottom of this box
    low_white_space_total_ratio = std::floor(row_num / 2.0) / row_num;
    cut_line_w = bottom + (int)(low_white_space_total_ratio * box_height);
    /* second part, split the total component_ptrs to two part,
     * by sort component_ptrs based on y location in ascending order */
    size_t mini_index;
    double mini_loc;
    for (size_t i = 0; i < component_ptrs.size(); i++) {
      node = component_ptrs[i];
      mini_index = i;
      mini_loc = node->Y();
      for (size_t j = i + 1; j < component_ptrs.size(); j++) {
        node1 = component_ptrs[j];
        if (node1->Y() < mini_loc) {
          mini_index = j;
          mini_loc = node1->Y();
        }
      }
      Component* tmp_component_ptr = component_ptrs[mini_index];
      component_ptrs[mini_index] = component_ptrs[i];
      component_ptrs[i] = tmp_component_ptr;
    }

    /* Find the component index whose cumulative area is closest to the target
     * lower-box white-space ratio. */
    unsigned long long tmp_total_component_area_low = 0;
    int lower_area_split_index = 0;
    for (size_t i = 0; i < component_ptrs.size(); i++) {
      node = component_ptrs[i];
      tmp_total_component_area_low += node->Area();
      component_area_low_percentage =
          double(tmp_total_component_area_low) / double(total_component_area);
      // "\n";
      if (fabs(component_area_low_percentage - low_white_space_total_ratio) <
          mini_error) {
        mini_error =
            fabs(component_area_low_percentage - low_white_space_total_ratio);
        lower_area_split_index = i;
      }
      if (component_area_low_percentage >= low_white_space_total_ratio) {
        // " << lower_area_split_index << "\n";
        break;
      }
    }
    /* if the index is smaller than this index, put the component to
     * component_ptrs_low, otherwise, put it to component_ptrs_high and update
     * total_component_area_low and total_component_area_high */
    component_area_low = 0;
    for (int i = 0; i < (int)component_ptrs.size(); i++) {
      node = component_ptrs[i];
      if (i <= lower_area_split_index) {
        component_area_low += node->Area();
        component_ptrs_low.push_back(component_ptrs[i]);
      } else {
        component_ptrs_high.push_back(component_ptrs[i]);
      }
    }
    total_component_area_low = component_area_low;
    total_component_area_high = total_component_area - total_component_area_low;

    /* third part, find the cut-line to split component area,
     * in this case, the absolute value of this line if actually not important,
     * it is set to the y-coordinate of the component closest to the target
     * component-area split. */
    cut_ur_point.x = ur_point.x;
    cut_ll_point.x = ll_point.x;
    node = component_ptrs[lower_area_split_index];
    cut_line = node->Y();
    cut_ll_point.y = cut_line;
    cut_ur_point.y = cut_line;
  } else {
    /* first, split the total component_ptrs to two part,
     * by sort component_ptrs based on y location in ascending order */
    size_t mini_index;
    double mini_loc;
    for (size_t i = 0; i < component_ptrs.size(); i++) {
      node = component_ptrs[i];
      mini_index = i;
      mini_loc = node->X();
      for (size_t j = i + 1; j < component_ptrs.size(); j++) {
        node1 = component_ptrs[j];
        if (node1->X() < mini_loc) {
          mini_index = j;
          mini_loc = node1->X();
        }
      }
      Component* tmp_component_ptr = component_ptrs[mini_index];
      component_ptrs[mini_index] = component_ptrs[i];
      component_ptrs[i] = tmp_component_ptr;
    }
    /* Find the component index whose cumulative area is closest to one half of
     * the total component area. */
    unsigned long long tmp_total_component_area_low = 0;
    int lower_area_split_index = 0;
    for (size_t i = 0; i < component_ptrs.size(); i++) {
      node = component_ptrs[i];
      tmp_total_component_area_low += node->Area();
      component_area_low_percentage =
          double(tmp_total_component_area_low) / double(total_component_area);
      // "\n";
      if (fabs(component_area_low_percentage - 1 / ratio) < mini_error) {
        mini_error = fabs(component_area_low_percentage - 1 / ratio);
        lower_area_split_index = i;
      }
      if (component_area_low_percentage > 0.5) {
        // " << lower_area_split_index << "\n";
        break;
      }
    }
    /* third, if the index is smaller than this index, put the component to
     * component_ptrs_low, otherwise, put it to component_ptrs_high and update
     * total_component_area_low and total_component_area_high */
    component_area_low = 0;
    for (int i = 0; i < (int)component_ptrs.size(); i++) {
      node = component_ptrs[i];
      if (i <= lower_area_split_index) {
        component_area_low += node->Area();
        component_ptrs_low.push_back(component_ptrs[i]);
      } else {
        component_ptrs_high.push_back(component_ptrs[i]);
      }
    }
    total_component_area_low = component_area_low;
    total_component_area_high = total_component_area - total_component_area_low;
    /* forth, find the cut-line to split component area,
     * in this case, the absolute value of this line if actually not important,
     * it is set to the coordinate of the component closest to half of the total
     * component area. */
    cut_ur_point.y = ur_point.y;
    cut_ll_point.y = ll_point.y;
    node = component_ptrs[lower_area_split_index];
    cut_line = node->X();
    cut_ll_point.x = cut_line;
    cut_ur_point.x = cut_line;
    /* finally, the cut-line for white space is proportional to the
     * total_component_area_low */
    cut_line_w = left + (int)((double(total_component_area_low) /
                               double(total_component_area)) *
                              (right - left));
  }
  return true;
}

void SpreadingRegion::Report() {
  std::string cur_direction = cut_direction_x ? "x" : "y";
  LOG(info) << "cut direction: " << cur_direction << "\n"
            << "white spaces all used by macros: " << all_terminal << "\n"
            << "total white space: " << total_white_space << "\n"
            << "grid bin index: " << ll_index << " " << ur_index << "\n"
            << "grid bin cut index: " << cut_ll_index << " " << cut_ur_index
            << "\n"
            << "box coordinate: " << ll_point << " " << ur_point << "\n"
            << "box cut coordinate: " << cut_ll_point << " " << cut_ur_point
            << "\n"
            << "total component area: " << total_component_area << "\n"
            << "total component area low: " << total_component_area_low << "\n"
            << "total component area high: " << total_component_area_high
            << "\n"
            << "shape: (" << left << ", " << bottom << ") (" << right << ", "
            << top << ")\n";

  LOG(info) << "component list: " << component_ptrs.size() << "\n";
  for (auto& component : component_ptrs) {
    LOG(info) << component->Name() << ", "
              << "(" << component->LLX() << ", " << component->LLY() << "), "
              << "(" << component->URX() << ", " << component->URY() << ")\n";
  }
  LOG(info) << "\nend\n";

  LOG(info) << "component list low: " << component_ptrs_low.size() << "\n";
  for (auto& num : component_ptrs_low) {
    LOG(info) << num << ", ";
  }
  LOG(info) << "\nend\n";

  LOG(info) << "component list hi: " << component_ptrs_high.size() << "\n";
  for (auto& num : component_ptrs_high) {
    LOG(info) << num << ", ";
  }
  LOG(info) << "\nend\n";

  LOG(info) << "blockage list: " << placement_blockages_.size() << "\n";
  for (auto& p_blockage : placement_blockages_) {
    const RectI& rect = p_blockage->GetRect();
    LOG(info) << "(" << rect.LLX() << ", " << rect.LLY() << "), "
              << "(" << rect.URX() << ", " << rect.URY() << ")\n";
  }
  LOG(info) << "\nend\n";

  LOG(info) << "vertical boundaries\n";
  for (auto& num : vertical_cutlines) {
    LOG(info) << num << ", ";
  }
  LOG(info) << "\nend\n";

  LOG(info) << "horizontal boundaries\n";
  for (auto& num : horizontal_cutlines) {
    LOG(info) << num << ", ";
  }
  LOG(info) << "\nend\n";
}

}  // namespace dali
