//
// Created by Yihang Yang on 1/2/20.
//

#include "LGTetrisEx.h"

#include <cfloat>

#include <algorithm>
#include <list>

LGTetrisEx::LGTetrisEx()
    : Placer(),
      row_height_(1),
      legalize_from_left_(true),
      cur_iter_(0),
      max_iter_(100),
      k_width_(0.001),
      k_height_(0.001),
      k_left_(1),
      tot_num_rows_(0) {}

void LGTetrisEx::MergeIntervals(std::vector<std::vector<int>> &intervals) {
  /****
   * This member function comes from a solution I submitted to LeetCode, lol
   *
   * If two intervals overlap with each other, these two intervals will be merged into one
   *
   * This member function can merge a list of intervals
   * ****/
  int sz = intervals.size();
  if (sz <= 1) return;

  std::sort(intervals.begin(),
            intervals.end(),
            [](const std::vector<int> &inter1, const std::vector<int> &inter2) {
              return inter1[0] < inter2[0];
            });

  std::vector<std::vector<int>> res;

  int begin = intervals[0][0];
  int end = intervals[0][1];

  std::vector<int> tmp(2, 0);
  for (int i = 1; i < sz; ++i) {
    if (end < intervals[i][0]) {
      tmp[0] = begin;
      tmp[1] = end;
      res.push_back(tmp);
      begin = intervals[i][0];
    }
    if (end < intervals[i][1]) {
      end = intervals[i][1];
    }
  }

  tmp[0] = begin;
  tmp[1] = end;
  res.push_back(tmp);

  intervals = res;
}

void LGTetrisEx::InitLegalizer() {
  /****
   * 1. calculate the number of rows for a given row_height
   * 2. initialize white space available in rows
   * 3. initialize block contour to be the left contour
   * 4. allocate space for index_loc_list_
   * ****/
  tot_num_rows_ = (top_ - bottom_) / row_height_;

  std::vector<std::vector<std::vector<int>>> macro_segments;
  macro_segments.resize(tot_num_rows_);
  std::vector<int> tmp(2, 0);
  bool out_of_range;
  for (auto &&block: circuit_->block_list) {
    if (block.IsMovable()) continue;
    int ly = int(std::floor(block.LLY()));
    int uy = int(std::ceil(block.URY()));
    int lx = int(std::floor(block.LLX()));
    int ux = int(std::ceil(block.URX()));

    out_of_range = (ly >= RegionTop()) || (uy <= RegionBottom()) || (lx >= RegionRight()) || (ux <= RegionLeft());

    if (out_of_range) continue;

    int start_row = StartRow(ly);
    int end_row = EndRow(uy);

    start_row = std::max(0, start_row);
    end_row = std::min(tot_num_rows_ - 1, end_row);

    tmp[0] = std::max(RegionLeft(), lx);
    tmp[1] = std::min(RegionRight(), ux);
    if (tmp[1] > tmp[0]) {
      for (int i = start_row; i <= end_row; ++i) {
        macro_segments[i].push_back(tmp);
      }
    }
  }
  for (auto &intervals : macro_segments) {
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
      if (interval[0] == left_ && interval[1] < RegionRight()) {
        intermediate_seg_rows[i].push_back(interval[1]);
      }

      if (interval[0] > left_) {
        if (intermediate_seg_rows[i].empty()) {
          intermediate_seg_rows[i].push_back(left_);
        }
        intermediate_seg_rows[i].push_back(interval[0]);
        if (interval[1] < RegionRight()) {
          intermediate_seg_rows[i].push_back(interval[1]);
        }
      }
    }
    if (intermediate_seg_rows[i].size() % 2 == 1) {
      intermediate_seg_rows[i].push_back(right_);
    }
  }

  white_space_in_rows_.resize(tot_num_rows_);
  int min_blk_width = int(circuit_->MinWidth());
  for (int i = 0; i < tot_num_rows_; ++i) {
    int len = int(intermediate_seg_rows[i].size());
    white_space_in_rows_[i].reserve(len / 2);
    for (int j = 0; j < len; j += 2) {
      if (intermediate_seg_rows[i][j + 1] - intermediate_seg_rows[i][j] >= min_blk_width) {
        white_space_in_rows_[i].emplace_back(intermediate_seg_rows[i][j], intermediate_seg_rows[i][j + 1]);
      }
    }
  }

  //GenAvailSpace();

  block_contour_.resize(tot_num_rows_, left_);

  IndexLocPair<int> tmp_index_loc_pair(0, 0, 0);
  index_loc_list_.resize(circuit_->block_list.size(), tmp_index_loc_pair);
}

