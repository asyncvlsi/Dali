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
 * Fallback legalizer, used when the standard-cell legalizer fails.
 *
 * Places cells one at a time into the nearest row with room, in the manner of
 * Tetris. That greedy order makes it robust -- it will nearly always produce
 * something legal -- but worse than the legalizer it backs up, so it is a
 * safety net rather than an alternative.
 */
#include "extended_tetris_legalizer.h"

#include <algorithm>
#include <cfloat>
#include <climits>
#include <list>

#include "dali/common/helper.h"
#include "dali/common/misc.h"

namespace dali {

constexpr int kMaxLegalizationFailureExamples = 5;

static void LogLegalizationFailureExample(const char* pass_name,
                                          size_t iteration, int example_id,
                                          const Component& component,
                                          const Value2D<int>& target_loc,
                                          int search_start_row,
                                          int search_end_row, int total_rows,
                                          int region_left, int region_right) {
  LOG(warning) << "ExtendedTetris legalization could not place component "
               << component.Name() << " during " << pass_name << " pass"
               << " (iteration " << iteration << ", example " << example_id
               << ")\n"
               << "  reason: no legal row segment was found in the expanded "
                  "search window\n"
               << "  component size(grid): " << component.Width() << " x "
               << component.Height() << "\n"
               << "  original box(grid): (" << component.LLX() << ", "
               << component.LLY() << ") - (" << component.URX() << ", "
               << component.URY() << ")\n"
               << "  candidate loc(grid): (" << target_loc.x << ", "
               << target_loc.y << ")\n"
               << "  search rows: [" << search_start_row << ", "
               << search_end_row << "], total rows: " << total_rows << "\n"
               << "  placement x-boundary(grid): [" << region_left << ", "
               << region_right << "]\n";
}

static void LogLegalizationFailureSummary(const char* pass_name,
                                          size_t iteration,
                                          int failed_component_count,
                                          size_t checked_component_count) {
  LOG(warning) << "ExtendedTetris legalization " << pass_name
               << " pass failed (iteration " << iteration
               << "): " << failed_component_count << " of "
               << checked_component_count
               << " movable components could not be assigned to legal row "
                  "space.\n"
               << "  The local search window expands on each retry; this "
                  "usually means the remaining row whitespace, fixed "
                  "blockages, or target density cannot fit those components "
                  "within the current legalizer settings.\n";
}

ExtendedTetrisLegalizer::ExtendedTetrisLegalizer()
    : Placer(),
      row_height_(0),
      row_height_set_(false),
      legalize_from_left_(true),
      cur_iter_(0),
      max_iter_(20),
      tot_num_rows_(0) {}

void ExtendedTetrisLegalizer::SetRowHeight(int row_height) {
  DaliExpects(row_height > 0, "Cannot set negative row height!");
  row_height_ = row_height;
  row_height_set_ = true;
}

void ExtendedTetrisLegalizer::SetMaxIteration(size_t max_iter) {
  max_iter_ = max_iter;
}

void ExtendedTetrisLegalizer::SetWidthHeightFactor(double k_width,
                                                   double k_height) {
  k_width_ = k_width;
  k_height_ = k_height;
}

void ExtendedTetrisLegalizer::SetLeftBoundFactor(double k_left,
                                                 double k_left_step) {
  k_left_ = k_left;
  k_left_step_ = k_left_step;
}

void ExtendedTetrisLegalizer::InitializeFromGriddedRowLegalizer(
    GriddedRowLegalizer* grlg) {
  DaliExpects(grlg != nullptr,
              "Cannot initialize ExtendedTetrisLegalizer from a nullptr "
              "GriddedRowLegalizer");
  // circuit
  ckt_ptr_ = grlg->ckt_ptr_;

  // rows info
  auto& stripe = grlg->col_list_[0].stripe_list_[0];
  row_height_ = stripe.row_height_;
  tot_num_rows_ = static_cast<int>(stripe.gridded_rows_.size());
  is_first_row_N_ = stripe.gridded_rows_[0].IsOrientN();

  // placement boundary
  left_ = stripe.LLX();
  right_ = stripe.URX();
  bottom_ = stripe.LLY();
  top_ = stripe.URY();

  // rows
  rows_.clear();
  rows_.resize(tot_num_rows_);
  for (int i = 0; i < tot_num_rows_; ++i) {
    auto& row = stripe.gridded_rows_[i];
    for (auto& seg : row.Segments()) {
      rows_[i].emplace_back(seg.LLX(), seg.URX());
    }
  }

  component_contour_.clear();
  component_contour_.resize(tot_num_rows_, left_);

  ComponentInitialLocation tmp_index_loc_pair(nullptr, 0, 0);
  component_initial_locations_.clear();
  component_initial_locations_.resize(ckt_ptr_->Components().size(),
                                      tmp_index_loc_pair);
}

void ExtendedTetrisLegalizer::SetRowInfoAuto() {
  if (!row_height_set_) {
    if (!ckt_ptr_->design().Components().empty()) {
      row_height_ = ckt_ptr_->design().Components()[0].Height();
    } else {
      row_height_ = ckt_ptr_->RowHeightGridUnit();
    }
  }
  tot_num_rows_ = (top_ - bottom_) / row_height_;
  component_contour_.resize(tot_num_rows_, left_);
}

void ExtendedTetrisLegalizer::DetectWhiteSpace() {
  std::vector<std::vector<SegI>> macro_segments;
  macro_segments.resize(tot_num_rows_);

  // find all placement blockages
  auto& placement_blockages = ckt_ptr_->design().PlacementBlockages();

  for (auto& blockage : placement_blockages) {
    auto& rect = blockage.GetRect();
    int lx = rect.LLX();
    int ly = rect.LLY();
    int ux = rect.URX();
    int uy = rect.URY();

    bool out_of_range = (ly >= RegionTop()) || (uy <= RegionBottom()) ||
                        (lx >= RegionRight()) || (ux <= RegionLeft());

    if (out_of_range) {
      continue;
    }

    int start_row = StartRow(ly);
    int end_row = EndRow(uy);

    start_row = std::max(0, start_row);
    end_row = std::min(tot_num_rows_ - 1, end_row);

    Seg tmp(0, 0);
    tmp.lo = std::max(RegionLeft(), lx);
    tmp.hi = std::min(RegionRight(), ux);
    if (tmp.hi > tmp.lo) {
      for (int i = start_row; i <= end_row; ++i) {
        macro_segments[i].push_back(tmp);
      }
    }
  }
  for (auto& intervals : macro_segments) {
    MergeIntervals(intervals);
  }

  std::vector<std::vector<int>> intermediate_seg_rows;
  intermediate_seg_rows.resize(tot_num_rows_);
  for (int i = 0; i < tot_num_rows_; ++i) {
    if (macro_segments[i].empty()) {
      intermediate_seg_rows[i].push_back(left_);
      intermediate_seg_rows[i].push_back(right_);
      continue;
    }
    int segments_size = int(macro_segments[i].size());
    for (int j = 0; j < segments_size; ++j) {
      auto& interval = macro_segments[i][j];
      if (interval.lo == left_ && interval.hi < RegionRight()) {
        intermediate_seg_rows[i].push_back(interval.hi);
      }

      if (interval.lo > left_) {
        if (intermediate_seg_rows[i].empty()) {
          intermediate_seg_rows[i].push_back(left_);
        }
        intermediate_seg_rows[i].push_back(interval.lo);
        if (interval.hi < RegionRight()) {
          intermediate_seg_rows[i].push_back(interval.hi);
        }
      }
    }
    if (intermediate_seg_rows[i].size() % 2 == 1) {
      intermediate_seg_rows[i].push_back(right_);
    }
  }

  rows_.resize(tot_num_rows_);
  int min_component_width = int(ckt_ptr_->MinComponentWidth());
  for (int i = 0; i < tot_num_rows_; ++i) {
    int len = int(intermediate_seg_rows[i].size());
    rows_[i].reserve(len / 2);
    for (int j = 0; j < len; j += 2) {
      if (intermediate_seg_rows[i][j + 1] - intermediate_seg_rows[i][j] >=
          min_component_width) {
        rows_[i].emplace_back(intermediate_seg_rows[i][j],
                              intermediate_seg_rows[i][j + 1]);
      }
    }
  }
}

void ExtendedTetrisLegalizer::InitIndexLocList() {
  ComponentInitialLocation tmp_index_loc_pair(nullptr, 0, 0);
  component_initial_locations_.resize(ckt_ptr_->Components().size(),
                                      tmp_index_loc_pair);
}

/****
 * 1. calculate the number of rows for a given row_height
 * 2. initialize white space available in rows
 * 3. initialize component contour to be the left contour
 * 4. allocate space for index_loc_list_
 * ****/
void ExtendedTetrisLegalizer::InitLegalizer() {
  SetRowInfoAuto();
  DetectWhiteSpace();
  InitIndexLocList();
}

int ExtendedTetrisLegalizer::RowHeight() const { return row_height_; }

int ExtendedTetrisLegalizer::StartRow(int y_loc) const {
  return (y_loc - bottom_) / row_height_;
}

int ExtendedTetrisLegalizer::EndRow(int y_loc) const {
  int relative_y = y_loc - bottom_;
  int res = relative_y / row_height_;
  if (relative_y % row_height_ == 0) {
    --res;
  }
  return res;
}

int ExtendedTetrisLegalizer::MaxRow(int height) const {
  return ((top_ - height) - bottom_) / row_height_;
}

int ExtendedTetrisLegalizer::HeightToRow(int height) const {
  return std::ceil(height / double(row_height_));
}

int ExtendedTetrisLegalizer::LocToRow(int y_loc) const {
  return (y_loc - bottom_) / row_height_;
}

int ExtendedTetrisLegalizer::RowToLoc(int row_num, int displacement) const {
  return row_num * row_height_ + bottom_ + displacement;
}

int ExtendedTetrisLegalizer::AlignLocToRowLoc(double y_loc) const {
  int row_num = static_cast<int>(std::round((y_loc - bottom_) / row_height_));
  if (row_num < 0) row_num = 0;
  if (row_num >= tot_num_rows_) row_num = tot_num_rows_ - 1;
  return row_num * row_height_ + bottom_;
}

/****
 * This member function checks if the region specified by [lo_x, hi_x] and
 * [lo_row, hi_row] is legal or not
 *
 * If this space overlaps with fixed macros or out of placement range, then this
 * space is illegal.
 *
 * To determine this space is legal, one just need to show that every row is
 * legal
 * ****/
bool ExtendedTetrisLegalizer::IsSpaceLegal(int lo_x, int hi_x, int lo_row,
                                           int hi_row) const {
  assert(lo_x <= hi_x);
  assert(lo_row <= hi_row);

  bool loc_out_range = (hi_x > right_) || (lo_x < left_) ||
                       (hi_row >= tot_num_rows_) || (lo_row < 0);
  if (loc_out_range) {
    return false;
  }

  bool is_tmp_row_legal;
  bool is_partial_cover_lo;
  bool is_partial_cover_hi;
  bool is_before_seg;
  int seg_count = 0;

  bool is_all_row_legal = true;
  for (int i = lo_row; i <= hi_row; ++i) {
    seg_count = rows_[i].size();
    is_tmp_row_legal = false;
    for (int j = 0; j < seg_count; ++j) {
      if (rows_[i][j].lo <= lo_x && rows_[i][j].hi >= hi_x) {
        is_tmp_row_legal = true;
        break;
      }

      is_partial_cover_lo = rows_[i][j].lo > lo_x && rows_[i][j].lo < hi_x;
      is_partial_cover_hi = rows_[i][j].hi > lo_x && rows_[i][j].hi < hi_x;
      is_before_seg = rows_[i][j].lo >= hi_x;
      if (is_partial_cover_lo || is_partial_cover_hi || is_before_seg) {
        break;
      }
    }

    if (!is_tmp_row_legal) {
      is_all_row_legal = false;
      break;
    }
  }
  return is_all_row_legal;
}

bool ExtendedTetrisLegalizer::IsFitToRow(int row_id,
                                         Component& component) const {
  if (component.MacroPtr()->HasWellInfo()) {
    // if there is no well_ptr, we can assume it is a standard cell design
    return true;
  }
  int region_cnt = component.MacroPtr()->RegionCount();
  if (region_cnt & 1) {  // odd region_cnt can be placed into any rows
    return true;
  }
  // even region_cnt can only be placed into every other row
  bool is_gnd_bottom = component.MacroPtr()->IsNwellAbovePwell(0);
  bool is_row_even = !(row_id & 1);
  bool is_row_N =
      (is_row_even && is_first_row_N_) || (!is_row_even && !is_first_row_N_);

  return is_row_N == is_gnd_bottom;
}

bool ExtendedTetrisLegalizer::ShouldOrientN(int row_id,
                                            Component& component) const {
  // if cell flip is disabled, then cell orientation is always N
  if (disable_cell_flip_) {
    return true;
  }

  bool is_gnd_bottom = true;
  if (component.MacroPtr()->HasWellInfo()) {
    // if there is no well_ptr, we can assume it is a standard cell design
    is_gnd_bottom = true;
  } else {
    is_gnd_bottom = component.MacroPtr()->IsNwellAbovePwell(0);
  }
  bool is_row_even = !(row_id & 1);
  bool is_row_N =
      (is_row_even && is_first_row_N_) || (!is_row_even && !is_first_row_N_);

  return ((is_row_N && is_gnd_bottom) || (!is_row_N && !is_gnd_bottom));
}

void ExtendedTetrisLegalizer::InitComponentContourForward() {
  component_contour_.assign(component_contour_.size(), left_);
}

void ExtendedTetrisLegalizer::InitAndSortComponentAscendingX() {
  component_initial_locations_.clear();
  auto& components = ckt_ptr_->Components();
  for (auto& component : components) {
    // skipp dummy components and fixed components
    if (IsDummyComponent(component)) continue;
    if (component.IsFixed()) continue;
    double x_loc = component.LLX() - k_width_ * component.Width() -
                   k_height_ * component.Height();
    double y_loc = component.LLY();
    component_initial_locations_.emplace_back(&component, x_loc, y_loc);
  }

  std::sort(component_initial_locations_.begin(),
            component_initial_locations_.end(),
            [](const ComponentInitialLocation& pair0,
               const ComponentInitialLocation& pair1) {
              return (pair0.x < pair1.x) ||
                     ((pair0.x == pair1.x) && (pair0.y < pair1.y));
            });
}

/****
 * Mark the space used by this component by changing the start point of
 * available space in each related row
 * ****/
void ExtendedTetrisLegalizer::UseSpaceLeft(Component const& component) {
  int start_row = StartRow(int(component.LLY()));
  int end_row = EndRow(int(component.URY()));

  DaliExpects(component.URY() <= RegionTop(), "Out of bound?");
  DaliExpects(end_row < tot_num_rows_, "Out of bound?");
  DaliExpects(start_row >= 0, "Out of bound?");

  int end_x = int(component.URX());
  for (int i = start_row; i <= end_row; ++i) {
    component_contour_[i] = end_x;
  }
}

/****
 * Returns whether the current location is legal
 * 1. if this component matches this row
 * 2. if the space itself is illegal, then return false
 * 3. if the space covers placed components, then return false
 * 4. otherwise, return true
 * ****/
bool ExtendedTetrisLegalizer::IsCurrentLocLegalLeft(Value2D<int>& loc,
                                                    Component& component) {
  int start_row = StartRow(loc.y);
  int end_row = EndRow(loc.y + component.Height());

  // can this component fit this row?
  if (!IsFitToRow(start_row, component)) {
    return false;
  }

  // is this location legal?
  bool is_space_legal =
      IsSpaceLegal(loc.x, loc.x + component.Width(), start_row, end_row);
  if (!is_space_legal) {
    return false;
  }

  // is space not occupied by other cells?
  for (int i = start_row; i <= end_row; ++i) {
    if (component_contour_[i] > loc.x) {
      return false;
    }
  }

  return true;
}

/****
 * Returns the left boundary of the white space region where this component
 * should be placed
 *
 * For each row, find the segment which is closest to [lo_x, hi_x]
 * If a segment is [lo_seg, hi_seg], the distance is defined as
 *        min(|lo_seg - lo_x| + |lo_seg - hi_x|, |hi_seg - lo_x| + |hi_seg -
 * hi_x|)
 * ****/
int ExtendedTetrisLegalizer::WhiteSpaceBoundLeft(int lo_x, int hi_x, int lo_row,
                                                 int hi_row) {
  int white_space_bound = left_;

  int min_distance = INT_MAX;
  int tmp_bound;

  for (int i = lo_row; i <= hi_row; ++i) {
    tmp_bound = left_;
    for (auto& seg : rows_[i]) {
      if (seg.lo <= lo_x && seg.hi >= hi_x) {
        tmp_bound = seg.lo;
        min_distance = 0;
        break;
      }
      int tmp_distance = std::min(abs(seg.lo - lo_x) + abs(seg.lo - hi_x),
                                  abs(seg.hi - lo_x) + abs(seg.hi - hi_x));
      if (tmp_distance < min_distance) {
        tmp_bound = seg.lo;
        min_distance = tmp_distance;
      }
    }
    white_space_bound = std::max(white_space_bound, tmp_bound);
  }

  return white_space_bound;
}

/****
 * Returns whether a legal location can be found, and put the final location to
 * @params loc
 * ****/
bool ExtendedTetrisLegalizer::FindLocLeft(Value2D<int>& loc,
                                          Component& component) {
  int width = component.Width();
  int height = component.Height();

  int left_component_bound =
      static_cast<int>(std::round(loc.x - k_left_ * width));
  int max_search_row = MaxRow(height);
  int component_row_height = HeightToRow(height);

  int lower_search_y = static_cast<int>(std::round(loc.y - k_start * height));
  int upper_search_y = static_cast<int>(std::round(loc.y + k_end * height));
  int search_start_row = std::max(0, LocToRow(lower_search_y));
  int search_end_row = std::min(max_search_row, LocToRow(upper_search_y));

  int best_row = 0;
  int best_loc_x = INT_MIN;
  double min_cost = DBL_MAX;

  for (int tmp_start_row = search_start_row; tmp_start_row <= search_end_row;
       ++tmp_start_row) {
    int tmp_end_row = tmp_start_row + component_row_height - 1;
    bool is_fit_to_row = IsFitToRow(tmp_start_row, component);
    if (!is_fit_to_row) {
      continue;
    }
    int left_white_space_bound =
        WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_row, tmp_end_row);

    int tmp_x = std::max(left_white_space_bound, left_component_bound);

    for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
      tmp_x = std::max(tmp_x, component_contour_[n]);
    }

