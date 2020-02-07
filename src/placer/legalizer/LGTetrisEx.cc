//
// Created by Yihang Yang on 1/2/20.
//

#include "LGTetrisEx.h"

#include <cfloat>

#include <algorithm>
#include <list>

#include "placer/legalizer/LGTetris/freesegmentlist.h"

LGTetrisEx::LGTetrisEx()
    : Placer(),
      row_height_(1),
      legalize_from_left_(true),
      cur_iter_(0),
      max_iter_(10),
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
            [](std::vector<int> &inter1, std::vector<int> &inter2) {
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
  tot_num_rows_ = (top_ - bottom_) / row_height_ + 1;

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
  for (int i = 0; i < tot_num_rows_; ++i) {
    int len = int(intermediate_seg_rows[i].size());
    white_space_in_rows_[i].reserve(len / 2);
    for (int j = 0; j < len; j += 2) {
      white_space_in_rows_[i].emplace_back(intermediate_seg_rows[i][j], intermediate_seg_rows[i][j + 1]);
    }
  }

  GenAvailSpace();

  block_contour_.resize(tot_num_rows_, left_);

  IndexLocPair<int> tmp_index_loc_pair(0, 0, 0);
  index_loc_list_.resize(circuit_->block_list.size(), tmp_index_loc_pair);
}

void LGTetrisEx::UseSpace(Block const &block) {
  /****
   * Mark the space used by this block by changing the start point of available space in each related row
   * ****/
  auto start_row = (unsigned int) (block.LLY() - RegionBottom());
  unsigned int end_row = start_row + block.Height() - 1;
  if (end_row >= block_contour_.size()) {
    //std::cout << "  ly:     " << int(block.LLY())       << "\n"
    //          << "  height: " << block.Height()   << "\n"
    //          << "  top:    " << Top()    << "\n"
    //          << "  bottom: " << Bottom() << "\n";
    Assert(false, "Cannot use space out of range");
  }

  int end_x = int(block.URX());
  for (unsigned int i = start_row; i <= end_row; ++i) {
    block_contour_[i] = end_x;
  }
}

bool LGTetrisEx::IsCurrentLocLegal(Value2D<int> &loc, int width, int height) {
  bool loc_out_range = (loc.x + width > right_) || (loc.x < left_) || (loc.y + height > top_) || (loc.y < bottom_);
  if (loc_out_range) {
    return false;
  }

  bool all_row_avail = true;
  int start_row = loc.y - bottom_;
  int end_row = start_row + height - 1;
  for (int i = start_row; i <= end_row; ++i) {
    if (block_contour_[i] > loc.x) {
      all_row_avail = false;
      break;
    }
  }

  return all_row_avail;
}

bool LGTetrisEx::FindLoc(Value2D<int> &loc, int width, int height) {
  bool is_successful = true;

  int init_row;
  int left_bound;

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

  left_bound = (int) std::round(loc.x - k_left_ * width);
  //left_bound = loc.x;

  init_row = int(loc.y - bottom_);
  max_search_row = top_ - bottom_ - height;

  search_start_row = std::max(0, init_row - 4 * height);
  search_end_row = std::min(max_search_row, init_row + 5 * height);

  best_row = 0;
  best_loc_x = INT_MIN;
  min_cost = DBL_MAX;

  for (int tmp_start_row = search_start_row; tmp_start_row <= search_end_row; ++tmp_start_row) {
    tmp_end_row = tmp_start_row + height - 1;
    tmp_x = std::max(left_, left_bound);

    for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
      tmp_x = std::max(tmp_x, block_contour_[n]);
    }

    //if (tmp_x + width > right_) continue;

    tmp_y = tmp_start_row + bottom_;
    //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

    tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
    if (tmp_cost < min_cost) {
      best_loc_x = tmp_x;
      best_row = tmp_start_row;
      min_cost = tmp_cost;
    }
  }

  if (best_loc_x < left_ || best_loc_x + width > right_) {
    int old_start_row = search_start_row;
    int old_end_row = search_end_row;
    search_start_row = std::max(0, search_start_row - cur_iter_ * height);
    search_end_row = std::min(max_search_row, search_end_row + cur_iter_ * height);
    for (int tmp_start_row = search_start_row; tmp_start_row < old_start_row; ++tmp_start_row) {
      tmp_end_row = tmp_start_row + height - 1;
      tmp_x = std::max(left_, left_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::max(tmp_x, block_contour_[n]);
      }

      if (tmp_x + width > right_) continue;

      tmp_y = tmp_start_row + bottom_;
      //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

      tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
      if (tmp_cost < min_cost) {
        best_loc_x = tmp_x;
        best_row = tmp_start_row;
        min_cost = tmp_cost;
      }
    }
    for (int tmp_start_row = old_end_row; tmp_start_row < search_end_row; ++tmp_start_row) {
      tmp_end_row = tmp_start_row + height - 1;
      tmp_x = std::max(left_, left_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::max(tmp_x, block_contour_[n]);
      }

      if (tmp_x + width > right_) continue;

      tmp_y = tmp_start_row + bottom_;
      //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

      tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
      if (tmp_cost < min_cost) {
        best_loc_x = tmp_x;
        best_row = tmp_start_row;
        min_cost = tmp_cost;
      }
    }

  }

  // if still cannot find a legal location, enter fail mode
  is_successful = (best_loc_x >= left_) && (best_loc_x + width <= right_);

  loc.x = best_loc_x;
  loc.y = best_row + bottom_;

  return is_successful;
}

