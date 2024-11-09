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
#ifndef DALI_PLACER_WELLTAPPLACER_WELLTAPPLACER_H_
#define DALI_PLACER_WELLTAPPLACER_WELLTAPPLACER_H_

#include <phydb/phydb.h>

#include <list>
#include <vector>

#include "dali/circuit/block_type.h"
#include "dali/common/misc.h"

namespace dali {

/**
 * A structure to define properties of a row for well-tap cell insertion
 * 1. white space segment
 * 2. orientation, N or FS
 * 3. lower left location of all well-tap cells
 */
struct Row {
  int orig_x = 0;
  int orig_y = 0;
  int num_x = 0;
  std::vector<bool> avail_sites;  // white space segments
  std::vector<bool> well_taps;

  bool is_N = true;  // orientation
};

/**
 * A class for inserting well-tap cells.
 * List of parameters:
 * 1. cell_: the kind of well-tap cell to use.
 * 2. cell_interval: the maximum distance from the center of one well-tap cell
 * to the center of the following well-tap cell in the same row. Unit is core
 * site width.
 * 3. is_checker_board: placing well-tap cells in checker_board mode or not.
 */
class WellTapPlacer {
  phydb::PhyDB *phy_db_ = nullptr;
  phydb::Site *site_ptr_ = nullptr;
  int top_ = 0;
  int bottom_ = 0;
  int left_ = INT_MAX;
  int right_ = INT_MIN;
  int row_height_ = 0;
  int row_step_ = 0;

  std::vector<Row> rows_;  // white space in each row

  phydb::Macro *cell_ = nullptr;            // pointer to well-tap cell
  int cell_width_ = -1;                     // unit is row_step_
  int cell_interval_ = -1;                  // unit is row_step_
  int cell_min_distance_to_boundary_ = -1;  // unit is row_step_
  bool is_checker_board_ = true;

 public:
  explicit WellTapPlacer(phydb::PhyDB *phy_db);
  ~WellTapPlacer();
  void FetchRowsFromPhyDB();
  void InitializeWhiteSpaceInRows();
  void SetCell(phydb::Macro *cell);
  void SetCellInterval(double cell_interval_microns);
  void SetCellMinDistanceToBoundary(
      double cell_min_distance_to_boundary_microns);
  void UseCheckerBoardMode(bool is_checker_board);
  void AddWellTapToRowUniform(Row &row, int first_loc, int interval);
  void AddWellTapUniform();
  void AddWellTapCheckerBoard();
  void AddWellTap();
  void ExportWellTapCellsToPhyDB();

  void PlotAvailSpace();

  int StartRow(int y_loc) { return (y_loc - bottom_) / row_height_; }
  int EndRow(int y_loc) {
    int relative_y = y_loc - bottom_;
    int res = relative_y / row_height_;
    if (relative_y % row_height_ == 0) {
      --res;
    }
    return res;
  }
  int StartCol(int x_loc, int orig_x) { return (x_loc - orig_x) / row_step_; }
  int EndCol(int x_loc, int orig_x) {
    int relative_x = x_loc - orig_x;
    int res = relative_x / row_step_;
    if (relative_x % row_step_ == 0) {
      --res;
    }
    return res;
  }
};

}  // namespace dali

#endif  // DALI_PLACER_WELLTAPPLACER_WELLTAPPLACER_H_