    int tmp_y = RowToLoc(tmp_start_row);

    double tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
    if (tmp_cost < min_cost) {
      best_loc_x = tmp_x;
      best_row = tmp_start_row;
      min_cost = tmp_cost;
    }
  }

  int best_row_legal = 0;
  int best_loc_x_legal = INT_MIN;
  double min_cost_legal = DBL_MAX;
  bool is_loc_legal = IsSpaceLegal(best_loc_x, best_loc_x + width, best_row,
                                   best_row + component_row_height - 1);

  if (!is_loc_legal) {
    int old_start_row = search_start_row;
    int old_end_row = search_end_row;
    int extended_range = cur_iter_ * component_row_height;
    search_start_row = std::max(0, search_start_row - extended_range);
    search_end_row = std::min(max_search_row, search_end_row + extended_range);
    for (int tmp_start_row = search_start_row; tmp_start_row <= old_start_row;
         ++tmp_start_row) {
      int tmp_end_row = tmp_start_row + component_row_height - 1;
      if (!IsFitToRow(tmp_start_row, component)) continue;
      int left_white_space_bound =
          WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_row, tmp_end_row);
      int tmp_x = std::max(left_white_space_bound, left_component_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::max(tmp_x, component_contour_[n]);
      }

      int tmp_y = RowToLoc(tmp_start_row);
      // double tmp_hpwl = EstimatedHPWL(component, tmp_x, tmp_y);

      double tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
      if (tmp_cost < min_cost) {
        best_loc_x = tmp_x;
        best_row = tmp_start_row;
        min_cost = tmp_cost;
      }

      is_loc_legal =
          IsSpaceLegal(tmp_x, tmp_x + width, tmp_start_row, tmp_end_row);

      if (is_loc_legal) {
        if (tmp_cost < min_cost_legal) {
          best_loc_x_legal = tmp_x;
          best_row_legal = tmp_start_row;
          min_cost_legal = tmp_cost;
        }
      }
    }
    for (int tmp_start_row = old_end_row; tmp_start_row <= search_end_row;
         ++tmp_start_row) {
      int tmp_end_row = tmp_start_row + component_row_height - 1;
      if (!IsFitToRow(tmp_start_row, component)) continue;
      int left_white_space_bound =
          WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_row, tmp_end_row);
      int tmp_x = std::max(left_white_space_bound, left_component_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::max(tmp_x, component_contour_[n]);
      }

      int tmp_y = RowToLoc(tmp_start_row);
      // double tmp_hpwl = EstimatedHPWL(component, tmp_x, tmp_y);

      double tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
      if (tmp_cost < min_cost) {
        best_loc_x = tmp_x;
        best_row = tmp_start_row;
        min_cost = tmp_cost;
      }

      is_loc_legal =
          IsSpaceLegal(tmp_x, tmp_x + width, tmp_start_row, tmp_end_row);

      if (is_loc_legal) {
        if (tmp_cost < min_cost_legal) {
          best_loc_x_legal = tmp_x;
          best_row_legal = tmp_start_row;
          min_cost_legal = tmp_cost;
        }
      }
    }
  }

  // if still cannot find a legal location, enter fail mode
  bool is_successful = IsSpaceLegal(best_loc_x, best_loc_x + width, best_row,
                                    best_row + component_row_height - 1);
  if (!is_successful) {
    if (best_loc_x_legal >= left_ && best_loc_x_legal <= right_ - width) {
      is_successful = IsSpaceLegal(best_loc_x_legal, best_loc_x_legal + width,
                                   best_row_legal,
                                   best_row_legal + component_row_height - 1);
    }
    if (is_successful) {
      best_loc_x = best_loc_x_legal;
      best_row = best_row_legal;
    }
  }

  loc.x = best_loc_x;
  loc.y = RowToLoc(best_row);

  return is_successful;
}

