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
#include "LGTetrisEx.h"

#include <cfloat>

#include <algorithm>
#include <list>

#include "dali/common/helper.h"

namespace dali {

LGTetrisEx::LGTetrisEx()
    : Placer(),
      row_height_(0),
      row_height_set_(false),
      legalize_from_left_(true),
      cur_iter_(0),
      max_iter_(20),
      tot_num_rows_(0) {}

void LGTetrisEx::SetRowHeight(int row_height) {
  DaliExpects(row_height > 0, "Cannot set negative row height!");
  row_height_ = row_height;
  row_height_set_ = true;
}

void LGTetrisEx::SetMaxIteration(size_t max_iter) {
  max_iter_ = max_iter;
}

void LGTetrisEx::SetWidthHeightFactor(double k_width, double k_height) {
  k_width_ = k_width;
  k_height_ = k_height;
}

void LGTetrisEx::SetLeftBoundFactor(double k_left, double k_left_step) {
  k_left_ = k_left;
  k_left_step_ = k_left_step;
}

void LGTetrisEx::InitializeFromGriddedRowLegalizer(GriddedRowLegalizer *grlg) {
  DaliExpects(grlg != nullptr,
              "Cannot initialize LGTetrisEx from a nullptr GriddedRowLegalizer");
  // circuit
  p_ckt_ = grlg->p_ckt_;

  // rows info
  auto &stripe = grlg->col_list_[0].stripe_list_[0];
  row_height_ = stripe.row_height_;
  tot_num_rows_ = static_cast<int>(stripe.gridded_rows_.size());
  is_first_row_N_ = stripe.gridded_rows_[0].IsOrientN();

  // placement boundary
  left_ = stripe.LLX();
  right_ = stripe.URX();
  bottom_ = stripe.LLY();
  top_ = stripe.URY();

  // rows
  row_segments_.clear();
  row_segments_.resize(tot_num_rows_);
  for (int i = 0; i < tot_num_rows_; ++i) {
    auto &row = stripe.gridded_rows_[i];
    for (auto &seg: row.Segments()) {
      row_segments_[i].emplace_back(seg.LLX(), seg.URX());
    }
  }

  block_contour_.clear();
  block_contour_.resize(tot_num_rows_, left_);

  BlkInitPair tmp_index_loc_pair(nullptr, 0, 0);
  blk_inits_.clear();
  blk_inits_.resize(Blocks().size(), tmp_index_loc_pair);
}

void LGTetrisEx::SetRowInfoAuto() {
  if (!row_height_set_) {
    row_height_ = p_ckt_->RowHeightGridUnit();
  }
  tot_num_rows_ = (top_ - bottom_) / row_height_;
  block_contour_.resize(tot_num_rows_, left_);
}

void LGTetrisEx::DetectWhiteSpace() {
  std::vector<std::vector<SegI>> macro_segments;
  macro_segments.resize(tot_num_rows_);
  Seg tmp(0, 0);
  bool out_of_range;
  for (auto &block: Blocks()) {
    if (IsDummyBlock(block)) continue;
    if (block.IsMovable()) continue;
    int ly = int(std::floor(block.LLY()));
    int uy = int(std::ceil(block.URY()));
    int lx = int(std::floor(block.LLX()));
    int ux = int(std::ceil(block.URX()));

    out_of_range = (ly >= RegionTop()) || (uy <= RegionBottom())
        || (lx >= RegionRight()) || (ux <= RegionLeft());

    if (out_of_range) continue;

    int start_row = StartRow(ly);
    int end_row = EndRow(uy);

    start_row = std::max(0, start_row);
    end_row = std::min(tot_num_rows_ - 1, end_row);

    tmp.lo = std::max(RegionLeft(), lx);
    tmp.hi = std::min(RegionRight(), ux);
    if (tmp.hi > tmp.lo) {
      for (int i = start_row; i <= end_row; ++i) {
        macro_segments[i].push_back(tmp);
      }
    }
  }
  for (auto &intervals: macro_segments) {
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
      auto &interval = macro_segments[i][j];
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

  row_segments_.resize(tot_num_rows_);
  int min_blk_width = int(p_ckt_->MinBlkWidth());
  for (int i = 0; i < tot_num_rows_; ++i) {
    int len = int(intermediate_seg_rows[i].size());
    row_segments_[i].reserve(len / 2);
    for (int j = 0; j < len; j += 2) {
      if (intermediate_seg_rows[i][j + 1] - intermediate_seg_rows[i][j]
          >= min_blk_width) {
        row_segments_[i].emplace_back(
            intermediate_seg_rows[i][j],
            intermediate_seg_rows[i][j + 1]
        );
      }
    }
  }
  //PlotAvailSpace();
}

void LGTetrisEx::InitIndexLocList() {
  BlkInitPair tmp_index_loc_pair(nullptr, 0, 0);
  blk_inits_.resize(Blocks().size(), tmp_index_loc_pair);
}

/****
 * 1. calculate the number of rows for a given row_height
 * 2. initialize white space available in rows
 * 3. initialize block contour to be the left contour
 * 4. allocate space for index_loc_list_
 * ****/
void LGTetrisEx::InitLegalizer() {
  SetRowInfoAuto();
  DetectWhiteSpace();
  InitIndexLocList();
}

int LGTetrisEx::RowHeight() const {
  return row_height_;
}

int LGTetrisEx::StartRow(int y_loc) const {
  return (y_loc - bottom_) / row_height_;
}

int LGTetrisEx::EndRow(int y_loc) const {
  int relative_y = y_loc - bottom_;
  int res = relative_y / row_height_;
  if (relative_y % row_height_ == 0) {
    --res;
  }
  return res;
}

int LGTetrisEx::MaxRow(int height) const {
  return ((top_ - height) - bottom_) / row_height_;
}

int LGTetrisEx::HeightToRow(int height) const {
  return std::ceil(height / double(row_height_));
}

int LGTetrisEx::LocToRow(int y_loc) const {
  return (y_loc - bottom_) / row_height_;
}

int LGTetrisEx::RowToLoc(int row_num, int displacement) const {
  return row_num * row_height_ + bottom_ + displacement;
}

int LGTetrisEx::AlignLocToRowLoc(double y_loc) const {
  int row_num = static_cast<int>(std::round((y_loc - bottom_) / row_height_));
  if (row_num < 0) row_num = 0;
  if (row_num >= tot_num_rows_) row_num = tot_num_rows_ - 1;
  return row_num * row_height_ + bottom_;
}

/****
 * This member function checks if the region specified by [lo_x, hi_x] and [lo_row, hi_row] is legal or not
 *
 * If this space overlaps with fixed macros or out of placement range, then this space is illegal.
 *
 * To determine this space is legal, one just need to show that every row is legal
 * ****/
bool LGTetrisEx::IsSpaceLegal(
    int lo_x, int hi_x,
    int lo_row, int hi_row
) const {
  assert(lo_x <= hi_x);
  assert(lo_row <= hi_row);

  bool loc_out_range = (hi_x > right_) || (lo_x < left_)
      || (hi_row >= tot_num_rows_) || (lo_row < 0);
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
    seg_count = row_segments_[i].size();
    is_tmp_row_legal = false;
    for (int j = 0; j < seg_count; ++j) {
      if (row_segments_[i][j].lo <= lo_x
          && row_segments_[i][j].hi >= hi_x) {
        is_tmp_row_legal = true;
        break;
      }

      is_partial_cover_lo = row_segments_[i][j].lo > lo_x
          && row_segments_[i][j].lo < hi_x;
      is_partial_cover_hi = row_segments_[i][j].hi > lo_x
          && row_segments_[i][j].hi < hi_x;
      is_before_seg = row_segments_[i][j].lo >= hi_x;
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

bool LGTetrisEx::IsFitToRow(int row_id, Block &block) const {
  int region_cnt = block.TypePtr()->WellPtr()->RegionCount();
  if (region_cnt & 1) { // odd region_cnt can be placed into any rows
    return true;
  }
  // even region_cnt can only be placed into every other row
  bool is_gnd_bottom = block.TypePtr()->WellPtr()->IsNwellAbovePwell(0);
  bool is_row_even = !(row_id & 1);
  bool is_row_N = (is_row_even && is_first_row_N_) ||
      (!is_row_even && !is_first_row_N_);

  return is_row_N == is_gnd_bottom;
}

bool LGTetrisEx::ShouldOrientN(int row_id, Block &block) const {
  bool is_gnd_bottom = block.TypePtr()->WellPtr()->IsNwellAbovePwell(0);
  bool is_row_even = !(row_id & 1);
  bool is_row_N = (is_row_even && is_first_row_N_) ||
      (!is_row_even && !is_first_row_N_);

  return ((is_row_N && is_gnd_bottom) || (!is_row_N && !is_gnd_bottom));
}

void LGTetrisEx::InitBlockContourForward() {
  block_contour_.assign(block_contour_.size(), left_);
}

void LGTetrisEx::InitAndSortBlockAscendingX() {
  blk_inits_.clear();
  for (auto &blk: Blocks()) {
    // skipp dummy blocks and fixed blocks
    if (IsDummyBlock(blk)) continue;
    if (blk.IsFixed()) continue;
    double x_loc = blk.LLX() - k_width_ * blk.Width()
        - k_height_ * blk.Height();
    double y_loc = blk.LLY();
    blk_inits_.emplace_back(&blk, x_loc, y_loc);
  }

  std::sort(
      blk_inits_.begin(),
      blk_inits_.end(),
      [](const BlkInitPair &pair0, const BlkInitPair &pair1) {
        return (pair0.x < pair1.x)
            || ((pair0.x == pair1.x) && (pair0.y < pair1.y));
      }
  );
}

/****
 * Mark the space used by this block by changing the start point of available space in each related row
 * ****/
void LGTetrisEx::UseSpaceLeft(Block const &block) {
  int start_row = StartRow(int(block.LLY()));
  int end_row = EndRow(int(block.URY()));

  DaliExpects(block.URY() <= RegionTop(), "Out of bound?");
  DaliExpects(end_row < tot_num_rows_, "Out of bound?");
  DaliExpects(start_row >= 0, "Out of bound?");

  int end_x = int(block.URX());
  for (int i = start_row; i <= end_row; ++i) {
    block_contour_[i] = end_x;
  }
}

/****
 * Returns whether the current location is legal
 * 1. if this block matches this row
 * 2. if the space itself is illegal, then return false
 * 3. if the space covers placed blocks, then return false
 * 4. otherwise, return true
 * ****/
bool LGTetrisEx::IsCurrentLocLegalLeft(
    Value2D<int> &loc, Block &block
) {
  int start_row = StartRow(loc.y);
  int end_row = EndRow(loc.y + block.Height());

  // can this block fit this row?
  if (!IsFitToRow(start_row, block)) {
    return false;
  }

  // is this location legal?
  bool is_space_legal = IsSpaceLegal(
      loc.x, loc.x + block.Width(),
      start_row, end_row
  );
  if (!is_space_legal) {
    return false;
  }

  // is space not occupied by other cells?
  for (int i = start_row; i <= end_row; ++i) {
    if (block_contour_[i] > loc.x) {
      return false;
    }
  }

  return true;
}

/****
 * Returns the left boundary of the white space region where this block should be placed
 *
 * For each row, find the segment which is closest to [lo_x, hi_x]
 * If a segment is [lo_seg, hi_seg], the distance is defined as
 *        min(|lo_seg - lo_x| + |lo_seg - hi_x|, |hi_seg - lo_x| + |hi_seg - hi_x|)
 * ****/
int LGTetrisEx::WhiteSpaceBoundLeft(
    int lo_x, int hi_x,
    int lo_row, int hi_row
) {
  int white_space_bound = left_;

  int min_distance = INT_MAX;
  int tmp_bound;

  for (int i = lo_row; i <= hi_row; ++i) {
    tmp_bound = left_;
    for (auto &seg: row_segments_[i]) {
      if (seg.lo <= lo_x && seg.hi >= hi_x) {
        tmp_bound = seg.lo;
        min_distance = 0;
        break;
      }
      int tmp_distance = std::min(
          abs(seg.lo - lo_x) + abs(seg.lo - hi_x),
          abs(seg.hi - lo_x) + abs(seg.hi - hi_x)
      );
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
 * Returns whether a legal location can be found, and put the final location to @params loc
 * ****/
bool LGTetrisEx::FindLocLeft(Value2D<int> &loc, Block &block) {
  bool is_successful;

  int left_white_space_bound;
  int search_start_row;
  int search_end_row;

  int best_row;
  int best_loc_x;
  double min_cost;

  double tmp_cost;
  int tmp_end_row;
  int tmp_x;
  int tmp_y;

  int width = block.Width();
  int height = block.Height();

  int left_block_bound = static_cast<int>(std::round(loc.x - k_left_ * width));
  int max_search_row = MaxRow(height);
  int blk_row_height = HeightToRow(height);

  int lower_search_y = static_cast<int>(std::round(loc.y - k_start * height));
  int upper_search_y = static_cast<int>(std::round(loc.y + k_end * height));
  search_start_row = std::max(0, LocToRow(lower_search_y));
  search_end_row = std::min(max_search_row, LocToRow(upper_search_y));

  best_row = 0;
  best_loc_x = INT_MIN;
  min_cost = DBL_MAX;

  for (int tmp_start_row = search_start_row; tmp_start_row <= search_end_row;
       ++tmp_start_row) {
    tmp_end_row = tmp_start_row + blk_row_height - 1;
    if (!IsFitToRow(tmp_start_row, block)) continue;
    //left_white_space_bound = left_;
    left_white_space_bound = WhiteSpaceBoundLeft(
        loc.x, loc.x + width,
        tmp_start_row, tmp_end_row
    );

    tmp_x = std::max(left_white_space_bound, left_block_bound);

    for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
      tmp_x = std::max(tmp_x, block_contour_[n]);
    }

    tmp_y = RowToLoc(tmp_start_row);

    tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
    if (tmp_cost < min_cost) {
      best_loc_x = tmp_x;
      best_row = tmp_start_row;
      min_cost = tmp_cost;
    }
  }

  int best_row_legal = 0;
  int best_loc_x_legal = INT_MIN;
  double min_cost_legal = DBL_MAX;
  bool is_loc_legal = IsSpaceLegal(
      best_loc_x, best_loc_x + width,
      best_row, best_row + blk_row_height - 1
  );

  if (!is_loc_legal) {
    int old_start_row = search_start_row;
    int old_end_row = search_end_row;
    int extended_range = cur_iter_ * blk_row_height;
    search_start_row = std::max(0, search_start_row - extended_range);
    search_end_row =
        std::min(max_search_row, search_end_row + extended_range);
    for (int tmp_start_row = search_start_row;
         tmp_start_row < old_start_row; ++tmp_start_row) {
      tmp_end_row = tmp_start_row + blk_row_height - 1;
      if (!IsFitToRow(tmp_start_row, block)) continue;
      left_white_space_bound = WhiteSpaceBoundLeft(
          loc.x, loc.x + width,
          tmp_start_row, tmp_end_row
      );
      tmp_x = std::max(left_white_space_bound, left_block_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::max(tmp_x, block_contour_[n]);
      }

      tmp_y = RowToLoc(tmp_start_row);
      //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

      tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
      if (tmp_cost < min_cost) {
        best_loc_x = tmp_x;
        best_row = tmp_start_row;
        min_cost = tmp_cost;
      }

      is_loc_legal = IsSpaceLegal(
          tmp_x, tmp_x + width,
          tmp_start_row, tmp_end_row
      );

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
      tmp_end_row = tmp_start_row + blk_row_height - 1;
      if (!IsFitToRow(tmp_start_row, block)) continue;
      left_white_space_bound = WhiteSpaceBoundLeft(
          loc.x, loc.x + width,
          tmp_start_row, tmp_end_row
      );
      tmp_x = std::max(left_white_space_bound, left_block_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::max(tmp_x, block_contour_[n]);
      }

      tmp_y = RowToLoc(tmp_start_row);
      //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

      tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
      if (tmp_cost < min_cost) {
        best_loc_x = tmp_x;
        best_row = tmp_start_row;
        min_cost = tmp_cost;
      }

      is_loc_legal = IsSpaceLegal(
          tmp_x, tmp_x + width,
          tmp_start_row, tmp_end_row
      );

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
  is_successful = IsSpaceLegal(
      best_loc_x, best_loc_x + width,
      best_row, best_row + blk_row_height - 1
  );
  if (!is_successful) {
    if (best_loc_x_legal >= left_ && best_loc_x_legal <= right_ - width) {
      is_successful = IsSpaceLegal(
          best_loc_x_legal, best_loc_x_legal + width,
          best_row_legal, best_row_legal + blk_row_height - 1
      );
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
 * 1. first sort all the circuit based on their location and size from low to high
 *    effective_loc = current_lx - k_width_ * width - k_height_ * height;
 * 2. for each cell, find the leftmost legal location, the location is left-bounded by:
 *    left_bound = current_lx - k_left_ * width;
 *    and
 *    left boundary of the placement region
 * 3. local search range is bounded by
 *    a). [left_bound, right_] (the range in the x direction)
 *    b). [init_y - height, init_y + 2 * height] (the range in the y direction)
 *    if legal location cannot be found in this range, extend the y_direction by height at each end
 * 4. if still no legal location can be found, do the reverse legalization procedure till reach the maximum iteration
 * ****/
bool LGTetrisEx::LocalLegalizationLeft() {
  InitBlockContourForward();
  InitAndSortBlockAscendingX();

  bool is_successful = true;
  for (auto &blk_init_pair: blk_inits_) {
    auto &block = *(blk_init_pair.blk_ptr);

    Value2D<int> target_loc;
    target_loc.x = static_cast<int>(std::round(block.LLX()));
    target_loc.y = AlignLocToRowLoc(block.LLY());

    // is current local legal
    bool is_current_loc_legal = IsCurrentLocLegalLeft(target_loc, block);

    // if not legal
    if (!is_current_loc_legal) {
      // can we find a location nearby, this location can be illegal
      bool is_legal_loc_found = FindLocLeft(target_loc, block);
      if (!is_legal_loc_found) {
        is_successful = false;
      }
    }

    // we will move this block to this location even if it is illegal
    block.SetLoc(target_loc.x, target_loc.y);
    int row_id = LocToRow(target_loc.y);
    BlockOrient orient = ShouldOrientN(row_id, block) ? N : FS;
    block.SetOrient(orient);

    UseSpaceLeft(block);
  }

  return is_successful;
}

void LGTetrisEx::InitBlockContourBackward() {
  block_contour_.assign(block_contour_.size(), right_);
}

void LGTetrisEx::InitAndSortBlockDescendingX() {
  blk_inits_.clear();
  for (auto &blk: Blocks()) {
    if (IsDummyBlock(blk)) continue;
    if (blk.IsFixed()) continue;
    double x_loc = blk.URX() + k_width_ * blk.Width()
        + k_height_ * blk.Height();
    double y_loc = blk.LLY();
    blk_inits_.emplace_back(&blk, x_loc, y_loc);
  }
  std::sort(
      blk_inits_.begin(),
      blk_inits_.end(),
      [](const BlkInitPair &lhs, const BlkInitPair &rhs) {
        return (lhs.x > rhs.x) || (lhs.x == rhs.x && lhs.y > rhs.y);
      }
  );
}

void LGTetrisEx::UseSpaceRight(Block const &block) {
  int start_row = StartRow((int) std::round(block.LLY()));
  int end_row = EndRow((int) std::round(block.URY()));

  DaliExpects(block.URY() <= RegionTop(), "Out of bound?");
  DaliExpects(end_row < tot_num_rows_, "Out of bound?");
  DaliExpects(start_row >= 0, "Out of bound?");

  int end_x = int(block.LLX());
  for (int r = start_row; r <= end_row; ++r) {
    block_contour_[r] = end_x;
  }
}

/****
 * Returns whether the current location is legal
 * 1. if this block matches this row
 * 2. if the space itself is illegal, then return false
 * 3. if the space covers placed blocks, then return false
 * 4. otherwise, return true
 * ****/
bool LGTetrisEx::IsCurrentLocLegalRight(Value2D<int> &loc, Block &block) {
  int width = block.Width();
  int height = block.Height();
  int start_row = StartRow(loc.y);
  int end_row = EndRow(loc.y + height);

  bool is_orient_match = IsFitToRow(start_row, block);
  if (!is_orient_match) {
    return false;
  }

  bool is_space_legal = IsSpaceLegal(
      loc.x - width, loc.x,
      start_row, end_row
  );
  if (!is_space_legal) {
    return false;
  }

  bool all_row_avail = true;
  for (int i = start_row; i <= end_row; ++i) {
    if (block_contour_[i] < loc.x) {
      all_row_avail = false;
      break;
    }
  }

  return all_row_avail;
}

/****
 * Returns the right boundary of the white space region where this block should be placed
 *
 * For each row, find the segment which is closest to [lo_x, hi_x]
 * If a segment is [lo_seg, hi_seg], the distance is defined as
 *        min(|lo_seg - lo_x| + |lo_seg - hi_x|, |hi_seg - lo_x| + |hi_seg - hi_x|)
 * ****/
int LGTetrisEx::WhiteSpaceBoundRight(
    int lo_x, int hi_x,
    int lo_row, int hi_row
) {
  int white_space_bound = right_;

  int min_distance = INT_MAX;
  int tmp_bound;

  for (int i = lo_row; i <= hi_row; ++i) {
    tmp_bound = right_;
    for (auto &seg: row_segments_[i]) {
      if (seg.lo <= lo_x && seg.hi >= hi_x) {
        tmp_bound = seg.hi;
        min_distance = 0;
        break;
      }
      int tmp_distance = std::min(abs(seg.lo - lo_x) + abs(seg.lo - hi_x),
                                  abs(seg.hi - lo_x)
                                      + abs(seg.hi - hi_x));
      if (tmp_distance < min_distance) {
        tmp_bound = seg.hi;
        min_distance = tmp_distance;
      }
    }
    white_space_bound = std::min(white_space_bound, tmp_bound);
  }

  return white_space_bound;
}

bool LGTetrisEx::FindLocRight(Value2D<int> &loc, Block &block) {
  bool is_successful;

  int blk_row_height;
  int right_block_bound;
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

  int width = block.Width();
  int height = block.Height();

  right_block_bound = (int) std::round(loc.x + k_left_ * width);
  //right_block_bound = loc.x;

  max_search_row = MaxRow(height);
  blk_row_height = HeightToRow(height);

  search_start_row = std::max(0, LocToRow(loc.y - k_start * height));
  search_end_row = std::min(max_search_row, LocToRow(loc.y + k_end * height));

  best_row = 0;
  best_loc_x = INT_MAX;
  min_cost = DBL_MAX;

  for (int tmp_start_row = search_start_row; tmp_start_row <= search_end_row;
       ++tmp_start_row) {
    tmp_end_row = tmp_start_row + blk_row_height - 1;
    if (!IsFitToRow(tmp_start_row, block)) continue;
    right_white_space_bound = WhiteSpaceBoundRight(loc.x - width,
                                                   loc.x,
                                                   tmp_start_row,
                                                   tmp_end_row);

    tmp_x = std::min(right_white_space_bound, right_block_bound);
    //tmp_x = std::min(right_, right_block_bound);

    for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
      tmp_x = std::min(tmp_x, block_contour_[n]);
    }

    //if (tmp_x - width < left_) continue;

    tmp_y = RowToLoc(tmp_start_row);
    //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

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
  bool is_loc_legal = IsSpaceLegal(best_loc_x - width,
                                   best_loc_x,
                                   best_row,
                                   best_row + blk_row_height - 1);

  if (!is_loc_legal) {
    int old_start_row = search_start_row;
    int old_end_row = search_end_row;
    int extended_range = cur_iter_ * blk_row_height;
    search_start_row = std::max(0, search_start_row - extended_range);
    search_end_row =
        std::min(max_search_row, search_end_row + extended_range);
    for (int tmp_start_row = search_start_row;
         tmp_start_row < old_start_row; ++tmp_start_row) {
      tmp_end_row = tmp_start_row + blk_row_height - 1;
      if (!IsFitToRow(tmp_start_row, block)) continue;
      right_white_space_bound = WhiteSpaceBoundRight(loc.x - width,
                                                     loc.x,
                                                     tmp_start_row,
                                                     tmp_end_row);

      tmp_x = std::min(right_white_space_bound, right_block_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::min(tmp_x, block_contour_[n]);
      }

      tmp_y = RowToLoc(tmp_start_row);
      //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

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
      tmp_end_row = tmp_start_row + blk_row_height - 1;
      if (!IsFitToRow(tmp_start_row, block)) continue;
      right_white_space_bound = WhiteSpaceBoundRight(loc.x - width,
                                                     loc.x,
                                                     tmp_start_row,
                                                     tmp_end_row);

      tmp_x = std::min(right_white_space_bound, right_block_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::min(tmp_x, block_contour_[n]);
      }

      tmp_y = RowToLoc(tmp_start_row);
      //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

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
  is_successful = IsSpaceLegal(best_loc_x - width, best_loc_x,
                               best_row, best_row + blk_row_height - 1);
  if (!is_successful) {
    if (best_loc_x_legal <= right_ && best_loc_x_legal >= left_ + width) {
      is_successful =
          IsSpaceLegal(best_loc_x_legal - width,
                       best_loc_x_legal,
                       best_row_legal,
                       best_row_legal + blk_row_height - 1);
    }
    if (is_successful) {
      best_loc_x = best_loc_x_legal;
      best_row = best_row_legal;
    }
  }

  loc.x = best_loc_x;
  loc.y = RowToLoc(best_row);;

  return is_successful;
}

/****
 * 1. first sort all the circuit based on their location and size from high to low
 *    effective_loc = current_rx - k_width_ * width - k_height_ * height;
 * 2. for each cell, find the rightmost legal location, the location is right-bounded by:
 *    right_bound = current_rx + k_left_ * width;
 *    and
 *    right boundary of the placement region
 * 3. local search range is bounded by
 *    a). [left_, right_bound] (the range in the x direction)
 *    b). [init_y - height, init_y + 2 * height] (the range in the y direction)
 *    if legal location cannot be found in this range, extend the y_direction by height at each end
 * 4. if still no legal location can be found, do the reverse legalization procedure till reach the maximum iteration
 * ****/
bool LGTetrisEx::LocalLegalizationRight() {
  InitBlockContourBackward();
  InitAndSortBlockDescendingX();

  bool is_successful = true;
  for (auto &blk_init_pair: blk_inits_) {
    auto &block = *(blk_init_pair.blk_ptr);
    Value2D<int> target_loc;
    target_loc.x = int(std::round(block.URX()));
    target_loc.y = AlignLocToRowLoc(block.LLY());
    bool is_current_loc_legal = IsCurrentLocLegalRight(target_loc, block);

    if (!is_current_loc_legal) {
      bool is_legal_loc_found = FindLocRight(target_loc, block);
      if (!is_legal_loc_found) {
        is_successful = false;
      }
    }

    block.SetURX(target_loc.x);
    block.SetLLY(target_loc.y);
    int row_id = LocToRow(target_loc.y);
    BlockOrient orient = ShouldOrientN(row_id, block) ? N : FS;
    block.SetOrient(orient);

    UseSpaceRight(block);
  }

  return is_successful;
}

void LGTetrisEx::ResetLeftLimitFactor() {
  k_left_ = k_left_init_;
}

void LGTetrisEx::UpdateLeftLimitFactor() {
  k_left_ += k_left_step_;
}

double LGTetrisEx::EstimatedHPWL(Block &block, int x, int y) {
  double max_x = x;
  double max_y = y;
  double min_x = x;
  double min_y = y;
  double tot_hpwl = 0;
  auto &net_list = Nets();
  for (auto &net_num: block.NetList()) {
    auto &net = net_list[net_num];
    if (net.PinCnt() > 100) continue;
    for (auto &blk_pin: net.BlockPins()) {
      if (blk_pin.BlkPtr() != &block) {
        min_x = std::min(min_x, blk_pin.AbsX());
        min_y = std::min(min_y, blk_pin.AbsY());
        max_x = std::max(max_x, blk_pin.AbsX());
        max_y = std::max(max_y, blk_pin.AbsY());
      }
    }
    tot_hpwl += (max_x - min_x) + (max_y - min_y);
  }

  return tot_hpwl;
}

bool LGTetrisEx::StartPlacement() {
  BOOST_LOG_TRIVIAL(info)
    << "---------------------------------------\n"
    << "Start LGTetrisEx Legalization\n";

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  is_row_assignment_ = false;
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
    //GenMATLABTable("lg" + std::to_string(cur_iter_) + "_result.txt");
    ReportHPWL();
    if (is_success) {
      break;
    }
  }
  if (!is_success) {
    BOOST_LOG_TRIVIAL(info) << "Legalization fails\n";
  }

  BOOST_LOG_TRIVIAL(info)
    << "\033[0;36m"
    << "LGTetrisEx Legalization complete (" << cur_iter_ + 1 << ")\n"
    << "\033[0m";

  ReportHPWL();

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  BOOST_LOG_TRIVIAL(info)
    << "(wall time: " << wall_time << "s, cpu time: "
    << cpu_time << "s)\n";

  ReportMemory();

  return true;
}

bool LGTetrisEx::StartRowAssignment() {
  BOOST_LOG_TRIVIAL(info)
    << "---------------------------------------\n"
    << "Start Row Assignment\n";

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  is_row_assignment_ = true;
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
  BOOST_LOG_TRIVIAL(info)
    << "\033[0;36m"
    << "Row assignment complete (" << cur_iter_ + 1 << ")\n"
    << "\033[0m";

  if (!is_success) {
    BOOST_LOG_TRIVIAL(info) << "Placement illegal\n";
  }

  ReportHPWL();
  ReportBoundingBox();

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  BOOST_LOG_TRIVIAL(info)
    << "(wall time: " << wall_time << "s, cpu time: "
    << cpu_time << "s)\n";

  ReportMemory();

  return true;
}

void LGTetrisEx::GenAvailSpace(std::string const &name_of_file) {
  BOOST_LOG_TRIVIAL(info) << "Generating available space, dump result to: "
                          << name_of_file << "\n";
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " << name_of_file);
  ost << RegionLeft() << "\t"
      << RegionRight() << "\t"
      << RegionRight() << "\t"
      << RegionLeft() << "\t"
      << RegionBottom() << "\t"
      << RegionBottom() << "\t"
      << RegionTop() << "\t"
      << RegionTop() << "\n";
  for (int i = 0; i < tot_num_rows_; ++i) {
    auto &row = row_segments_[i];
    for (auto &seg: row) {
      ost << seg.lo << "\t"
          << seg.hi << "\t"
          << seg.hi << "\t"
          << seg.lo << "\t"
          << i * row_height_ + RegionBottom() << "\t"
          << i * row_height_ + RegionBottom() << "\t"
          << (i + 1) * row_height_ + RegionBottom() << "\t"
          << (i + 1) * row_height_ + RegionBottom() << "\n";
    }
  }

  for (auto &block: Blocks()) {
    if (block.IsMovable()) continue;
    ost << block.LLX() << "\t"
        << block.URX() << "\t"
        << block.URX() << "\t"
        << block.LLX() << "\t"
        << block.LLY() << "\t"
        << block.LLY() << "\t"
        << block.URY() << "\t"
        << block.URY() << "\n";
  }
}

}
