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
#ifndef DALI_PLACER_GLOBAL_PLACER_BOXBIN_H_
#define DALI_PLACER_GLOBAL_PLACER_BOXBIN_H_

#include <fstream>
#include <iostream>
#include <vector>

#include "dali/circuit/block.h"
#include "dali/circuit/placement_blockage.h"
#include "dali/placer/global_placer/cell_cut_point.h"
#include "dali/placer/global_placer/grid_bin.h"

namespace dali {

/** Candidate rectangular window in grid-bin coordinates. */
struct WindowQuadruple {
  int llx, lly, urx, ury;

  /** Return true when this window fully covers rhs. */
  bool Cover(WindowQuadruple const& rhs) const {
    return llx <= rhs.llx && lly <= rhs.lly && urx >= rhs.urx && ury >= rhs.ury;
  }

  /** Return true when the window aspect ratio is within [lo, hi]. */
  bool AspectRatioInRange(double lo, double hi) const {
    double ratio = (ury - lly + 1) / double(urx - llx + 1);
    return ratio <= hi && ratio >= lo;
  }
};

/** Recursive bisection box used by look-ahead legalization. */
class BoxBin {
 public:
  BoxBin();
  bool cut_direction_x;
  // Cut line is along the x direction.
  bool all_terminal;
  unsigned long long total_white_space;
  double filling_rate;
  bool IsAllFixedBlk() const { return all_terminal; };
  /* Cut-line to split box white space. */
  GridBinIndex ll_index;
  GridBinIndex ur_index;
  GridBinIndex cut_ll_index;
  GridBinIndex cut_ur_index;
  /* Cut-line to split cell area. */
  CellCutPoint ll_point;
  CellCutPoint ur_point;
  CellCutPoint cut_ll_point;
  CellCutPoint cut_ur_point;
  /* Total cell area, and the values in two child boxes. */
  unsigned long long total_cell_area;
  unsigned long long total_cell_area_low;
  unsigned long long total_cell_area_high;

  /* Cell ids in the box and in the two child boxes. */
  std::vector<Block*> cell_list;
  std::vector<Block*> cell_list_low;
  std::vector<Block*> cell_list_high;

  /* The cell_id for terminals in the box, will be updated only when the box is
   * a GridBin if there is no terminal in the grid bin, do not have to further
   * split the box into smaller boxs, otherwise split the box into smaller
   * boxes, until there is no terminals in any boxes*/
  std::vector<const PlacementBlockage*> placement_blockages_;

  /** Copy placement blockages from the matching grid bin. */
  void UpdatePlacementBlockages(
      std::vector<std::vector<GridBin>>& grid_bin_matrix) {
    placement_blockages_ =
        grid_bin_matrix[ll_index.x][ll_index.y].placement_blockages_;
  };
  /* UpdatePlacementBlockages can only be called when the box is a grid_bin_box.
   */
  bool HasPlacementBlockages() const { return !placement_blockages_.empty(); };

  std::vector<int> vertical_cutlines;
  std::vector<int> horizontal_cutlines;

  /** Recompute obstacle boundary cutline candidates. */
  void UpdateObsBoundary();

  /** Return true when horizontal cutline candidates dominate. */
  bool IsMoreHorizontalCutlines() const;

  /* If the box is smaller than a grid bin, these placement-region boundaries
   * define where cells will be placed. */
  int left;
  int right;
  int bottom;
  int top;

  /** Update boundaries from the grid-bin matrix. */
  void UpdateBoundaries(std::vector<std::vector<GridBin>>& grid_bin_matrix);

  /* UpdateWhiteSpaceAndFixedBlocks can only be called after boundaries are set.
   */
  void UpdateWhiteSpaceAndFixedBlocks(
      std::vector<const PlacementBlockage*>& placement_blockages);

  void update_all_terminal(std::vector<std::vector<GridBin>>& grid_bin_matrix);
  void update_cell_area();
  void update_cell_area_white_space(
      std::vector<std::vector<GridBin>>& grid_bin_matrix);
  void UpdateCellAreaWhiteSpaceFillingRate(
      std::vector<std::vector<unsigned long long>>& grid_bin_white_space_LUT,
      std::vector<std::vector<GridBin>>& grid_bin_matrix);
  void ExpandBox(int grid_cnt_x, int grid_cnt_y);
  bool write_box_boundary(std::string const& NameOfFile);
  bool write_cell_region(
      std::string const& NameOfFile = "first_cell_bounding_box.txt");
  static unsigned long long white_space_LUT(
      std::vector<std::vector<unsigned long long>>& grid_bin_white_space_LUT,
      GridBinIndex& ll, GridBinIndex& ur);
  void UpdateCellList(std::vector<std::vector<GridBin>>& grid_bin_matrix);
  bool write_cell_in_box(std::string const& NameOfFile);
  bool update_cut_index_white_space(
      std::vector<std::vector<unsigned long long>>& grid_bin_white_space_LUT);
  bool update_cut_point_cell_list_low_high(
      unsigned long long& box1_total_white_space,
      unsigned long long& box2_total_white_space);
  bool update_cut_point_cell_list_low_high_leaf(int& cut_line_w,
                                                int ave_blk_height);

  void Report();
};

}  // namespace dali

#endif  // DALI_PLACER_GLOBAL_PLACER_BOXBIN_H_