/****
 * 1. first sort all the circuit based on their location and size from low to
 * high effective_loc = current_lx - k_width_ * width - k_height_ * height;
 * 2. for each cell, find the leftmost legal location, the location is
 * left-bounded by: left_bound = current_lx - k_left_ * width; and left boundary
 * of the placement region
 * 3. local search range is bounded by
 *    a). [left_bound, right_] (the range in the x direction)
 *    b). [init_y - height, init_y + 2 * height] (the range in the y direction)
 *    if legal location cannot be found in this range, extend the y_direction by
 * height at each end
 * 4. if still no legal location can be found, do the reverse legalization
 * procedure till reach the maximum iteration
 * ****/
bool ExtendedTetrisLegalizer::LocalLegalizationLeft() {
  InitComponentContourForward();
  InitAndSortComponentAscendingX();

  bool is_successful = true;
  int failed_component_count = 0;
  for (auto& component_initial_location : component_initial_locations_) {
    auto& component = *(component_initial_location.component_ptr);

    Value2D<int> target_loc;
    target_loc.x = static_cast<int>(std::round(component.LLX()));
    target_loc.y = AlignLocToRowLoc(component.LLY());

    // is current local legal
    bool is_current_loc_legal = IsCurrentLocLegalLeft(target_loc, component);

    // if not legal
    if (!is_current_loc_legal) {
      // can we find a location nearby, this location can be illegal
      bool is_legal_loc_found = FindLocLeft(target_loc, component);
      if (!is_legal_loc_found) {
        is_successful = false;
        ++failed_component_count;
        if (logged_legalization_failure_examples_ <
            kMaxLegalizationFailureExamples) {
          ++logged_legalization_failure_examples_;
          int component_row_height = HeightToRow(component.Height());
          int max_search_row = MaxRow(component.Height());
          int extended_range = cur_iter_ * component_row_height;
          int search_start_row = std::max(
              0, LocToRow(component.LLY() - k_start * component.Height()) -
                     extended_range);
          int search_end_row =
              std::min(max_search_row,
                       LocToRow(component.LLY() + k_end * component.Height()) +
                           extended_range);
          LogLegalizationFailureExample(
              "left-to-right", cur_iter_, logged_legalization_failure_examples_,
              component, target_loc, search_start_row, search_end_row,
              tot_num_rows_, RegionLeft(), RegionRight());
        }
      }
    }

    // we will move this component to this location even if it is illegal
    component.SetLoc(target_loc.x, target_loc.y);
    int row_id = LocToRow(target_loc.y);
    ComponentOrient orient = ShouldOrientN(row_id, component) ? N : FS;
    component.SetOrient(orient);

    UseSpaceLeft(component);
  }

  if (failed_component_count > 0) {
    LogLegalizationFailureSummary("left-to-right", cur_iter_,
                                  failed_component_count,
                                  component_initial_locations_.size());
  }

  return is_successful;
}

