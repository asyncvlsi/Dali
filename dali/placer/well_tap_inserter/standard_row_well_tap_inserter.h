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
#ifndef DALI_PLACER_WELL_TAP_INSERTER_STANDARD_ROW_WELL_TAP_INSERTER_H_
#define DALI_PLACER_WELL_TAP_INSERTER_STANDARD_ROW_WELL_TAP_INSERTER_H_

#include <phydb/phydb.h>

#include <climits>
#include <list>
#include <vector>

#include "dali/circuit/macro.h"
#include "dali/common/misc.h"

namespace dali {

/** Row occupancy and orientation data used for well-tap insertion. */
struct WellTapRowSites {
  int origin_x = 0;
  int origin_y = 0;
  int site_count = 0;
  std::vector<bool> available_sites;
  std::vector<bool> tap_starts;
  bool is_orient_n = true;
};

/** Inserts well-tap cells into available PhyDB row sites. */
class StandardRowWellTapInserter {
  phydb::PhyDB* phy_db_ = nullptr;
  int bottom_ = 0;
  int left_ = INT_MAX;
  int row_height_ = 0;
  int site_width_ = 0;

  std::vector<WellTapRowSites> rows_;  // white space in each row

  phydb::Macro* well_tap_macro_ = nullptr;
  int tap_width_in_sites_ = -1;
  int tap_interval_in_sites_ = -1;
  bool checkerboard_enabled_ = true;

 public:
  /** Inserts well taps into standard-cell rows at a covering pitch. */
  explicit StandardRowWellTapInserter(phydb::PhyDB* phy_db);

  /** Load row/site data from PhyDB. */
  void LoadRows();

  /** Initialize available whitespace in each row. */
  void MarkFixedComponentSites();

  /** Set the macro used for inserted well taps. */
  void SetTapMacro(phydb::Macro* well_tap_macro);

  /** Set maximum same-row well-tap spacing in microns. */
  void SetMaxTapInterval(double tap_interval_in_sites_microns);

  /** Enable or disable checkerboard insertion. */
  void SetCheckerboardEnabled(bool checkerboard_enabled);

  /** Insert configured taps into available standard-row sites. */
  void InsertTaps();

  /** Export inserted well taps back to PhyDB. */
  void ExportToPhyDB();

  /** Dump available row sites for explicit debugging. */
  void DumpAvailableSites();

 private:
  /** Add uniformly spaced well taps to one row. */
  void InsertUniformTapsInRow(WellTapRowSites& row, int first_loc,
                              int interval);

  /** Add uniformly spaced well taps to every row. */
  void InsertUniformTaps();

  /** Add well taps in checkerboard mode. */
  void InsertCheckerboardTaps();

  /** Return true when a row contains a tap starting at physical x. */
  bool HasTapAtX(const WellTapRowSites& row, int x) const;

  int StartRow(int y_loc) const { return (y_loc - bottom_) / row_height_; }
  int EndRow(int y_loc) const {
    int relative_y = y_loc - bottom_;
    int res = relative_y / row_height_;
    if (relative_y % row_height_ == 0) {
      --res;
    }
    return res;
  }
  int StartCol(int x_loc, int origin_x) const {
    return (x_loc - origin_x) / site_width_;
  }
  int EndCol(int x_loc, int origin_x) const {
    int relative_x = x_loc - origin_x;
    int res = relative_x / site_width_;
    if (relative_x % site_width_ == 0) {
      --res;
    }
    return res;
  }
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_TAP_INSERTER_STANDARD_ROW_WELL_TAP_INSERTER_H_