bool LGTetrisEx::IsSpaceLegal(int lo_x, int hi_x, int lo_row, int hi_row) {
  /****
   * This member function checks if the region specified by [lo_x, hi_x] and [lo_row, hi_row] is legal or not
   *
   * If this space overlaps with fixed macros or out of placement range, then this space is illegal.
   *
   * To determine this space is legal, one just need to show that every row is legal
   * ****/
  assert(lo_x <= hi_x);

  bool loc_out_range = (hi_x > right_) || (lo_x < left_) || (hi_row >= tot_num_rows_) || (lo_row < 0);
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
    seg_count = white_space_in_rows_[i].size();
    is_tmp_row_legal = false;
    for (int j = 0; j < seg_count; ++j) {
      if (white_space_in_rows_[i][j].lo <= lo_x && white_space_in_rows_[i][j].hi >= hi_x) {
        is_tmp_row_legal = true;
        break;
      }

      is_partial_cover_lo = white_space_in_rows_[i][j].lo > lo_x && white_space_in_rows_[i][j].lo < hi_x;
      is_partial_cover_hi = white_space_in_rows_[i][j].hi > lo_x && white_space_in_rows_[i][j].hi < hi_x;
      is_before_seg = white_space_in_rows_[i][j].lo >= hi_x;
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

void LGTetrisEx::UseSpaceLeft(Block const &block) {
  /****
   * Mark the space used by this block by changing the start point of available space in each related row
   * ****/
  int start_row = StartRow(int(block.LLY()));
  int end_row = EndRow(int(block.URY()));
  /*if (block.URY() > RegionTop()) {
    std::cout << "  ly:     " << int(block.LLY()) << "\n"
              << "  height: " << block.Height() << "\n"
              << "  top:    " << RegionTop() << "\n"
              << "  bottom: " << RegionBottom() << "\n"
              << "  name:   " << *block.Name() << "\n"
              << "  start_r:" << start_row << "\n"
              << "  end_r:  " << end_row << "\n"
              << "  tot_r:  " << tot_num_rows_ << "\n";
    GenMATLABTable("lg_result.txt");
    Assert(false, "Cannot use space out of range");
  }*/

  assert(block.URY() <= RegionTop());
  assert(end_row < tot_num_rows_);
  assert(start_row >= 0);

  int end_x = int(block.URX());
  for (int i = start_row; i <= end_row; ++i) {
    block_contour_[i] = end_x;
  }
}

bool LGTetrisEx::IsCurrentLocLegalLeft(Value2D<int> &loc, int width, int height) {
  /****
   * Returns whether the current location is legal
   *
   * 1. if the space itself is illegal, then return false
   * 2. if the space covers placed blocks, then return false
   * 3. otherwise, return true
   * ****/
  int start_row = StartRow(loc.y);
  int end_row = EndRow(loc.y + height);

  bool is_space_legal = IsSpaceLegal(loc.x, loc.x + width, start_row, end_row);
  if (!is_space_legal) {
    return false;
  }

  bool all_row_avail = true;
  for (int i = start_row; i <= end_row; ++i) {
    if (block_contour_[i] > loc.x) {
      all_row_avail = false;
      break;
    }
  }

  return all_row_avail;
}

int LGTetrisEx::WhiteSpaceBoundLeft(int lo_x, int hi_x, int lo_row, int hi_row) {
  /****
   * Returns the left boundary of the white space region where this block should be placed
   *
   * For each row, find the segment which is closest to [lo_x, hi_x]
   * If a segment is [lo_seg, hi_seg], the distance is defined as
   *        min(|lo_seg - lo_x| + |lo_seg - hi_x|, |hi_seg - lo_x| + |hi_seg - hi_x|)
   * ****/
  int white_space_bound = left_;

  int min_distance = INT_MAX;
  int tmp_bound;

  for (int i = lo_row; i <= hi_row; ++i) {
    tmp_bound = left_;
    for (auto &seg: white_space_in_rows_[i]) {
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

bool LGTetrisEx::FindLocLeft(Value2D<int> &loc, int width, int height) {
  /****
   * Returns whether a legal location can be found, and put the final location to @params loc
   * ****/
  bool is_successful;

  int blk_row_height;
  int left_block_bound;
  int left_white_space_bound;

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

  left_block_bound = (int) std::round(loc.x - k_left_ * width);
  //left_block_bound = loc.x;

  max_search_row = MaxRow(height);
  blk_row_height = HeightToRow(height);

  search_start_row = std::max(0, LocToRow(loc.y - 4 * height));
  search_end_row = std::min(max_search_row, LocToRow(loc.y + 5 * height));

  best_row = 0;
  best_loc_x = INT_MIN;
  min_cost = DBL_MAX;

  for (int tmp_start_row = search_start_row; tmp_start_row <= search_end_row; ++tmp_start_row) {
    tmp_end_row = tmp_start_row + blk_row_height - 1;
    //left_white_space_bound = left_;
    left_white_space_bound = WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_row, tmp_end_row);

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
  }

  int best_row_legal = 0;
  int best_loc_x_legal = INT_MIN;
  double min_cost_legal = DBL_MAX;
  bool is_loc_legal = IsSpaceLegal(best_loc_x, best_loc_x + width, best_row, best_row + blk_row_height - 1);

  if (!is_loc_legal) {
    int old_start_row = search_start_row;
    int old_end_row = search_end_row;
    int extended_range = cur_iter_ * blk_row_height;
    search_start_row = std::max(0, search_start_row - extended_range);
    search_end_row = std::min(max_search_row, search_end_row + extended_range);
    for (int tmp_start_row = search_start_row; tmp_start_row < old_start_row; ++tmp_start_row) {
      tmp_end_row = tmp_start_row + blk_row_height - 1;
      left_white_space_bound = WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_row, tmp_end_row);
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

      is_loc_legal = IsSpaceLegal(tmp_x, tmp_x + width, tmp_start_row, tmp_end_row);

      if (is_loc_legal) {
        if (tmp_cost < min_cost_legal) {
          best_loc_x_legal = tmp_x;
          best_row_legal = tmp_start_row;
          min_cost_legal = tmp_cost;
        }
      }
    }
    for (int tmp_start_row = old_end_row; tmp_start_row < search_end_row; ++tmp_start_row) {
      tmp_end_row = tmp_start_row + blk_row_height - 1;
      left_white_space_bound = WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_row, tmp_end_row);
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

      is_loc_legal = IsSpaceLegal(tmp_x, tmp_x + width, tmp_start_row, tmp_end_row);

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
  is_successful = IsSpaceLegal(best_loc_x, best_loc_x + width,
                               best_row, best_row + blk_row_height - 1);
  if (!is_successful) {
    if (best_loc_x_legal >= left_ && best_loc_x_legal <= right_ - width) {
      is_successful = IsSpaceLegal(best_loc_x_legal, best_loc_x_legal + width,
                                   best_row_legal, best_row_legal + blk_row_height - 1);
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

void LGTetrisEx::FastShiftLeft(int failure_point) {
  /****
   * This method is to FastShiftLeft() the blocks following the failure_point (included)
   * to reasonable locations in order to keep block orders
   *    1. when the current location of this block is illegal
   *    2. when there is no possible legal location on the right hand side of this block.
   * if failure_point is the first block
   * (this is not supposed to happen, if this happens, something is wrong with the global placement)
   *    we shift the bounding box of all blocks to the placement region,
   *    the bounding box left and bottom boundaries touch the left and bottom boundaries of placement region,
   *    note that the bounding box might be larger than the placement region, but should not be much larger
   * else:
   *    we shift the bounding box of the remaining blocks to the right hand side of the block just placed
   *    the bottom boundary of the bounding box will not be changed
   *    only the left boundary of the bounding box will be shifted to the right hand side of the block just placed
   * ****/
  std::vector<Block> &block_list = *BlockList();
  double bounding_left;
  if (failure_point == 0) {
    std::cout << "WARNING: unexpected case happens during legalization (failure point is 0)!\n";
  } else {
    double init_diff = index_loc_list_[failure_point - 1].x - index_loc_list_[failure_point].x;
    int failed_block = index_loc_list_[failure_point].num;
    bounding_left = block_list[failed_block].LLX();
    int last_placed_block = index_loc_list_[failure_point - 1].num;
    int left_new = (int) std::round(block_list[last_placed_block].LLX());
    //std::cout << left_new << "  " << bounding_left << "\n";
    int sz = index_loc_list_.size();
    int block_num;
    for (int i = failure_point; i < sz; ++i) {
      block_num = index_loc_list_[i].num;
      if (block_list[block_num].IsMovable()) {
        block_list[block_num].IncreX(left_new - bounding_left + init_diff);
      }
    }
  }
}

bool LGTetrisEx::LocalLegalizationLeft() {
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
   ****/
  bool is_successful = true;
  block_contour_.assign(block_contour_.size(), left_);
  std::vector<Block> &block_list = *BlockList();

  int sz = index_loc_list_.size();
  for (int i = 0; i < sz; ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].LLX() - k_width_ * block_list[i].Width() - k_height_ * block_list[i].Height();
    index_loc_list_[i].y = block_list[i].LLY();
  }
  std::sort(index_loc_list_.begin(), index_loc_list_.end());

  double init_x;
  double init_y;
  int height;
  int width;

  Value2D<int> res;
  bool is_current_loc_legal;
  bool is_legal_loc_found;

  int i;
  for (i = 0; i < sz; ++i) {
    //std::cout << i << "\n";
    auto &block = block_list[index_loc_list_[i].num];

    if (block.IsFixed()) continue;

    init_x = int(block.LLX());
    init_y = AlignedLocToRow(block.LLY());
    height = int(block.Height());
    width = int(block.Width());

    res.x = init_x;
    res.y = init_y;

    is_current_loc_legal = IsCurrentLocLegalLeft(res, width, height);

    if (!is_current_loc_legal) {
      is_legal_loc_found = FindLocLeft(res, width, height);
      if (!is_legal_loc_found) {
        is_successful = false;
        //std::cout << res.x << "  " << res.y << "  " << block.Num() << " left\n";
        //break;
      }
    }

    block.SetLoc(res.x, res.y);

    UseSpaceLeft(block);
  }

  /*if (!is_successful) {
    FastShiftLeft(i);
  }*/

  return is_successful;
}

void LGTetrisEx::UseSpaceRight(Block const &block) {
  int start_row = StartRow(int(block.LLY()));
  int end_row = EndRow(int(block.URY()));
  /*if (end_row >= block_contour_.size()) {
    std::cout << "  ly:     " << block.LLY() << "\n"
              << "  height: " << block.Height() << "\n"
              << "  top:    " << Top() << "\n"
              << "  bottom: " << Bottom() << "\n"
              << "  is legal: " << is_current_loc_legal << "\n";
    Assert(false, "Cannot use space out of range");
  }*/

  assert(block.URY() <= RegionTop());
  assert(end_row < tot_num_rows_);
  assert(start_row >= 0);

  int end_x = int(block.LLX());
  for (int r = start_row; r <= end_row; ++r) {
    block_contour_[r] = end_x;
  }
}

bool LGTetrisEx::IsCurrentLocLegalRight(Value2D<int> &loc, int width, int height) {
  bool is_space_legal = IsSpaceLegal(loc.x - width, loc.x, StartRow(loc.y), EndRow(loc.y + height));
  if (!is_space_legal) {
    return false;
  }

  bool all_row_avail = true;
  int start_row = StartRow(loc.y);
  int end_row = EndRow(loc.y + height);
  for (int i = start_row; i <= end_row; ++i) {
    if (block_contour_[i] < loc.x) {
      all_row_avail = false;
      break;
    }
  }

  return all_row_avail;
}

int LGTetrisEx::WhiteSpaceBoundRight(int lo_x, int hi_x, int lo_row, int hi_row) {
  /****
  * Returns the right boundary of the white space region where this block should be placed
  *
  * For each row, find the segment which is closest to [lo_x, hi_x]
  * If a segment is [lo_seg, hi_seg], the distance is defined as
  *        min(|lo_seg - lo_x| + |lo_seg - hi_x|, |hi_seg - lo_x| + |hi_seg - hi_x|)
  * ****/
  int white_space_bound = right_;

  int min_distance = INT_MAX;
  int tmp_bound;

  for (int i = lo_row; i <= hi_row; ++i) {
    tmp_bound = right_;
    for (auto &seg: white_space_in_rows_[i]) {
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

bool LGTetrisEx::FindLocRight(Value2D<int> &loc, int width, int height) {
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

  right_block_bound = (int) std::round(loc.x + k_left_ * width);
  //right_block_bound = loc.x;

  max_search_row = MaxRow(height);
  blk_row_height = HeightToRow(height);

  search_start_row = std::max(0, LocToRow(loc.y - 4 * height));
  search_end_row = std::min(max_search_row, LocToRow(loc.y + 5 * height));

  best_row = 0;
  best_loc_x = INT_MAX;
  min_cost = DBL_MAX;

  for (int tmp_start_row = search_start_row; tmp_start_row <= search_end_row; ++tmp_start_row) {
    tmp_end_row = tmp_start_row + blk_row_height - 1;
    right_white_space_bound = WhiteSpaceBoundRight(loc.x - width, loc.x, tmp_start_row, tmp_end_row);

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
  bool is_loc_legal = IsSpaceLegal(best_loc_x - width, best_loc_x, best_row, best_row + blk_row_height - 1);

  if (!is_loc_legal) {
    int old_start_row = search_start_row;
    int old_end_row = search_end_row;
    int extended_range = cur_iter_ * blk_row_height;
    search_start_row = std::max(0, search_start_row - extended_range);
    search_end_row = std::min(max_search_row, search_end_row + extended_range);
    for (int tmp_start_row = search_start_row; tmp_start_row < old_start_row; ++tmp_start_row) {
      tmp_end_row = tmp_start_row + blk_row_height - 1;
      right_white_space_bound = WhiteSpaceBoundRight(loc.x - width, loc.x, tmp_start_row, tmp_end_row);

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

      is_loc_legal = IsSpaceLegal(tmp_x - width, tmp_x, tmp_start_row, tmp_end_row);

      if (is_loc_legal) {
        if (tmp_cost < min_cost_legal) {
          best_loc_x_legal = tmp_x;
          best_row_legal = tmp_start_row;
          min_cost_legal = tmp_cost;
        }
      }
    }
    for (int tmp_start_row = old_end_row; tmp_start_row < search_end_row; ++tmp_start_row) {
      tmp_end_row = tmp_start_row + blk_row_height - 1;
      right_white_space_bound = WhiteSpaceBoundRight(loc.x - width, loc.x, tmp_start_row, tmp_end_row);

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

      is_loc_legal = IsSpaceLegal(tmp_x - width, tmp_x, tmp_start_row, tmp_end_row);

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
      is_successful = IsSpaceLegal(best_loc_x_legal - width, best_loc_x_legal,
                                   best_row, best_row + blk_row_height - 1);
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

void LGTetrisEx::FastShiftRight(int failure_point) {
  /****
   * This method is to FastShiftLeft() the blocks following the failure_point (included)
   * to reasonable locations in order to keep block orders
   *    1. when the current location of this block is illegal
   *    2. when there is no possible legal location on the right hand side of this block.
   * if failure_point is the first block
   * (this is not supposed to happen, if this happens, something is wrong with the global placement)
   *    we shift the bounding box of all blocks to the placement region,
   *    the bounding box left and bottom boundaries touch the left and bottom boundaries of placement region,
   *    note that the bounding box might be larger than the placement region, but should not be much larger
   * else:
   *    we shift the bounding box of the remaining blocks to the right hand side of the block just placed
   *    the bottom boundary of the bounding box will not be changed
   *    only the left boundary of the bounding box will be shifted to the right hand side of the block just placed
   * ****/
  std::vector<Block> &block_list = *BlockList();
  double bounding_right;
  if (failure_point == 0) {
    std::cout << "WARNING: unexpected case happens during legalization (reverse failure point is 0)!\n";

  } else {
    double init_diff = index_loc_list_[failure_point - 1].x - index_loc_list_[failure_point].x;
    bounding_right = index_loc_list_[failure_point].x;
    int last_placed_block = index_loc_list_[failure_point - 1].num;
    int right_new = (int) std::round(block_list[last_placed_block].URX());
    //std::cout << left_new << "  " << bounding_left << "\n";
    int sz = index_loc_list_.size();
    int block_num = -1;
    for (int i = failure_point; i < sz; ++i) {
      block_num = index_loc_list_[i].num;
      if (block_list[block_num].IsMovable()) {
        block_list[block_num].DecreX(right_new - bounding_right + init_diff);
      }
    }
  }
}

bool LGTetrisEx::LocalLegalizationRight() {
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
   ****/
  block_contour_.assign(block_contour_.size(), right_);
  std::vector<Block> &block_list = *BlockList();

  int sz = index_loc_list_.size();
  for (int i = 0; i < sz; ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].URX() + k_width_ * block_list[i].Width() + k_height_ * block_list[i].Height();
    index_loc_list_[i].y = block_list[i].LLY();
  }
  std::sort(index_loc_list_.begin(),
            index_loc_list_.end(),
            [](const IndexLocPair<int> &lhs, const IndexLocPair<int> &rhs) {
              return (lhs.x > rhs.x) || (lhs.x == rhs.x && lhs.y > rhs.y);
            });

  int init_x;
  int init_y;
  int height;
  int width;

  bool is_current_loc_legal;
  bool is_legal_loc_found;

  Value2D<int> res;
  bool is_successful = true;
  int i;
  for (i = 0; i < sz; ++i) {
    //std::cout << i << "\n";
    auto &block = block_list[index_loc_list_[i].num];
    if (block.IsFixed()) continue;

    init_x = int(block.URX());
    init_y = AlignedLocToRow(block.LLY());
    height = int(block.Height());
    width = int(block.Width());

    res.x = init_x;
    res.y = init_y;

    is_current_loc_legal = IsCurrentLocLegalRight(res, width, height);

    if (!is_current_loc_legal) {
      is_legal_loc_found = FindLocRight(res, width, height);
      if (!is_legal_loc_found) {
        is_successful = false;
        //std::cout << res.x << "  " << res.y << "  " << block.Num() << " right\n";
        //break;
      }
    }

    block.SetURX(res.x);
    block.SetLLY(res.y);

    UseSpaceRight(block);
  }

  /*if (!is_successful) {
    FastShiftRight(i);
  }*/

  return is_successful;
}

double LGTetrisEx::EstimatedHPWL(Block &block, int x, int y) {
  double max_x = x;
  double max_y = y;
  double min_x = x;
  double min_y = y;
  double tot_hpwl = 0;
  Net *net;
  for (auto &net_num: block.net_list) {
    net = &(circuit_->net_list[net_num]);
    if (net->P() > 100) continue;
    for (auto &blk_pin_pair: net->blk_pin_list) {
      if (blk_pin_pair.GetBlock() != &block) {
        min_x = std::min(min_x, blk_pin_pair.AbsX());
        min_y = std::min(min_y, blk_pin_pair.AbsY());
        max_x = std::max(max_x, blk_pin_pair.AbsX());
        max_y = std::max(max_y, blk_pin_pair.AbsY());
      }
    }
    tot_hpwl += (max_x - min_x) + (max_y - min_y);
  }

  return tot_hpwl;
}

void LGTetrisEx::StartPlacement() {
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "---------------------------------------\n"
              << "Start LGTetrisEx Legalization\n";
  }

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  InitLegalizer();

  bool is_success = false;
  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    if (legalize_from_left_) {
      is_success = LocalLegalizationLeft();
    } else {
      is_success = LocalLegalizationRight();
    }
    legalize_from_left_ = !legalize_from_left_;
    ++k_left_;
    //GenMATLABTable("lg" + std::to_string(cur_iter_) + "_result.txt");
    ReportHPWL(LOG_CRITICAL);
    if (is_success) {
      break;
    }
  }
  if (!is_success) {
    std::cout << "Legalization fails\n";
  }

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "\033[0;36m"
              << "LGTetrisEx Legalization complete (" << cur_iter_ + 1 << ")\n"
              << "\033[0m";
  }

  ReportHPWL(LOG_CRITICAL);

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("(wall time: %.4fs, cpu time: %.4fs)\n", wall_time, cpu_time);
  }

  ReportMemory(LOG_CRITICAL);
}

void LGTetrisEx::GenAvailSpace(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open output file: " + name_of_file);
  ost << RegionLeft() << "\t"
      << RegionRight() << "\t"
      << RegionRight() << "\t"
      << RegionLeft() << "\t"
      << RegionBottom() << "\t"
      << RegionBottom() << "\t"
      << RegionTop() << "\t"
      << RegionTop() << "\n";
  for (int i = 0; i < tot_num_rows_; ++i) {
    auto &row = white_space_in_rows_[i];
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

  for (auto &block: circuit_->block_list) {
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