void ExtendedTetrisLegalizer::InitComponentContourBackward() {
  component_contour_.assign(component_contour_.size(), right_);
}

void ExtendedTetrisLegalizer::InitAndSortComponentDescendingX() {
  component_initial_locations_.clear();
  auto& components = ckt_ptr_->Components();
  for (auto& component : components) {
    if (IsDummyComponent(component)) continue;
    if (component.IsFixed()) continue;
    double x_loc = component.URX() + k_width_ * component.Width() +
                   k_height_ * component.Height();
    double y_loc = component.LLY();
    component_initial_locations_.emplace_back(&component, x_loc, y_loc);
  }
  std::sort(component_initial_locations_.begin(),
            component_initial_locations_.end(),
            [](const ComponentInitialLocation& lhs,
               const ComponentInitialLocation& rhs) {
              return (lhs.x > rhs.x) || (lhs.x == rhs.x && lhs.y > rhs.y);
            });
}

void ExtendedTetrisLegalizer::UseSpaceRight(Component const& component) {
  int start_row = StartRow((int)std::round(component.LLY()));
  int end_row = EndRow((int)std::round(component.URY()));

  DaliExpects(component.URY() <= RegionTop(), "Out of bound?");
  DaliExpects(end_row < tot_num_rows_, "Out of bound?");
  DaliExpects(start_row >= 0, "Out of bound?");

  int end_x = int(component.LLX());
  for (int r = start_row; r <= end_row; ++r) {
    component_contour_[r] = end_x;
  }
}

