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

#include "box_bin.h"

#include <cmath>

#include "dali/common/helper.h"

namespace dali {

BoxBin::BoxBin() {
  all_terminal = false;
  cut_direction_x = false;
  total_cell_area = 0;
  total_white_space = 0;
  filling_rate = 0;
  total_cell_area_low = 0;
  total_cell_area_high = 0;
  left = 0;
  right = 0;
  bottom = 0;
  top = 0;
}

void BoxBin::update_all_terminal(std::vector<std::vector<GridBin> > &grid_bin_matrix) {
  GridBin *bin;
  for (int x = ll_index.x; x <= ur_index.x; x++) {
    for (int y = ll_index.y; y <= ur_index.y; y++) {
      bin = &grid_bin_matrix[x][y];
      if (!bin->IsAllFixedBlk()) {
        all_terminal = false;
        return;
      }
    }
  }
  all_terminal = true;
}

void BoxBin::update_cell_area() {
  /*
  int temp_total_cell_area = 0;
  Block *node;
  for (auto &cell_id: cell_list) {
    node = &Nodelist[cell_id];
    temp_total_cell_area += node->Area();
  }
  BOOST_LOG_TRIVIAL(info)   << "Total cell area: " << total_cell_area << "  " << temp_total_cell_area << "\n";
  temp_total_cell_area = 0;
  for (auto &node: Nodelist) {
    if (node.isterminal()) continue;
    if ((node.x0 >= ll_point.x) && (node.x0 < ur_point.x) && (node.y0 >= ll_point.y) && (node.y0 < ur_point.y)) {
      temp_total_cell_area += node.Area();
    }
  }
  BOOST_LOG_TRIVIAL(info)   << "Total cell area: " << total_cell_area << "  " << temp_total_cell_area << "\n";
  */

  total_cell_area = 0;
  for (auto &blk_ptr : cell_list) {
    total_cell_area += blk_ptr->Area();
  }
}

void BoxBin::update_cell_area_white_space(std::vector<std::vector<GridBin> > &grid_bin_matrix) {
  total_cell_area = 0;
  total_white_space = 0;
  for (int x = ll_index.x; x <= ur_index.x; x++) {
    for (int y = ll_index.y; y <= ur_index.y; y++) {
      total_white_space += grid_bin_matrix[x][y].white_space;
      total_cell_area += grid_bin_matrix[x][y].cell_area;
    }
  }
  filling_rate = double(total_cell_area) / double(total_white_space);
}

void BoxBin::UpdateCellAreaWhiteSpaceFillingRate(
    std::vector<std::vector<unsigned long long>> &grid_bin_white_space_LUT,
    std::vector<std::vector<GridBin>> &grid_bin_matrix
) {
  if (ll_index.x == 0) {
    if (ll_index.y == 0) {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y];
    } else {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
          - grid_bin_white_space_LUT[ur_index.x][ll_index.y - 1];
    }
  } else {
    if (ll_index.y == 0) {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
          - grid_bin_white_space_LUT[ll_index.x - 1][ur_index.y];
    } else {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
          - grid_bin_white_space_LUT[ur_index.x][ll_index.y - 1]
          - grid_bin_white_space_LUT[ll_index.x - 1][ur_index.y]
          + grid_bin_white_space_LUT[ll_index.x - 1][ll_index.y - 1];
    }
  }
  total_cell_area = 0;
  for (int x = ll_index.x; x <= ur_index.x; x++) {
    for (int y = ll_index.y; y <= ur_index.y; y++) {
      total_cell_area += grid_bin_matrix[x][y].cell_area;
    }
  }
  filling_rate = double(total_cell_area) / double(total_white_space);
}

void BoxBin::ExpandBox(int grid_cnt_x, int grid_cnt_y) {
  if (ll_index.x == 0 && ll_index.y == 0 && ur_index.x == grid_cnt_x - 1
      && ur_index.y == grid_cnt_y - 1) {
    BOOST_LOG_TRIVIAL(fatal) << "Reach maximum, cannot further expand\n";
  }
  if (ll_index.x > 0) --ll_index.x;
  if (ll_index.y > 0) --ll_index.y;
  if (ur_index.x < grid_cnt_x - 1) ++ur_index.x;
  if (ur_index.y < grid_cnt_y - 1) ++ur_index.y;
}