void LGTetrisEx::FastShift(int failure_point) {
  /****
   * This method is to FastShift() the blocks following the failure_point (included)
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

bool LGTetrisEx::LocalLegalization() {
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
    init_y = int(block.LLY());
    height = int(block.Height());
    width = int(block.Width());

    res.x = init_x;
    res.y = init_y;

    is_current_loc_legal = IsCurrentLocLegal(res, width, height);

    if (!is_current_loc_legal) {
      is_legal_loc_found = FindLoc(res, width, height);
      if (!is_legal_loc_found) {
        is_successful = false;
        //break;
      }
    }

    block.SetLoc(res.x, res.y);

    UseSpace(block);
  }

  /*if (!is_successful) {
    FastShift(i);
  }*/

  return is_successful;
}

void LGTetrisEx::UseSpaceRight(Block const &block) {
  auto start_row = int(block.LLY() - RegionBottom());
  unsigned int end_row = start_row + block.Height() - 1;
  /*if (end_row >= block_contour_.size()) {
    std::cout << "  ly:     " << block.LLY() << "\n"
              << "  height: " << block.Height() << "\n"
              << "  top:    " << Top() << "\n"
              << "  bottom: " << Bottom() << "\n"
              << "  is legal: " << is_current_loc_legal << "\n";
    Assert(false, "Cannot use space out of range");
  }*/

  assert(end_row < block_contour_.size());
  assert(start_row >= 0);

  int end_x = int(block.LLX());
  for (unsigned int r = start_row; r <= end_row; ++r) {
    block_contour_[r] = end_x;
  }
}

bool LGTetrisEx::IsCurrentLocLegalRight(Value2D<int> &loc, int width, int height) {
  bool loc_out_range = (loc.x > right_) || (loc.x - width < left_) || (loc.y + height > top_) || (loc.y < bottom_);
  //std::cout << loc.y + height << "  " << loc_out_range << "\n";
  if (loc_out_range) {
    return false;
  }

  bool all_row_avail = true;
  int start_row = loc.y - bottom_;
  int end_row = start_row + height - 1;
  for (int i = start_row; i <= end_row; ++i) {
    if (block_contour_[i] < loc.x) {
      all_row_avail = false;
      break;
    }
  }

  return all_row_avail;
}

bool LGTetrisEx::FindLocRight(Value2D<int> &loc, int width, int height) {
  bool is_successful = true;

  int init_row;
  int right_bound;

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

  right_bound = (int) std::round(loc.x + k_left_ * width);
  //right_bound = loc.x;

  init_row = int(loc.y - bottom_);
  max_search_row = top_ - bottom_ - height;

  search_start_row = std::max(0, init_row - 4 * height);
  search_end_row = std::min(max_search_row, init_row + 5 * height);

  best_row = 0;
  best_loc_x = INT_MAX;
  min_cost = DBL_MAX;

  for (int tmp_start_row = search_start_row; tmp_start_row <= search_end_row; ++tmp_start_row) {
    tmp_end_row = tmp_start_row + height - 1;
    tmp_x = std::min(right_, right_bound);

    for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
      tmp_x = std::min(tmp_x, block_contour_[n]);
    }

    //if (tmp_x - width < left_) continue;

    tmp_y = tmp_start_row + bottom_;
    //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

    tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
    if (tmp_cost < min_cost) {
      best_loc_x = tmp_x;
      best_row = tmp_start_row;
      min_cost = tmp_cost;
    }
  }

  if (best_loc_x > right_ || best_loc_x - width < left_) {
    int old_start_row = search_start_row;
    int old_end_row = search_end_row;
    search_start_row = std::max(0, search_start_row - cur_iter_ * height);
    search_end_row = std::min(max_search_row, search_end_row + cur_iter_ * height);
    for (int tmp_start_row = search_start_row; tmp_start_row <= old_start_row; ++tmp_start_row) {
      tmp_end_row = tmp_start_row + height - 1;
      tmp_x = std::min(right_, right_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::min(tmp_x, block_contour_[n]);
      }

      if (tmp_x - width < left_) continue;

      tmp_y = tmp_start_row + bottom_;
      //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

      tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
      if (tmp_cost < min_cost) {
        best_loc_x = tmp_x;
        best_row = tmp_start_row;
        min_cost = tmp_cost;
      }
    }
    for (int tmp_start_row = old_end_row; tmp_start_row <= search_end_row; ++tmp_start_row) {
      tmp_end_row = tmp_start_row + height - 1;
      tmp_x = std::min(right_, right_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::min(tmp_x, block_contour_[n]);
      }

      if (tmp_x - width < left_) continue;

      tmp_y = tmp_start_row + bottom_;
      //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

      tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);
      if (tmp_cost < min_cost) {
        best_loc_x = tmp_x;
        best_row = tmp_start_row;
        min_cost = tmp_cost;
      }
    }

  }

  // if still cannot find a legal location, enter fail mode
  is_successful = (best_loc_x - width >= left_) || (best_loc_x <= right_);

  loc.x = best_loc_x;
  loc.y = best_row + bottom_;

  return is_successful;
}

void LGTetrisEx::FastShiftRight(int failure_point) {
  /****
   * This method is to FastShift() the blocks following the failure_point (included)
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
    init_y = int(block.LLY());
    height = int(block.Height());
    width = int(block.Width());

    res.x = init_x;
    res.y = init_y;

    is_current_loc_legal = IsCurrentLocLegalRight(res, width, height);

    if (!is_current_loc_legal) {
      is_legal_loc_found = FindLocRight(res, width, height);
      if (!is_legal_loc_found) {
        is_successful = false;
        //break;
      }
    }

    block.SetURX(res.x);
    block.SetLLY(res.y);

    //std::cout << res.x << "  " << res.y << "  " << block.Num() << "\n";

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
      is_success = LocalLegalization();
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
    std::cout << "(wall time: "
              << wall_time << "s, cpu time: "
              << cpu_time << "s)\n";
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