/****
 * Returns whether the current location is legal
 * 1. if this component matches this row
 * 2. if the space itself is illegal, then return false
 * 3. if the space covers placed components, then return false
 * 4. otherwise, return true
 * ****/
bool ExtendedTetrisLegalizer::IsCurrentLocLegalRight(Value2D<int>& loc,
                                                     Component& component) {
  int width = component.Width();
  int height = component.Height();
  int start_row = StartRow(loc.y);
  int end_row = EndRow(loc.y + height);

  bool is_orient_match = IsFitToRow(start_row, component);
  if (!is_orient_match) {
    return false;
  }

  bool is_space_legal = IsSpaceLegal(loc.x - width, loc.x, start_row, end_row);
  if (!is_space_legal) {
    return false;
  }

  bool all_row_avail = true;
  for (int i = start_row; i <= end_row; ++i) {
    if (component_contour_[i] < loc.x) {
      all_row_avail = false;
      break;
    }
  }

  return all_row_avail;
}

/****
 * Returns the right boundary of the white space region where this component
 * should be placed
 *
 * For each row, find the segment which is closest to [lo_x, hi_x]
 * If a segment is [lo_seg, hi_seg], the distance is defined as
 *        min(|lo_seg - lo_x| + |lo_seg - hi_x|, |hi_seg - lo_x| + |hi_seg -
 * hi_x|)
 * ****/
