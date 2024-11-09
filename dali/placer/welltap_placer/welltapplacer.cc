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
#include "welltapplacer.h"

#include <fstream>

namespace dali {

WellTapPlacer::WellTapPlacer(phydb::PhyDB *phy_db) {
  DaliExpects(phy_db != nullptr,
              "Cannot initialize a WellTapPlacer without providing a valid "
              "PhyDB pointer");
  phy_db_ = phy_db;
}

WellTapPlacer::~WellTapPlacer() {
  phy_db_ = nullptr;
  site_ptr_ = nullptr;
}

void WellTapPlacer::FetchRowsFromPhyDB() {
  auto &row_vec = phy_db_->GetRowVec();
  size_t sz = row_vec.size();
  if (sz == 0) return;
  rows_.reserve(sz);

  int site_id = row_vec[0].GetSiteId();
  auto &sites = phy_db_->GetTechPtr()->GetSitesRef();
  auto site_name = sites[site_id].GetName();
  for (auto &site : sites) {
    if (site.GetName() == site_name) {
      site_ptr_ = &site;
      break;
    }
  }

  row_height_ = (int)std::round(site_ptr_->GetHeight() *
                                phy_db_->design().GetUnitsDistanceMicrons());
  row_step_ = (int)std::round(site_ptr_->GetWidth() *
                              phy_db_->design().GetUnitsDistanceMicrons());

  DaliExpects(site_ptr_ != nullptr, "Cannot find Site in PhyDB");
  bottom_ = row_vec[0].GetOriginY();
  top_ = row_vec.back().GetOriginY() + row_height_;
  int prev_y = row_vec[0].GetOriginY();
  for (auto &phydb_row : row_vec) {
    int orig_x = phydb_row.GetOriginX();
    int orig_y = phydb_row.GetOriginY();
    DaliExpects(orig_y == prev_y,
                "Only support well-tap insertion for closely packed rows");
    prev_y = orig_y + row_height_;

    bool is_N = phydb_row.GetOrient() == phydb::CompOrient::N;
    Row &row = rows_.emplace_back();
    row.is_N = is_N;
    row.orig_x = orig_x;
    row.orig_y = orig_y;
    row.num_x = phydb_row.GetNumX();
    row.avail_sites.assign(row.num_x, true);
    row.well_taps.assign(row.num_x, false);

    left_ = std::min(orig_x, left_);
    right_ = std::max(orig_x + row.num_x * row_step_, right_);
  }
}

void WellTapPlacer::InitializeWhiteSpaceInRows() {
  if (rows_.empty()) return;
  for (auto &comp : phy_db_->GetDesignPtr()->GetComponentsRef()) {
    if (comp.GetPlacementStatus() != phydb::PlaceStatus::FIXED) continue;
    phydb::Macro *macro = comp.GetMacro();
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
      int start_col = StartCol(comp_lx, rows_[i].orig_x);
      int end_col = EndCol(comp_ux, rows_[i].orig_x);
      start_col = std::max(0, start_col);
      end_col = std::min((int)rows_[i].num_x - 1, end_col);
      for (int j = start_col; j <= end_col; ++j) {
        rows_[i].avail_sites[j] = false;
      }
    }
  }
}

void WellTapPlacer::SetCell(phydb::Macro *cell) {
  DaliExpects(cell != nullptr, "Cannot use nullptr as well-tap cell");
  cell_ = cell;
  cell_width_ = (int)std::ceil(
      cell->GetWidth() * phy_db_->GetDesignPtr()->GetUnitsDistanceMicrons() /
      row_step_);
}

void WellTapPlacer::SetCellInterval(double cell_interval_microns) {
  int unit_micron = phy_db_->GetDesignPtr()->GetUnitsDistanceMicrons();
  cell_interval_ =
      (int)std::floor(cell_interval_microns * unit_micron / row_step_);
  // std::cout << cell_interval_ << "\n";
}

void WellTapPlacer::SetCellMinDistanceToBoundary(
    double cell_min_distance_to_boundary_microns) {
  int unit_micron = phy_db_->GetDesignPtr()->GetUnitsDistanceMicrons();
  cell_min_distance_to_boundary_ = (int)std::ceil(
      cell_min_distance_to_boundary_microns * unit_micron / row_step_);
  // std::cout << cell_min_distance_to_boundary_ << "\n";
}

void WellTapPlacer::UseCheckerBoardMode(bool is_checker_board) {
  is_checker_board_ = is_checker_board;
}

