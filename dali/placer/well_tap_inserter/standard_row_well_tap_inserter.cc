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
 * Inserts well taps into standard-cell rows.
 *
 * The standard-cell counterpart to the gridded flow's row completer: taps go in
 * at a pitch that keeps every cell within MaxPlugDist of one, into rows of fixed
 * height rather than rows sized by their contents.
 */
#include "dali/placer/well_tap_inserter/standard_row_well_tap_inserter.h"

#include <algorithm>
#include <climits>
#include <fstream>

namespace dali {

StandardRowWellTapInserter::StandardRowWellTapInserter(phydb::PhyDB* phy_db) {
  DaliExpects(phy_db != nullptr,
              "Cannot initialize standard-row well-tap insertion without a "
              "valid PhyDB pointer");
  phy_db_ = phy_db;
}

void StandardRowWellTapInserter::LoadRows() {
  auto& row_vec = phy_db_->GetRowVec();
  size_t sz = row_vec.size();
  if (sz == 0) return;

  rows_.clear();
  rows_.reserve(sz);
  left_ = INT_MAX;

  int site_id = row_vec[0].GetSiteId();
  auto& sites = phy_db_->GetTechPtr()->GetSitesRef();
  DaliExpects(site_id >= 0 && site_id < static_cast<int>(sites.size()),
              "PhyDB row references an invalid site");
  auto site_name = sites[site_id].GetName();
  phydb::Site* row_site = nullptr;
  for (auto& site : sites) {
    if (site.GetName() == site_name) {
      row_site = &site;
      break;
    }
  }

  DaliExpects(row_site != nullptr, "Cannot find row site in PhyDB");

  row_height_ = (int)std::round(row_site->GetHeight() *
                                phy_db_->design().GetUnitsDistanceMicrons());
  site_width_ = (int)std::round(row_site->GetWidth() *
                                phy_db_->design().GetUnitsDistanceMicrons());

  DaliExpects(row_height_ > 0 && site_width_ > 0,
              "PhyDB row site must have positive dimensions");
  bottom_ = row_vec[0].GetOriginY();
  int prev_y = row_vec[0].GetOriginY();
  for (auto& phydb_row : row_vec) {
    int origin_x = phydb_row.GetOriginX();
    int origin_y = phydb_row.GetOriginY();
    DaliExpects(origin_y == prev_y,
                "Only support well-tap insertion for closely packed rows");
    prev_y = origin_y + row_height_;

    bool is_orient_n = phydb_row.GetOrient() == phydb::CompOrient::N;
    WellTapRowSites& row = rows_.emplace_back();
    row.is_orient_n = is_orient_n;
    row.origin_x = origin_x;
    row.origin_y = origin_y;
    row.site_count = phydb_row.GetNumX();
    row.available_sites.assign(row.site_count, true);
    row.tap_starts.assign(row.site_count, false);

    left_ = std::min(origin_x, left_);
  }
}

void StandardRowWellTapInserter::MarkFixedComponentSites() {
  if (rows_.empty()) return;
  for (auto& comp : phy_db_->GetDesignPtr()->GetComponentsRef()) {
    if (comp.GetPlacementStatus() != phydb::PlaceStatus::FIXED) continue;
    phydb::Macro* macro = comp.GetMacro();
    int comp_width = (int)std::round(
        macro->GetWidth() * phy_db_->GetDesignPtr()->GetUnitsDistanceMicrons());
    int comp_height =
        (int)std::round(macro->GetHeight() *
                        phy_db_->GetDesignPtr()->GetUnitsDistanceMicrons());

    int comp_lx = comp.GetLocation().x;
    int comp_ly = comp.GetLocation().y;
    int comp_ux = comp_lx + comp_width;
    int comp_uy = comp_ly + comp_height;

    int start_row = StartRow(comp_ly);
    int end_row = EndRow(comp_uy);

    start_row = std::max(0, start_row);
    end_row = std::min((int)rows_.size() - 1, end_row);

    for (int i = start_row; i <= end_row; ++i) {
      int start_col = StartCol(comp_lx, rows_[i].origin_x);
      int end_col = EndCol(comp_ux, rows_[i].origin_x);
      start_col = std::max(0, start_col);
      end_col = std::min((int)rows_[i].site_count - 1, end_col);
      for (int j = start_col; j <= end_col; ++j) {
        rows_[i].available_sites[j] = false;
      }
    }
  }
}

void StandardRowWellTapInserter::SetTapMacro(phydb::Macro* well_tap_macro) {
  DaliExpects(well_tap_macro != nullptr,
              "Cannot use nullptr as well-tap macro");
  well_tap_macro_ = well_tap_macro;
  tap_width_in_sites_ = (int)std::ceil(
      well_tap_macro->GetWidth() *
      phy_db_->GetDesignPtr()->GetUnitsDistanceMicrons() / site_width_);
}

void StandardRowWellTapInserter::SetMaxTapInterval(
    double max_interval_microns) {
  int unit_micron = phy_db_->GetDesignPtr()->GetUnitsDistanceMicrons();
  tap_interval_in_sites_ =
      (int)std::floor(max_interval_microns * unit_micron / site_width_);
  DaliExpects(tap_interval_in_sites_ > 0,
              "Well-tap interval must cover at least one row site");
}

void StandardRowWellTapInserter::SetCheckerboardEnabled(
    bool checkerboard_enabled) {
  checkerboard_enabled_ = checkerboard_enabled;
}

void StandardRowWellTapInserter::InsertUniformTapsInRow(WellTapRowSites& row,
                                                        int first_loc,
                                                        int interval) {
  DaliExpects(interval > 0, "Well-tap insertion interval must be positive");
  int lo_col = 0;
  int hi_col = -1;
  while (lo_col < row.site_count && hi_col < row.site_count) {
    // find the first available sites on the right hand side of the previous
    // high bound
    for (lo_col = hi_col + 1; lo_col < row.site_count; ++lo_col) {
      if (row.available_sites[lo_col]) {
        break;
      }
    }
    if (lo_col >= row.site_count) {
      break;
    }
    // find the first unavailable sites on the right hand side of the low bound
    for (hi_col = lo_col + 1; hi_col < row.site_count; ++hi_col) {
      if (!row.available_sites[hi_col]) {
        break;
      }
    }
    hi_col -= 1;

    if (hi_col - lo_col + 1 < tap_width_in_sites_) {
      continue;
    }

    // now we have a range of available sites [lo_col, hi_col]

    // we do a left->right scan to insert well tap cells for the first round
    int leftmost_tap_col = INT_MAX;
    int rightmost_tap_col = INT_MIN;
    int number_of_taps_created = 0;
    for (int i = lo_col; i + tap_width_in_sites_ - 1 <= hi_col; ++i) {
      int loc_x = row.origin_x + i * site_width_;
      if ((loc_x - first_loc) % (interval * site_width_) == 0) {
        row.tap_starts[i] = true;
        leftmost_tap_col = std::min(leftmost_tap_col, i);
        rightmost_tap_col = std::max(rightmost_tap_col, i);
        ++number_of_taps_created;
      }
    }

    if (number_of_taps_created == 0) {
      // if no well tap created, the distance must be smaller than
      // tap_interval_in_sites_ otherwise, there should be at least one
      if (hi_col - lo_col > interval / 2) {
        // add well taps at both ends
        row.tap_starts[lo_col] = true;
        row.tap_starts[hi_col + 1 - tap_width_in_sites_] = true;
      } else {
        // add well tap at one end
        row.tap_starts[lo_col] = true;
      }
    } else {
      if (leftmost_tap_col - lo_col > interval / 2) {
        // check if an extra well tap is needed at left
        row.tap_starts[lo_col] = true;
      }
      if (hi_col - rightmost_tap_col > interval / 2) {
        // check if an extra well tap is needed at right
        row.tap_starts[hi_col + 1 - tap_width_in_sites_] = true;
      }
    }
  }
}

bool StandardRowWellTapInserter::HasTapAtX(const WellTapRowSites& row,
                                           int x) const {
  int relative_x = x - row.origin_x;
  if (relative_x < 0 || relative_x % site_width_ != 0) {
    return false;
  }
  int column = relative_x / site_width_;
  return column < row.site_count && row.tap_starts[column];
}

void StandardRowWellTapInserter::InsertUniformTaps() {
  int first_loc =
      ((tap_interval_in_sites_ - tap_width_in_sites_) / 2) * site_width_ +
      left_;
  for (auto& row : rows_) {
    InsertUniformTapsInRow(row, first_loc, tap_interval_in_sites_);
  }
}

void StandardRowWellTapInserter::InsertCheckerboardTaps() {
  // add well tap macro using half well-tap interval
  int half_tap_interval = tap_interval_in_sites_ / 2;
  DaliExpects(half_tap_interval > 0,
              "Checkerboard insertion requires at least a two-site interval");
  int first_loc = left_;
  for (auto& row : rows_) {
    InsertUniformTapsInRow(row, first_loc, half_tap_interval);
  }

  // trim redundant well taps
  // there is no need to trim the first and last row
  int tot_num_rows = (int)rows_.size();
  for (int r = 1; r < tot_num_rows - 1; ++r) {
    bool is_odd_row = r % 2 == 1;
    WellTapRowSites& cur_row = rows_[r];
    WellTapRowSites& prev_row = rows_[r - 1];
    WellTapRowSites& next_row = rows_[r + 1];
    for (int i = 0; i < cur_row.site_count; ++i) {
      if (cur_row.tap_starts[i]) {
        int tap_x = cur_row.origin_x + i * site_width_;
        int global_site = (tap_x - left_) / site_width_;
        if (global_site % half_tap_interval != 0) {
          if (HasTapAtX(prev_row, tap_x) && HasTapAtX(next_row, tap_x)) {
            cur_row.tap_starts[i] = false;
          }
        } else {
          bool is_odd_tap = (global_site / half_tap_interval) % 2 == 1;
          if (is_odd_row == is_odd_tap) {
            if (HasTapAtX(prev_row, tap_x) && HasTapAtX(next_row, tap_x)) {
              cur_row.tap_starts[i] = false;
            }
          }
        }
      }
    }
  }
}

void StandardRowWellTapInserter::InsertTaps() {
  DaliExpects(well_tap_macro_ != nullptr,
              "Well-tap insertion requires a tap macro");
  DaliExpects(tap_interval_in_sites_ > 0,
              "Well-tap insertion requires a positive interval");
  if (checkerboard_enabled_) {
    InsertCheckerboardTaps();
  } else {
    InsertUniformTaps();
  }
}

void StandardRowWellTapInserter::ExportToPhyDB() {
  if (rows_.empty()) return;
  int counter = 0;
  std::string macro_name = well_tap_macro_->GetName();
  phydb::PlaceStatus place_status = phydb::PlaceStatus::FIXED;
  for (auto& row : rows_) {
    for (int i = 0; i < row.site_count; ++i) {
      if (row.tap_starts[i]) {
        std::string welltap_cell_name = "welltap" + std::to_string(counter++);
        int llx = row.origin_x + i * site_width_;
        int lly = row.origin_y;
        phydb::CompOrient orient =
            row.is_orient_n ? phydb::CompOrient::N : phydb::CompOrient::FS;
        phydb::Macro* macro_ptr = phy_db_->GetMacroPtr(macro_name);
        DaliExpects(macro_ptr != nullptr,
                    "Cannot find macro " << macro_name << " in PhyDB?!");
        phy_db_->AddComponent(welltap_cell_name, macro_ptr, place_status, llx,
                              lly, orient, phydb::CompSource::DIST);
      }
    }
  }
}

void StandardRowWellTapInserter::DumpAvailableSites() {
  std::ofstream ost("avail_space.txt");
  DaliExpects(ost.is_open(), "Cannot open output file: avail_space.txt");
  for (auto& row : rows_) {
    for (int i = 0; i < row.site_count; ++i) {
      if (!row.available_sites[i]) continue;
      int lx = row.origin_x + i * site_width_;
      int ux = lx + site_width_;
      int ly = row.origin_y;
      int uy = row.origin_y + row_height_;
      ost << lx << "\t" << ux << "\t" << ux << "\t" << lx << "\t" << ly << "\t"
          << ly << "\t" << uy << "\t" << uy << "\t" << 0 << "\t" << 1 << "\t"
          << 1 << "\n";

      if (!row.tap_starts[i]) continue;
      lx = row.origin_x + i * site_width_;
      ux = lx + 2 * site_width_;
      ly = row.origin_y;
      uy = row.origin_y + row_height_;
      ost << lx << "\t" << ux << "\t" << ux << "\t" << lx << "\t" << ly << "\t"
          << ly << "\t" << uy << "\t" << uy << "\t" << 1 << "\t" << 1 << "\t"
          << 1 << "\n";
    }
  }
}

}  // namespace dali