int ExtendedTetrisLegalizer::WhiteSpaceBoundRight(int lo_x, int hi_x,
                                                  int lo_row, int hi_row) {
  int white_space_bound = right_;

  int min_distance = INT_MAX;
  int tmp_bound;

  for (int i = lo_row; i <= hi_row; ++i) {
    tmp_bound = right_;
    for (auto& seg : rows_[i]) {
      if (seg.lo <= lo_x && seg.hi >= hi_x) {
        tmp_bound = seg.hi;
        min_distance = 0;
        break;
      }
      int tmp_distance = std::min(abs(seg.lo - lo_x) + abs(seg.lo - hi_x),
                                  abs(seg.hi - lo_x) + abs(seg.hi - hi_x));
      if (tmp_distance < min_distance) {
        tmp_bound = seg.hi;
        min_distance = tmp_distance;
      }
    }
    white_space_bound = std::min(white_space_bound, tmp_bound);
  }

  return white_space_bound;
}

bool ExtendedTetrisLegalizer::FindLocRight(Value2D<int>& loc,
                                           Component& component) {
  bool is_successful;

  int component_row_height;
  int right_component_bound;
  int right_white_space_bound;

  int max_search_row;
  int search_start_row;
  int search_end_row;

  int best_row;
  int best_loc_x;
  double min_cost;

  double tmp_cost;
  int tmp_end_row;
  int tmp_x;
  int tmp_y;

  int width = component.Width();
  int height = component.Height();

  right_component_bound = (int)std::round(loc.x + k_left_ * width);

  max_search_row = MaxRow(height);
  component_row_height = HeightToRow(height);

  search_start_row = std::max(0, LocToRow(loc.y - k_start * height));
  search_end_row = std::min(max_search_row, LocToRow(loc.y + k_end * height));

  best_row = 0;
  best_loc_x = INT_MAX;
  min_cost = DBL_MAX;

  for (int tmp_start_row = search_start_row; tmp_start_row <= search_end_row;
       ++tmp_start_row) {
    tmp_end_row = tmp_start_row + component_row_height - 1;
    if (!IsFitToRow(tmp_start_row, component)) continue;
    right_white_space_bound =
        WhiteSpaceBoundRight(loc.x - width, loc.x, tmp_start_row, tmp_end_row);

    tmp_x = std::min(right_white_space_bound, right_component_bound);

    for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
      tmp_x = std::min(tmp_x, component_contour_[n]);
    }


    tmp_y = RowToLoc(tmp_start_row);
    // double tmp_hpwl = EstimatedHPWL(component, tmp_x, tmp_y);

    tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
    if (tmp_cost < min_cost) {
      best_loc_x = tmp_x;
      best_row = tmp_start_row;
      min_cost = tmp_cost;
    }
  }

  int best_row_legal = 0;
  int best_loc_x_legal = INT_MAX;
  double min_cost_legal = DBL_MAX;
  bool is_loc_legal = IsSpaceLegal(best_loc_x - width, best_loc_x, best_row,
                                   best_row + component_row_height - 1);

  if (!is_loc_legal) {
    int old_start_row = search_start_row;
    int old_end_row = search_end_row;
    int extended_range = cur_iter_ * component_row_height;
    search_start_row = std::max(0, search_start_row - extended_range);
    search_end_row = std::min(max_search_row, search_end_row + extended_range);
    for (int tmp_start_row = search_start_row; tmp_start_row < old_start_row;
         ++tmp_start_row) {
      tmp_end_row = tmp_start_row + component_row_height - 1;
      if (!IsFitToRow(tmp_start_row, component)) continue;
      right_white_space_bound = WhiteSpaceBoundRight(
          loc.x - width, loc.x, tmp_start_row, tmp_end_row);

      tmp_x = std::min(right_white_space_bound, right_component_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::min(tmp_x, component_contour_[n]);
      }

      tmp_y = RowToLoc(tmp_start_row);
      // double tmp_hpwl = EstimatedHPWL(component, tmp_x, tmp_y);

      tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
      if (tmp_cost < min_cost) {
        best_loc_x = tmp_x;
        best_row = tmp_start_row;
        min_cost = tmp_cost;
      }

      is_loc_legal =
          IsSpaceLegal(tmp_x - width, tmp_x, tmp_start_row, tmp_end_row);

      if (is_loc_legal) {
        if (tmp_cost < min_cost_legal) {
          best_loc_x_legal = tmp_x;
          best_row_legal = tmp_start_row;
          min_cost_legal = tmp_cost;
        }
      }
    }
    for (int tmp_start_row = old_end_row; tmp_start_row < search_end_row;
         ++tmp_start_row) {
      tmp_end_row = tmp_start_row + component_row_height - 1;
      if (!IsFitToRow(tmp_start_row, component)) continue;
      right_white_space_bound = WhiteSpaceBoundRight(
          loc.x - width, loc.x, tmp_start_row, tmp_end_row);

      tmp_x = std::min(right_white_space_bound, right_component_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::min(tmp_x, component_contour_[n]);
      }

      tmp_y = RowToLoc(tmp_start_row);
      // double tmp_hpwl = EstimatedHPWL(component, tmp_x, tmp_y);

      tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
      if (tmp_cost < min_cost) {
        best_loc_x = tmp_x;
        best_row = tmp_start_row;
        min_cost = tmp_cost;
      }

      is_loc_legal =
          IsSpaceLegal(tmp_x - width, tmp_x, tmp_start_row, tmp_end_row);

      if (is_loc_legal) {
        if (tmp_cost < min_cost_legal) {
          best_loc_x_legal = tmp_x;
          best_row_legal = tmp_start_row;
          min_cost_legal = tmp_cost;
        }
      }
    }
  }

  // if still cannot find a legal location, enter fail mode
  is_successful = IsSpaceLegal(best_loc_x - width, best_loc_x, best_row,
                               best_row + component_row_height - 1);
  if (!is_successful) {
    if (best_loc_x_legal <= right_ && best_loc_x_legal >= left_ + width) {
      is_successful = IsSpaceLegal(best_loc_x_legal - width, best_loc_x_legal,
                                   best_row_legal,
                                   best_row_legal + component_row_height - 1);
    }
    if (is_successful) {
      best_loc_x = best_loc_x_legal;
      best_row = best_row_legal;
    }
  }

  loc.x = best_loc_x;
  loc.y = RowToLoc(best_row);
  ;

  return is_successful;
}