void WellTapPlacer::AddWellTapToRowUniform(Row &row, int first_loc,
                                           int interval) {
  int lo_col = 0;
  int hi_col = -1;
  while (lo_col < row.num_x && hi_col < row.num_x) {
    // find the first available sites on the right hand side of the previous
    // high bound
    for (lo_col = hi_col + 1; lo_col < row.num_x; ++lo_col) {
      if (row.avail_sites[lo_col]) {
        break;
      }
    }
    // find the first unavailable sites on the right hand side of the low bound
    for (hi_col = lo_col + 1; hi_col < row.num_x; ++hi_col) {
      if (!row.avail_sites[hi_col]) {
        break;
      }
    }
    hi_col -= 1;

    // now we have a range of available sites [lo_col, hi_col]

    // we do a left->right scan to insert well tap cells for the first round
    int leftmost_tap_col = INT_MAX;
    int rightmost_tap_col = INT_MIN;
    int number_of_cell_created = 0;
    for (int i = lo_col; i <= hi_col; ++i) {
      int loc_x = row.orig_x + i * row_step_;
      if ((loc_x - first_loc) % (interval * row_step_) == 0) {
        row.well_taps[i] = true;
        leftmost_tap_col = std::min(leftmost_tap_col, i);
        rightmost_tap_col = std::max(rightmost_tap_col, i);
        ++number_of_cell_created;
      }
    }

    if (number_of_cell_created == 0) {
      // if no cell created, the distance must be smaller than cell_interval_
      // otherwise, there should be at least one
      if (hi_col - lo_col > interval / 2) {
        // add cells at both ends
        row.well_taps[lo_col] = true;
        row.well_taps[hi_col + 1 - cell_width_] = true;
      } else {
        // add cell at one end
        row.well_taps[lo_col] = true;
      }
    } else {
      if (leftmost_tap_col - lo_col > interval / 2) {
        // check if an extra cell is needed at left
        row.well_taps[lo_col] = true;
      }
      if (hi_col - rightmost_tap_col > interval / 2) {
        // check if an extra cell is needed at right
        row.well_taps[hi_col + 1 - cell_width_] = true;
      }
    }
  }
}

void WellTapPlacer::AddWellTapUniform() {
  int first_loc = ((cell_interval_ - cell_width_) / 2) * row_step_ + left_;
  for (auto &row : rows_) {
    AddWellTapToRowUniform(row, first_loc, cell_interval_);
  }
}

void WellTapPlacer::AddWellTapCheckerBoard() {
  // add well tap cell using half cell interval
  int half_cell_interval = cell_interval_ / 2;
  int first_loc = left_;
  for (auto &row : rows_) {
    AddWellTapToRowUniform(row, first_loc, half_cell_interval);
  }

  // trim redundant well tap cells
  // there is no need to trim the first and last row
  int tot_num_rows = (int)rows_.size();
  for (int r = 1; r < tot_num_rows - 1; ++r) {
    bool is_odd_row = r % 2 == 1;
    Row &cur_row = rows_[r];
    Row &prev_row = rows_[r - 1];
    Row &next_row = rows_[r + 1];
    for (int i = 0; i < cur_row.num_x; ++i) {
      if (cur_row.well_taps[i]) {
        if (i % half_cell_interval != 0) {
          if (prev_row.well_taps[i] && next_row.well_taps[i]) {
            cur_row.well_taps[i] = false;
          }
        } else {
          bool is_odd_cell = (i / half_cell_interval) % 2 == 1;
          if (is_odd_row == is_odd_cell) {
            if (prev_row.well_taps[i] && next_row.well_taps[i]) {
              cur_row.well_taps[i] = false;
            }
          }
        }
      }
    }
  }
}

void WellTapPlacer::AddWellTap() {
  if (is_checker_board_) {
    AddWellTapCheckerBoard();
  } else {
    AddWellTapUniform();
  }
}

void WellTapPlacer::ExportWellTapCellsToPhyDB() {
  if (rows_.empty()) return;
  int counter = 0;
  std::string macro_name = cell_->GetName();
  phydb::PlaceStatus place_status = phydb::PlaceStatus::FIXED;
  for (auto &row : rows_) {
    for (int i = 0; i < row.num_x; ++i) {
      if (row.well_taps[i]) {
        std::string welltap_cell_name = "welltap" + std::to_string(counter++);
        int llx = row.orig_x + i * row_step_;
        int lly = row.orig_y;
        phydb::CompOrient orient =
            row.is_N ? phydb::CompOrient::N : phydb::CompOrient::FS;
        phydb::Macro *macro_ptr = phy_db_->GetMacroPtr(macro_name);
        DaliExpects(macro_ptr != nullptr,
                    "Cannot find macro " << macro_name << " in PhyDB?!");
        phy_db_->AddComponent(welltap_cell_name, macro_ptr, place_status, llx,
                              lly, orient, phydb::CompSource::DIST);
      }
    }
  }
}

void WellTapPlacer::PlotAvailSpace() {
  std::ofstream ost("avail_space.txt");
  DaliExpects(ost.is_open(), "Cannot open output file: avail_space.txt");
  for (auto &row : rows_) {
    for (int i = 0; i < row.num_x; ++i) {
      if (!row.avail_sites[i]) continue;
      int lx = row.orig_x + i * row_step_;
      int ux = lx + row_step_;
      int ly = row.orig_y;
      int uy = row.orig_y + row_height_;
      ost << lx << "\t" << ux << "\t" << ux << "\t" << lx << "\t" << ly << "\t"
          << ly << "\t" << uy << "\t" << uy << "\t" << 0 << "\t" << 1 << "\t"
          << 1 << "\n";

      if (!row.well_taps[i]) continue;
      lx = row.orig_x + i * row_step_;
      ux = lx + 2 * row_step_;
      ly = row.orig_y;
      uy = row.orig_y + row_height_;
      ost << lx << "\t" << ux << "\t" << ux << "\t" << lx << "\t" << ly << "\t"
          << ly << "\t" << uy << "\t" << uy << "\t" << 1 << "\t" << 1 << "\t"
          << 1 << "\n";
    }
  }
}

}  // namespace dali
