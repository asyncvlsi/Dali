//
// Created by yihan on 7/12/2019.
//

#include "tetrisspace.h"

TetrisSpace::TetrisSpace(int left, int right, int bottom, int top, int rowHeight, int minWidth):
    scan_line_(left),
    left_(left),
    right_(right),
    bottom_(bottom),
    top_(top),
    row_height_(rowHeight),
    min_width_(minWidth) {
  tot_num_row_ = std::floor((top_-bottom_)/rowHeight);
  for (int i=0; i<tot_num_row_; ++i) {
    FreeSegmentList tmpRow;
    free_segment_rows.push_back(tmpRow);
  }
  for (auto &&row:free_segment_rows) {
    auto* seg_ptr = new FreeSegment(left_, right_);
    row.PushBack(seg_ptr);
    row.SetMinWidth(min_width_);
  }
}

void TetrisSpace::FindCommonSegments(int startRowNum, int endRowNum, FreeSegmentList &commonSegments) {
  if (endRowNum < startRowNum) {
    assert (endRowNum >= startRowNum);
  }
  if ((startRowNum < 0) || (endRowNum > (int)(free_segment_rows.size()))) {
    assert((startRowNum >= 0) && (endRowNum <= (int)free_segment_rows.size()));
  }
  for (int i = startRowNum; i<endRowNum; ++i) {
    commonSegments.ApplyMask(free_segment_rows[i]);
    if (commonSegments.Empty()) break;
  }
}

bool TetrisSpace::IsSpaceAvail(int x_loc, int y_loc, int width, int height) {
  int start_row = std::floor((y_loc-bottom_)/row_height_);
  int end_row = start_row + (int)(std::ceil((double)height/row_height_)) - 1;
  bool all_row_avail = true;
  for (int i=start_row; i<= end_row; ++i) {
    if (!free_segment_rows[i].IsSpaceAvail(x_loc, width)) {
      all_row_avail = false;
      break;
    }
  }
  if (all_row_avail) {
    for (int i = start_row; i < end_row; ++i) {
      free_segment_rows[i].UseSpace(x_loc, width);
    }
  }
  return all_row_avail;
}

bool TetrisSpace::FindBlockLoc(double current_x, double current_y, int block_width, int block_height, Loc2D &result_loc) {
  scan_line_ = (int)(std::round(current_x));
  double min_cost = 1e30;
  int effective_height = (int)(std::ceil((double)block_height/row_height_));
  int top_row_to_check = (int)(free_segment_rows.size()) - effective_height + 1;
  bool all_row_fail = true;
  int min_disp_x;
  std::vector< Loc2D > candidate_list;
  for (int i=0; i<top_row_to_check; ++i) {
    FreeSegmentList common_segments(scan_line_, right_, min_width_);
    FindCommonSegments(i, i + effective_height, common_segments);
    common_segments.RemoveShortSeg(block_width);
    if (common_segments.Empty()) {
      candidate_list.emplace_back(-1, -1);
    } else {
      all_row_fail = false;
      min_disp_x = common_segments.MinDispLoc(scan_line_, block_width);
      candidate_list.emplace_back(min_disp_x, i);
    }
  }

  Loc2D best_loc(-1, -1);
  if (all_row_fail) { // need to change this in the future
    return false;
  }
  for (auto &&loc: candidate_list) {
    if (loc.x == -1 && loc.y == -1) {
      continue;
    }
    double displacement = std::fabs(current_x - loc.x) + std::fabs(current_y - (loc.y*row_height_ + bottom_));
    if (displacement < min_cost) {
      min_cost = displacement;
      best_loc = loc;
    }
  }

  for (int i = best_loc.y; i < best_loc.y + effective_height; ++i) {
    free_segment_rows[i].UseSpace(best_loc.x, block_width);
  }

  int overhead = effective_height * row_height_ - block_height;
  best_loc.y = best_loc.y*row_height_ + bottom_ + overhead/2;
  result_loc = best_loc;
  return true;
}

void TetrisSpace::Show() {
  for (auto &&row: free_segment_rows) {
    row.Show();
  }
}