/****
 * 1. first sort all the circuit based on their location and size from high to
 * low effective_loc = current_rx - k_width_ * width - k_height_ * height;
 * 2. for each cell, find the rightmost legal location, the location is
 * right-bounded by: right_bound = current_rx + k_left_ * width; and right
 * boundary of the placement region
 * 3. local search range is bounded by
 *    a). [left_, right_bound] (the range in the x direction)
 *    b). [init_y - height, init_y + 2 * height] (the range in the y direction)
 *    if legal location cannot be found in this range, extend the y_direction by
 * height at each end
 * 4. if still no legal location can be found, do the reverse legalization
 * procedure till reach the maximum iteration
 * ****/
bool ExtendedTetrisLegalizer::LocalLegalizationRight() {
  InitComponentContourBackward();
  InitAndSortComponentDescendingX();

  bool is_successful = true;
  int failed_component_count = 0;
  for (auto& component_initial_location : component_initial_locations_) {
    auto& component = *(component_initial_location.component_ptr);
    Value2D<int> target_loc;
    target_loc.x = int(std::round(component.URX()));
    target_loc.y = AlignLocToRowLoc(component.LLY());
    bool is_current_loc_legal = IsCurrentLocLegalRight(target_loc, component);

    if (!is_current_loc_legal) {
      bool is_legal_loc_found = FindLocRight(target_loc, component);
      if (!is_legal_loc_found) {
        is_successful = false;
        ++failed_component_count;
        if (logged_legalization_failure_examples_ <
            kMaxLegalizationFailureExamples) {
          ++logged_legalization_failure_examples_;
          int component_row_height = HeightToRow(component.Height());
          int max_search_row = MaxRow(component.Height());
          int extended_range = cur_iter_ * component_row_height;
          int search_start_row = std::max(
              0, LocToRow(component.LLY() - k_start * component.Height()) -
                     extended_range);
          int search_end_row =
              std::min(max_search_row,
                       LocToRow(component.LLY() + k_end * component.Height()) +
                           extended_range);
          LogLegalizationFailureExample(
              "right-to-left", cur_iter_, logged_legalization_failure_examples_,
              component, target_loc, search_start_row, search_end_row,
              tot_num_rows_, RegionLeft(), RegionRight());
        }
      }
    }

    component.SetURX(target_loc.x);
    component.SetLLY(target_loc.y);
    int row_id = LocToRow(target_loc.y);
    ComponentOrient orient = ShouldOrientN(row_id, component) ? N : FS;
    component.SetOrient(orient);

    UseSpaceRight(component);
  }

  if (failed_component_count > 0) {
    LogLegalizationFailureSummary("right-to-left", cur_iter_,
                                  failed_component_count,
                                  component_initial_locations_.size());
  }

  return is_successful;
}

void ExtendedTetrisLegalizer::ResetLeftLimitFactor() { k_left_ = k_left_init_; }

void ExtendedTetrisLegalizer::UpdateLeftLimitFactor() {
  k_left_ += k_left_step_;
}

