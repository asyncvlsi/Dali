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
#include "tetrisspace.h"

#include <cfloat>

namespace dali {

TetrisSpace::TetrisSpace(int32_t left,
                         int32_t right,
                         int32_t bottom,
                         int32_t top,
                         int32_t rowHeight,
                         int32_t minWidth) : scan_line_(left),
                                         left_(left),
                                         right_(right),
                                         bottom_(bottom),
                                         top_(top),
                                         row_height_(rowHeight),
                                         min_width_(minWidth) {
  tot_num_row_ = (top_ - bottom_) / rowHeight;
  top_ -= (top_ - bottom_) % rowHeight;
  for (int32_t i = 0; i < tot_num_row_; ++i) {
    FreeSegmentList tmpRow;
    free_segment_rows.push_back(tmpRow);
  }
  for (auto &row: free_segment_rows) {
    auto *seg_ptr = new FreeSegment(left_, right_);
    row.PushBack(seg_ptr);
    row.SetMinWidth(min_width_);
  }
}

int32_t TetrisSpace::ToStartRow(int32_t y_loc) {
  int32_t row_num = (y_loc - bottom_) / row_height_;
  if ((row_num < 0) || (row_num >= tot_num_row_)) {
    BOOST_LOG_TRIVIAL(info) << "Fatal error:" << std::endl;
    BOOST_LOG_TRIVIAL(info) << "  y_loc out of range, y_loc, bottom_, top_: "
                            << y_loc << "  " << bottom_ << "  "
                            << top_
                            << std::endl;
    DaliExpects(false, "ToStartRow conversion fail");
  }
  return (row_num);
}

int32_t TetrisSpace::ToEndRow(int32_t y_loc) {
  int32_t relative_y = y_loc - bottom_;
  int32_t row_num = relative_y / row_height_;
  if (relative_y % row_height_ == 0) {
    --row_num;
  }
  if ((row_num < 0) || (row_num >= tot_num_row_)) {
    BOOST_LOG_TRIVIAL(info) << "Fatal error:" << std::endl;
    BOOST_LOG_TRIVIAL(info) << "  y_loc out of range, y_loc, bottom_, top_: "
                            << y_loc << "  " << bottom_ << "  "
                            << top_
                            << std::endl;
    BOOST_LOG_TRIVIAL(info) << "  relative y location:" << relative_y
                            << std::endl;
    BOOST_LOG_TRIVIAL(info) << "  row height:" << row_height_ << std::endl;
    BOOST_LOG_TRIVIAL(info) << "  row num: " << row_num << std::endl;
    BOOST_LOG_TRIVIAL(info) << "  total row num: " << tot_num_row_ << std::endl;
    DaliExpects(false, "ToEndRow conversion fail");
  }
  return (row_num);
}

void TetrisSpace::UseSpace(int32_t llx, int32_t lly, int32_t width, int32_t height) {
  int32_t start_row = ToStartRow(lly);
  int32_t end_row = ToEndRow(lly + height);
  for (int32_t i = start_row; i <= end_row; ++i) {
    free_segment_rows[i].UseSpace(llx, width);
  }
}

void TetrisSpace::FindCommonSegments(int32_t startRowNum,
                                     int32_t endRowNum,
                                     FreeSegmentList &commonSegments) {
  if (endRowNum < startRowNum) {
    assert (endRowNum >= startRowNum);
  }
  if ((startRowNum < 0) || (endRowNum > (int32_t) (free_segment_rows.size()))) {
    assert((startRowNum >= 0) && (endRowNum <= (int32_t) free_segment_rows.size()));
  }
  for (int32_t i = startRowNum; i < endRowNum; ++i) {
    commonSegments.ApplyMask(free_segment_rows[i]);
    if (commonSegments.Empty()) break;
  }
}

bool TetrisSpace::IsSpaceAvail(int32_t llx, int32_t lly, int32_t width, int32_t height) {
  /****
   * 1. Check if the current location is in the placement region, if not return false. We need to find a new location;
   * If in this region:
   * 2. We need to check from the bottom row to the top row, which occupied by this block to know if for each row, the space is available;
   * 3. If one of the answer is no, return false;
   * 4. If all of them are yes, make the region as used.
   * ****/

  bool out_range =
      (llx + width > right_) || (llx < left_) || (lly + height > top_)
          || (lly < bottom_);
  if (out_range) {
    return false;
  }

  int32_t start_row = ToStartRow(lly);
  int32_t end_row = ToEndRow(lly + height);
  if (end_row >= tot_num_row_) {
    BOOST_LOG_TRIVIAL(info) << "Fatal error:\n";
    BOOST_LOG_TRIVIAL(info) << "  row info:   " << start_row << "  " << end_row
                            << "  " << tot_num_row_ << "\n";
    BOOST_LOG_TRIVIAL(info) << "  lly info: " << lly << "  " << lly + height
                            << "  " << top_ << "\n";
    exit(1);
  }
  bool all_row_avail = true;
  for (int32_t i = start_row; i <= end_row; ++i) {
    if (!free_segment_rows[i].IsSpaceAvail(llx, width)) {
      all_row_avail = false;
      break;
    }
  }
  if (all_row_avail) {
    UseSpace(llx, lly, width, height);
  }
  return all_row_avail;
}

bool TetrisSpace::FindBlockLoc(int32_t llx,
                               int32_t lly,
                               int32_t width,
                               int32_t height,
                               int2d &result_loc) {
  if (llx < left_) {
    scan_line_ = left_;
  } else {
    scan_line_ = llx;
  }
  if (scan_line_ >= right_) {
    scan_line_ = right_;
  }
  double min_cost = DBL_MAX;
  int32_t effective_height = (int32_t) (std::ceil((double) height / row_height_));
  int32_t top_row_to_check =
      (int32_t) (free_segment_rows.size()) - (effective_height - 1);
  bool all_row_fail = true;
  int32_t min_disp_x;
  std::vector<int2d> candidate_list;

  int32_t min_row = ToStartRow(std::max(bottom_, lly - 2 * height));
  int32_t max_row =
      ToEndRow(std::min(top_, lly + 2 * height)) - height / row_height_;

  for (int32_t i = min_row; i < max_row; ++i) {
    FreeSegmentList common_segments(scan_line_, right_, min_width_);
    FindCommonSegments(i, i + effective_height, common_segments);
    common_segments.RemoveShortSeg(width);
    if (common_segments.Empty()) {
      candidate_list.emplace_back(-1, -1);
    } else {
      all_row_fail = false;
      min_disp_x = common_segments.MinDispLoc(width);
      candidate_list.emplace_back(min_disp_x, i);
    }
  }

  while (all_row_fail) {
    int32_t old_min_row = min_row;
    int32_t old_max_row = max_row;
    min_row = std::max(0, min_row - 2 * height);
    max_row = std::min(top_row_to_check, max_row + 2 * height);
    for (int32_t i = min_row; i < max_row; ++i) {
      if (i >= old_min_row && i < old_max_row) continue;
      FreeSegmentList common_segments(scan_line_, right_, min_width_);
      FindCommonSegments(i, i + effective_height, common_segments);
      common_segments.RemoveShortSeg(width);
      if (common_segments.Empty()) {
        candidate_list.emplace_back(-1, -1);
      } else {
        all_row_fail = false;
        min_disp_x = common_segments.MinDispLoc(width);
        candidate_list.emplace_back(min_disp_x, i);
      }
    }
    if (min_row <= 0 && max_row >= top_row_to_check) break;
  }

  int2d best_loc(-1, -1);
  if (all_row_fail) { // need to change this in the future
    return false;
  }
  for (auto &loc: candidate_list) {
    if (loc.x == -1 && loc.y == -1) {
      continue;
    }
    double displacement = std::fabs(llx - loc.x)
        + std::fabs(lly - (loc.y * row_height_ + bottom_));
    if (displacement < min_cost) {
      min_cost = displacement;
      best_loc = loc;
    }
  }

  int32_t x_loc = best_loc.x;
  int32_t y_loc = best_loc.y * row_height_ + bottom_;
  UseSpace(x_loc, y_loc, width, height);

  int32_t overhead = effective_height * row_height_ - height;
  best_loc.y = best_loc.y * row_height_ + bottom_ + overhead / 2;
  result_loc = best_loc;
  return true;
}

void TetrisSpace::Show() {
  for (auto &row: free_segment_rows) {
    row.Show();
  }
}

}