bool BoxBin::write_box_boundary(std::string const &NameOfFile) {
  std::ofstream ost;
  ost.open(NameOfFile.c_str(), std::ios::app);
  if (ost.is_open() == 0) {
    BOOST_LOG_TRIVIAL(info) << "Cannot open file" << NameOfFile << "\n";
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

bool BoxBin::write_cell_region(std::string const &NameOfFile) {
  std::ofstream ost;
  ost.open(NameOfFile.c_str(), std::ios::app);
  if (ost.is_open() == 0) {
    BOOST_LOG_TRIVIAL(info) << "Cannot open file" << NameOfFile << "\n";
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

void BoxBin::UpdateCellList(std::vector<std::vector<GridBin> > &grid_bin_matrix) {
  cell_list.clear();
  for (int x = ll_index.x; x <= ur_index.x; x++) {
    for (int y = ll_index.y; y <= ur_index.y; y++) {
      for (auto &blk_ptr : grid_bin_matrix[x][y].cell_list) {
        cell_list.push_back(blk_ptr);
      }
      grid_bin_matrix[x][y].cell_list.clear();
      grid_bin_matrix[x][y].cell_area = 0;
      grid_bin_matrix[x][y].over_fill = false;
    }
  }
}

void BoxBin::UpdateBoundaries(std::vector<std::vector<GridBin> > &grid_bin_matrix) {
  left = grid_bin_matrix[ll_index.x][ll_index.y].left;
  bottom = grid_bin_matrix[ll_index.x][ll_index.y].bottom;
  right = grid_bin_matrix[ur_index.x][ur_index.y].right;
  top = grid_bin_matrix[ur_index.x][ur_index.y].top;
}

void BoxBin::UpdateWhiteSpaceAndFixedBlocks(
    std::vector<Block *> &box_fixed_blocks,
    std::vector<RectI *> &dummy_placement_blockages
) {
  total_white_space = (unsigned long long) (right - left) *
      (unsigned long long) (top - bottom);
  RectI bin_rect(left, bottom, right, top);

  std::vector<RectI> rects;
  for (auto &fixed_blk_ptr : box_fixed_blocks) {
    auto &fixed_blk = *fixed_blk_ptr;
    RectI fixed_blk_rect(
        static_cast<int>(std::round(fixed_blk.LLX())),
        static_cast<int>(std::round(fixed_blk.LLY())),
        static_cast<int>(std::round(fixed_blk.URX())),
        static_cast<int>(std::round(fixed_blk.URY()))
    );
    if (bin_rect.IsOverlap(fixed_blk_rect)) {
      fixed_blocks.push_back(fixed_blk_ptr);
      rects.push_back(bin_rect.GetOverlapRect(fixed_blk_rect));
    }
  }

  for (auto &blockage_ptr : dummy_placement_blockages) {
    auto &blockage = *blockage_ptr;
    if (bin_rect.IsOverlap(blockage)) {
      dummy_placement_blockages_.push_back(blockage_ptr);
      rects.push_back(bin_rect.GetOverlapRect(blockage));
    }
  }

  unsigned long long used_area = GetCoverArea(rects);
  if (total_white_space < used_area) {
    DaliExpects(
        false,
        "Fixed blocks takes more space than available space? "
            << total_white_space << " " << used_area
    );
  }

  total_white_space -= used_area;
}

void BoxBin::UpdateObsBoundary() {
  vertical_cutlines.clear();
  horizontal_cutlines.clear();
  if (fixed_blocks.empty() && dummy_placement_blockages_.empty()) {
    return;
  }
  for (auto &blk_ptr : fixed_blocks) {
    Block &node = *blk_ptr;
    if ((left < node.LLX()) && (right > node.LLX())) {
      vertical_cutlines.push_back((int) node.LLX());
    }
    if ((left < node.URX()) && (right > node.URX())) {
      vertical_cutlines.push_back((int) node.URX());
    }
    if ((bottom < node.LLY()) && (top > node.LLY())) {
      horizontal_cutlines.push_back((int) node.LLY());
    }
    if ((bottom < node.URY()) && (top > node.URY())) {
      horizontal_cutlines.push_back((int) node.URY());
    }
  }
  for (auto &blockage_ptr : dummy_placement_blockages_) {
    RectI &blockage = *blockage_ptr;
    if ((left < blockage.LLX()) && (right > blockage.LLX())) {
      vertical_cutlines.push_back((int) blockage.LLX());
    }
    if ((left < blockage.URX()) && (right > blockage.URX())) {
      vertical_cutlines.push_back((int) blockage.URX());
    }
    if ((bottom < blockage.LLY()) && (top > blockage.LLY())) {
      horizontal_cutlines.push_back((int) blockage.LLY());
    }
    if ((bottom < blockage.URY()) && (top > blockage.URY())) {
      horizontal_cutlines.push_back((int) blockage.URY());
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
      horizontal_cutlines[i] =
          horizontal_cutlines[min_index];
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
  /*
  BOOST_LOG_TRIVIAL(info)   << "Horizontal_obs_boundaries: ";
  for (auto &boundary: horizontal_obstacle_boundaries) {
    BOOST_LOG_TRIVIAL(info)   << boundary << " ";
  }
  BOOST_LOG_TRIVIAL(info)   << "\n";
  BOOST_LOG_TRIVIAL(info)   << "Vertical_obs_boundaries: ";
  for (auto &boundary: vertical_obstacle_boundaries) {
    BOOST_LOG_TRIVIAL(info)   << boundary << " ";
  }
  BOOST_LOG_TRIVIAL(info)   << "\n";
  */
}

bool BoxBin::IsMoreHorizontalCutlines() const {
  return horizontal_cutlines.size() > vertical_cutlines.size();
}

bool BoxBin::write_cell_in_box(
    std::string const &NameOfFile
) {
  std::ofstream ost;
  ost.open(NameOfFile.c_str(), std::ios::app);
  if (ost.is_open() == 0) {
    BOOST_LOG_TRIVIAL(info) << "Cannot open file" << NameOfFile << "\n";
    return false;
  }
  for (auto &blk_ptr : cell_list) {
    if (blk_ptr->IsMovable()) {
      ost << blk_ptr->X() << "\t" << blk_ptr->Y() << "\n";
    }
  }
  ost.close();
  return true;
}

unsigned long long BoxBin::white_space_LUT(
    std::vector<std::vector<unsigned long long>> &grid_bin_white_space_LUT,
    GridBinIndex &ll,
    GridBinIndex &ur
) {
  unsigned long long white_space;
  if (ll.x == 0) {
    if (ll.y == 0) {
      white_space = grid_bin_white_space_LUT[ur.x][ur.y];
    } else {
      white_space = grid_bin_white_space_LUT[ur.x][ur.y]
          - grid_bin_white_space_LUT[ur.x][ll.y - 1];
    }
  } else {
    if (ll.y == 0) {
      white_space = grid_bin_white_space_LUT[ur.x][ur.y]
          - grid_bin_white_space_LUT[ll.x - 1][ur.y];
    } else {
      white_space = grid_bin_white_space_LUT[ur.x][ur.y]
          - grid_bin_white_space_LUT[ur.x][ll.y - 1]
          - grid_bin_white_space_LUT[ll.x - 1][ur.y]
          + grid_bin_white_space_LUT[ll.x - 1][ll.y - 1];
    }
  }
  return white_space;
}

bool BoxBin::update_cut_index_white_space(std::vector<std::vector<unsigned long long>> &grid_bin_white_space_LUT) {
  double error, minimum_error = 1;
  unsigned long long white_space_low;
  int index_give_minimum_error;
  if (cut_direction_x) {
    if (ll_index.y == ur_index.y) return false;
    cut_ur_index.x = ur_index.x;
    cut_ll_index.x = ll_index.x;

    index_give_minimum_error = ll_index.y;

    for (cut_ur_index.y = ll_index.y; cut_ur_index.y < ur_index.y - 1;
         cut_ur_index.y++) {
      //BOOST_LOG_TRIVIAL(info)   << cut_ur_index.y << "\n";
      white_space_low =
          white_space_LUT(grid_bin_white_space_LUT, ll_index, cut_ur_index);
      error =
          std::fabs(double(white_space_low) / double(total_white_space) - 0.5);
      if (error < minimum_error) index_give_minimum_error = cut_ur_index.y;
      if (double(white_space_low) / double(total_white_space) > 0.5) break;
    }
    if (cut_ur_index.y != index_give_minimum_error) {
      cut_ur_index.y = index_give_minimum_error;
      //white_space_low = white_space_LUT(grid_bin_white_space_LUT, ll_index_, cut_ur_index);
    }
    cut_ll_index.y = cut_ur_index.y + 1;
    return true;
  } else {
    if (ll_index.x == ur_index.x) return false;
    cut_ur_index.y = ur_index.y;
    cut_ll_index.y = ll_index.y;

    index_give_minimum_error = ll_index.x;

    for (cut_ur_index.x = ll_index.x; cut_ur_index.x < ur_index.x - 1;
         cut_ur_index.x++) {
      //BOOST_LOG_TRIVIAL(info)   << cut_ur_index.x << "\n";
      white_space_low =
          white_space_LUT(grid_bin_white_space_LUT, ll_index, cut_ur_index);
      error =
          std::fabs(double(white_space_low) / double(total_white_space) - 0.5);
      if (error < minimum_error) index_give_minimum_error = cut_ur_index.x;
      if (double(white_space_low) / double(total_white_space) > 0.5) break;
    }
    if (cut_ur_index.x != index_give_minimum_error) {
      cut_ur_index.x = index_give_minimum_error;
      //white_space_low = white_space_LUT(grid_bin_white_space_LUT, ll_index_, cut_ur_index);
    }
    cut_ll_index.x = cut_ur_index.x + 1;
    return true;
  }
}

bool BoxBin::update_cut_point_cell_list_low_high(
    unsigned long long &box1_total_white_space,
    unsigned long long &box2_total_white_space
) {
  // this member function will be called only when two white spaces are not different from each other for several magnitudes
  unsigned long long cell_area_low = 0;
  double cut_line_low, cut_line_high;
  double cut_line = 0;
  double ratio =
      1 + double(box2_total_white_space) / double(box1_total_white_space);
  //TODO: this method can be accelerated
  if (cut_direction_x) {
    cut_ur_point.x = ur_point.x;
    cut_ll_point.x = ll_point.x;
    cut_line_low = ll_point.y;
    cut_line_high = ur_point.y;
    for (int i = 0; i < 20; i++) {
      //BOOST_LOG_TRIVIAL(info)   << i << "\n";
      cell_area_low = 0;
      cut_line = (cut_line_low + cut_line_high) / 2;
      for (auto &blk_ptr : cell_list) {
        if (blk_ptr->Y() < cut_line) {
          cell_area_low += blk_ptr->Area();
        }
      }
      //BOOST_LOG_TRIVIAL(info)   << cell_area_low/(double)total_cell_area << "\n";
      double tmp_ratio = double(total_cell_area) / double(cell_area_low);
      //TODO: this precision has some influence on the final result
      if (ratio > tmp_ratio) {
        cut_line_high = cut_line;
      } else if (ratio < tmp_ratio) {
        cut_line_low = cut_line;
      } else {
        break;
      }
    }
    total_cell_area_low = cell_area_low;
    total_cell_area_high = total_cell_area - total_cell_area_low;
    cut_ll_point.y = cut_line;
    cut_ur_point.y = cut_line;
    //BOOST_LOG_TRIVIAL(info)   << cut_line << " LLY " << ll_point.y << " URY " << ll_point.y << "\n";
    for (auto &blk_ptr : cell_list) {
      if (blk_ptr->Y() < cut_line) {
        cell_list_low.push_back(blk_ptr);
      } else {
        cell_list_high.push_back(blk_ptr);
      }
    }
  } else {
    cut_ur_point.y = ur_point.y;
    cut_ll_point.y = ll_point.y;
    cut_line_low = ll_point.x;
    cut_line_high = ur_point.x;
    for (int i = 0; i < 20; i++) {
      //BOOST_LOG_TRIVIAL(info)   << i << "\n";
      cell_area_low = 0;
      cut_line = (cut_line_low + cut_line_high) / 2;
      for (auto &blk_ptr : cell_list) {
        if (blk_ptr->X() < cut_line) {
          cell_area_low += blk_ptr->Area();
        }
      }
      //BOOST_LOG_TRIVIAL(info)   << cell_area_low/(double)total_cell_area << "\n";
      double tmp_ratio = double(total_cell_area) / double(cell_area_low);
      if (ratio > tmp_ratio) {
        cut_line_high = cut_line;
      } else if (ratio < tmp_ratio) {
        cut_line_low = cut_line;
      } else {
        break;
      }
    }
    total_cell_area_low = cell_area_low;
    total_cell_area_high = total_cell_area - total_cell_area_low;
    cut_ll_point.x = cut_line;
    cut_ur_point.x = cut_line;
    //BOOST_LOG_TRIVIAL(info)   << cut_line << " LLX " << ll_point.x << " URX " << ur_point.x << "\n";
    for (auto &blk_ptr : cell_list) {
      if (blk_ptr->X() < cut_line) {
        cell_list_low.push_back(blk_ptr);
      } else {
        cell_list_high.push_back(blk_ptr);
      }
    }
  }
  return true;
}

bool BoxBin::update_cut_point_cell_list_low_high_leaf(
    int &cut_line_w,
    int ave_blk_height
) {
  unsigned long long cell_area_low = 0;
  double cut_line = 0;
  // the above three cut-lines are for cells, thus they needs to be double
  double ratio = 2.0;
  // this ratio is to say that cell_area_low should be close to total_cell_area/ratio
  double mini_error = 1;
  double cell_area_low_percentage = 0;

  /* the way used here is sorting the cells instead of calculate the cut-line using bisection
   * when cut_direction_x is true, the new cut line of white space is chosen to be the middle of top and bottom, then update
   * cell_list_low and cell_list_high
   * when cut_direction_x is false, the new cut line of white space is chosen to be close to one half of total cell area */
  Block *node, *node1;
  if (cut_direction_x) {
    int box_height = top - bottom;
    int row_num = box_height / ave_blk_height;
    double low_white_space_total_ratio = 0.5;
    // first part, find the cut-line of white space, which the the middle of top and bottom of this box
    low_white_space_total_ratio = std::floor(row_num / 2.0) / row_num;
    cut_line_w = bottom + (int) (low_white_space_total_ratio * box_height);
    /*
    if ((box_height % ave_blk_height == 0) && (box_height > ave_blk_height)) {
      //BOOST_LOG_TRIVIAL(info)   << left << " " << right << " " << bottom << " " << top << "\n";
      low_white_space_total_ratio = std::floor(row_num/2.0)/row_num;
      cut_line_w = bottom + (int)(low_white_space_total_ratio * box_height);
    } else {
      BOOST_LOG_TRIVIAL(info)   << left << " " << right << " " << bottom << " " << top << "\n";
      BOOST_LOG_TRIVIAL(info)   << "Error: out of expectation, bin height is not the integer multiple of standard cell height!\n";
      exit(1);
    }*/

    /* second part, split the total cell_list to two part,
     * by sort cell_list based on y location in ascending order */
    size_t mini_index;
    double mini_loc;
    for (size_t i = 0; i < cell_list.size(); i++) {
      node = cell_list[i];
      mini_index = i;
      mini_loc = node->Y();
      for (size_t j = i + 1; j < cell_list.size(); j++) {
        node1 = cell_list[j];
        if (node1->Y() < mini_loc) {
          mini_index = j;
          mini_loc = node1->Y();
        }
      }
      Block *tmp_cell_ptr = cell_list[mini_index];
      cell_list[mini_index] = cell_list[i];
      cell_list[i] = tmp_cell_ptr;
    }

    /* and then, find the index of cell, the total cell area below which is closest to one half of the total cell area */
    unsigned long long tmp_tot_cell_area_low = 0;
    int index_tot_cell_low_closest_to_half = 0;
    for (size_t i = 0; i < cell_list.size(); i++) {
      node = cell_list[i];
      tmp_tot_cell_area_low += node->Area();
      cell_area_low_percentage =
          double(tmp_tot_cell_area_low) / double(total_cell_area);
      //BOOST_LOG_TRIVIAL(info)   << i << " " << cell_area_low_percentage << "\n";
      if (fabs(cell_area_low_percentage - low_white_space_total_ratio)
          < mini_error) {
        mini_error =
            fabs(cell_area_low_percentage - low_white_space_total_ratio);
        index_tot_cell_low_closest_to_half = i;
      }
      if (cell_area_low_percentage >= low_white_space_total_ratio) {
        //BOOST_LOG_TRIVIAL(info)   << "mini_error: " << mini_error << " index: " << index_tot_cell_low_closest_to_half << "\n";
        break;
      }
    }
    /* if the index is smaller than this index, put the cell_id to cell_list_low, otherwise, put it to cell_list_high
     * and update total_cell_area_low and total_cell_area_high */
    cell_area_low = 0;
    for (int i = 0; i < (int) cell_list.size(); i++) {
      node = cell_list[i];
      if (i <= index_tot_cell_low_closest_to_half) {
        cell_area_low += node->Area();
        cell_list_low.push_back(cell_list[i]);
      } else {
        cell_list_high.push_back(cell_list[i]);
      }
    }
    total_cell_area_low = cell_area_low;
    total_cell_area_high = total_cell_area - total_cell_area_low;

    /* third part, find the cut-line to split cell area,
     * in this case, the absolute value of this line if actually not important,
     * it is set to be the y-coordinate of the cell closest to half of the total cell area */
    cut_ur_point.x = ur_point.x;
    cut_ll_point.x = ll_point.x;
    node = cell_list[index_tot_cell_low_closest_to_half];
    cut_line = node->Y();
    cut_ll_point.y = cut_line;
    cut_ur_point.y = cut_line;
  } else {
    /* first, split the total cell_list to two part,
     * by sort cell_list based on y location in ascending order */
    size_t mini_index;
    double mini_loc;
    for (size_t i = 0; i < cell_list.size(); i++) {
      node = cell_list[i];
      mini_index = i;
      mini_loc = node->X();
      for (size_t j = i + 1; j < cell_list.size(); j++) {
        node1 = cell_list[j];
        if (node1->X() < mini_loc) {
          mini_index = j;
          mini_loc = node1->X();
        }
      }
      Block *tmp_cell_ptr = cell_list[mini_index];
      cell_list[mini_index] = cell_list[i];
      cell_list[i] = tmp_cell_ptr;
    }
    /* second, find the index of cell, the total cell area below which is closest to one half of the total cell area */
    unsigned long long tmp_tot_cell_area_low = 0;
    int index_tot_cell_low_closest_to_half = 0;
    for (size_t i = 0; i < cell_list.size(); i++) {
      node = cell_list[i];
      tmp_tot_cell_area_low += node->Area();
      cell_area_low_percentage =
          double(tmp_tot_cell_area_low) / double(total_cell_area);
      //BOOST_LOG_TRIVIAL(info)   << i << " " << cell_area_low_percentage << "\n";
      if (fabs(cell_area_low_percentage - 1 / ratio) < mini_error) {
        mini_error = fabs(cell_area_low_percentage - 1 / ratio);
        index_tot_cell_low_closest_to_half = i;
      }
      if (cell_area_low_percentage > 0.5) {
        //BOOST_LOG_TRIVIAL(info)   << "mini_error: " << mini_error << " index: " << index_tot_cell_low_closest_to_half << "\n";
        break;
      }
    }
    /* third, if the index is smaller than this index, put the cell_id to cell_list_low, otherwise, put it to cell_list_high
     * and update total_cell_area_low and total_cell_area_high */
    cell_area_low = 0;
    for (int i = 0; i < (int) cell_list.size(); i++) {
      node = cell_list[i];
      if (i <= index_tot_cell_low_closest_to_half) {
        cell_area_low += node->Area();
        cell_list_low.push_back(cell_list[i]);
      } else {
        cell_list_high.push_back(cell_list[i]);
      }
    }
    total_cell_area_low = cell_area_low;
    total_cell_area_high = total_cell_area - total_cell_area_low;
    /* forth, find the cut-line to split cell area,
     * in this case, the absolute value of this line if actually not important,
     * it is set to be the y-coordinate of the cell closest to half of the total cell area */
    cut_ur_point.y = ur_point.y;
    cut_ll_point.y = ll_point.y;
    node = cell_list[index_tot_cell_low_closest_to_half];
    cut_line = node->X();
    cut_ll_point.y = cut_line;
    cut_ur_point.y = cut_line;
    /* finally, the cut-line for white space is proportional to the total_cell_area_low */
    cut_line_w = left
        + (int) ((double(total_cell_area_low) / double(total_cell_area))
            * (right - left));
  }
  return true;
}

void BoxBin::Report() {
  std::string cur_direction = cut_direction_x ? "x" : "y";
  BOOST_LOG_TRIVIAL(info)
    << "cut direction: " << cur_direction << "\n"
    << "white spaces all used by macros: " << all_terminal << "\n"
    << "total white space: " << total_white_space << "\n"
    << "grid bin index: " << ll_index << " " << ur_index << "\n"
    << "grid bin cut index: " << cut_ll_index << " " << cut_ur_index << "\n"
    << "box coordinate: " << ll_point << " " << ur_point << "\n"
    << "box cut coordinate: " << cut_ll_point << " " << cut_ur_point << "\n"
    << "total cell area: " << total_cell_area << "\n"
    << "total cell area low: " << total_cell_area_low << "\n"
    << "total cell area high: " << total_cell_area_high << "\n"
    << "shape: (" << left << ", " << bottom
    << ") (" << right << ", " << top << ")\n";

  BOOST_LOG_TRIVIAL(info) << "cell list: " << cell_list.size() << "\n";
  for (auto &p_blk : cell_list) {
    BOOST_LOG_TRIVIAL(info)
      << p_blk->Name() << ", "
      << "(" << p_blk->LLX() << ", " << p_blk->LLY() << "), "
      << "(" << p_blk->URX() << ", " << p_blk->URY() << ")\n";
  }
  BOOST_LOG_TRIVIAL(info) << "\nend\n";

  BOOST_LOG_TRIVIAL(info) << "cell list low: " << cell_list_low.size() << "\n";
  for (auto &num : cell_list_low) {
    BOOST_LOG_TRIVIAL(info) << num << ", ";
  }
  BOOST_LOG_TRIVIAL(info) << "\nend\n";

  BOOST_LOG_TRIVIAL(info) << "cell list hi: " << cell_list_high.size() << "\n";
  for (auto &num : cell_list_high) {
    BOOST_LOG_TRIVIAL(info) << num << ", ";
  }
  BOOST_LOG_TRIVIAL(info) << "\nend\n";

  BOOST_LOG_TRIVIAL(info) << "terminal list: " << fixed_blocks.size() << "\n";
  for (auto &p_fixed_blk : fixed_blocks) {
    BOOST_LOG_TRIVIAL(info)
      << p_fixed_blk->Name() << ", "
      << "(" << p_fixed_blk->LLX() << ", " << p_fixed_blk->LLY() << "), "
      << "(" << p_fixed_blk->URX() << ", " << p_fixed_blk->URY() << ")\n";
  }
  BOOST_LOG_TRIVIAL(info) << "\nend\n";

  BOOST_LOG_TRIVIAL(info) << "vertical boundaries\n";
  for (auto &num : vertical_cutlines) {
    BOOST_LOG_TRIVIAL(info) << num << ", ";
  }
  BOOST_LOG_TRIVIAL(info) << "\nend\n";

  BOOST_LOG_TRIVIAL(info) << "horizontal boundaries\n";
  for (auto &num : horizontal_cutlines) {
    BOOST_LOG_TRIVIAL(info) << num << ", ";
  }
  BOOST_LOG_TRIVIAL(info) << "\nend\n";
}

}