double ExtendedTetrisLegalizer::EstimatedHPWL(Component& component, int x,
                                              int y) {
  double max_x = x;
  double max_y = y;
  double min_x = x;
  double min_y = y;
  double tot_hpwl = 0;
  auto& net_list = ckt_ptr_->Nets();
  for (auto& net_num : component.NetList()) {
    auto& net = net_list[net_num];
    if (net.PinCnt() > 100) continue;
    for (auto& component_pin : net.ComponentPins()) {
      if (component_pin.ComponentPtr() != &component) {
        min_x = std::min(min_x, component_pin.AbsX());
        min_y = std::min(min_y, component_pin.AbsY());
        max_x = std::max(max_x, component_pin.AbsX());
        max_y = std::max(max_y, component_pin.AbsY());
      }
    }
    tot_hpwl += (max_x - min_x) + (max_y - min_y);
  }

  return tot_hpwl;
}

void ExtendedTetrisLegalizer::ExportRowsToCircuit() {
  std::vector<GeneralRow>& rows = ckt_ptr_->design().Rows();
  rows.clear();
  rows.reserve(tot_num_rows_);
  bool is_orient_N = is_first_row_N_;

  // initialize rows in circuit
  for (int i = 0; i < tot_num_rows_; ++i) {
    rows.emplace_back();
    auto& last_row = rows.back();
    last_row.SetLY(i * row_height_ + RegionBottom());
    last_row.SetHeight(row_height_);
    last_row.SetOrient(is_orient_N);

    auto& row_segments = last_row.RowSegments();
    row_segments.reserve(rows_[i].size());
    for (auto& seg : rows_[i]) {
      row_segments.emplace_back();
      auto& last_segment = row_segments.back();
      last_segment.SetLX(seg.lo);
      last_segment.SetWidth(seg.Span());
    }
    is_orient_N = !is_orient_N;
  }

  // associate components to the right row segment
  auto& components = ckt_ptr_->Components();
  for (auto& component : components) {
    if (component.IsFixed()) continue;
    int row_id = LocToRow(component.LLY());
    bool is_associated = false;
    for (auto& seg : rows[row_id].RowSegments()) {
      if (component.LLX() >= seg.LX() && component.URX() <= seg.UX()) {
        is_associated = true;
        seg.AddComponent(&component);
        break;
      }
    }
    DaliExpects(is_associated, "cannot find a row segment for a component");
  }
}

bool ExtendedTetrisLegalizer::StartPlacement() {
  PrintStartStatement("ExtendedTetrisLegalizer Legalization");

  is_row_assignment_ = false;
  logged_legalization_failure_examples_ = 0;
  InitLegalizer();
  ResetLeftLimitFactor();

  bool is_success = false;
  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    if (legalize_from_left_) {
      is_success = LocalLegalizationLeft();
    } else {
      is_success = LocalLegalizationRight();
    }
    legalize_from_left_ = !legalize_from_left_;
    UpdateLeftLimitFactor();
    ReportHPWL();
    if (is_success) {
      break;
    }
  }

  if (is_success) {
    ExportRowsToCircuit();
  }

  // TODO: local reordering

  PrintEndStatement("ExtendedTetrisLegalizer Legalization", is_success);

  return is_success;
}

bool ExtendedTetrisLegalizer::StartRowAssignment() {
  PrintStartStatement("row assignment");

  is_row_assignment_ = true;
  logged_legalization_failure_examples_ = 0;
  ResetLeftLimitFactor();

  bool is_success = false;
  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    if (legalize_from_left_) {
      is_success = LocalLegalizationLeft();
    } else {
      is_success = LocalLegalizationRight();
    }
    legalize_from_left_ = !legalize_from_left_;
    UpdateLeftLimitFactor();
    if (is_success) {
      break;
    }
  }
  LOG(info) << "\033[0;36m"
            << "Row assignment complete (" << cur_iter_ + 1 << ")\n"
            << "\033[0m";

  if (!is_success) {
    LOG(info) << "Placement illegal\n";
  }

  ReportHPWL();
  ReportBoundingBox();

  elapsed_time_.RecordEndTime();
  elapsed_time_.PrintTimeElapsed();

  ReportMemory();

  return true;
}

void ExtendedTetrisLegalizer::GenAvailSpace(std::string const& name_of_file) {
  LOG(info) << "Generating available space, dump result to: " << name_of_file
            << "\n";
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " << name_of_file);
  ost << RegionLeft() << "\t" << RegionRight() << "\t" << RegionRight() << "\t"
      << RegionLeft() << "\t" << RegionBottom() << "\t" << RegionBottom()
      << "\t" << RegionTop() << "\t" << RegionTop() << "\n";
  for (int i = 0; i < tot_num_rows_; ++i) {
    auto& row = rows_[i];
    for (auto& seg : row) {
      ost << seg.lo << "\t" << seg.hi << "\t" << seg.hi << "\t" << seg.lo
          << "\t" << i * row_height_ + RegionBottom() << "\t"
          << i * row_height_ + RegionBottom() << "\t"
          << (i + 1) * row_height_ + RegionBottom() << "\t"
          << (i + 1) * row_height_ + RegionBottom() << "\n";
    }
  }

  auto& components = ckt_ptr_->Components();
  for (auto& component : components) {
    if (component.IsMovable()) continue;
    ost << component.LLX() << "\t" << component.URX() << "\t" << component.URX()
        << "\t" << component.LLX() << "\t" << component.LLY() << "\t"
        << component.LLY() << "\t" << component.URY() << "\t" << component.URY()
        << "\n";
  }
}

}  // namespace dali
