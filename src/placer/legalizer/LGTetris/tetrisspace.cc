//
// Created by yihan on 7/12/2019.
//

#include "tetrisspace.h"

TetrisSpace::TetrisSpace(int left, int right, int bottom, int top, int rowHeight, int minWidth):
    left_(left), right_(right), bottom_(bottom), top_(top), row_height_(rowHeight), min_width_(minWidth) {
  tot_num_row_ = std::floor((top_-bottom_)/rowHeight);
  for (int i=0; i<tot_num_row_; ++i) {
    FreeSegmentList tmpRow;
    free_segment_rows.push_back(tmpRow);
  }
  for (auto &&row:free_segment_rows) {
    auto* seg_ptr = new FreeSegment(left_, right_);
    row.push_back(seg_ptr);
    row.setminWidth(min_width_);

  }
}

void TetrisSpace::FindCommonSegments(int startRowNum, int endRowNum, FreeSegmentList &commonSegments) {
  if (endRowNum < startRowNum) {
    assert (endRowNum >= startRowNum);
  }
  if ((startRowNum < 0) || (endRowNum > (int)(free_segment_rows.size()))) {
    assert((startRowNum >= 0) && (endRowNum <= (int)free_segment_rows.size()));
  }
  commonSegments.copyFrom(free_segment_rows[startRowNum]);
  for (int i = startRowNum+1; i<endRowNum; ++i) {
    commonSegments.applyMask(free_segment_rows[i]);
    if (commonSegments.empty()) break;
  }
}

bool TetrisSpace::IsSpaceAvail(double x_loc, double y_loc, int width, int height) {
  int start_row = std::floor((y_loc-bottom_)/row_height_);
  int end_row = start_row + (int)(std::ceil((double)height/row_height_));
  for (int i=start_row; i<= end_row; ++i) {
    if (!free_segment_rows[i].IsSpaceAvail(x_loc, y_loc, width, height)) {
      return false;
    }
  }
  return true;
}

bool TetrisSpace::FindBlockLoc(double currentX, double currentY, int blockWidth, int blockHeight, Loc2D result_loc) {
  //std::cout << "Block dimension: " << blockWidth << " " << blockHeight << std::endl;
  double minDisplacement = 1e30;
  int effective_height = (int)(std::ceil((double)blockHeight/row_height_));
  //std::cout << "effective height: " << effective_height << std::endl;
  int topRowNumToCheck = (int)(free_segment_rows.size()) - effective_height + 1;
  bool allRowFail = true;
  std::vector< Loc2D > candidateList;
  for (int i=0; i<topRowNumToCheck; ++i) {
    FreeSegmentList commonSegments;
    FindCommonSegments(i, i + effective_height, commonSegments);
    //std::cout << i*row_height_ + bottom_ << std::endl;
    commonSegments.removeShortSeg(blockWidth);
    if (commonSegments.empty()) {
      candidateList.emplace_back(-1,-1);
    } else {
      allRowFail = false;
      candidateList.emplace_back(commonSegments.head()->start(),i);
    }
  }

  Loc2D bestLoc(-1,-1);
  //std::cout << "allRowFail: " << allRowFail << std::endl;
  if (allRowFail) { // need to change this in the future
    return false;
  }
  for (auto &&loc: candidateList) {
    if (loc.x == -1 && loc.y == -1) {
      continue;
    }
    double displacement = std::fabs(currentX - loc.x) + std::fabs(currentY - (loc.y*row_height_ + bottom_));
    if (displacement < minDisplacement) {
      minDisplacement = displacement;
      bestLoc = loc;
    }
  }
  //std::cout << "best loc: " << bestLoc.x << " " << bestLoc.y*row_height_ + bottom_ << std::endl;

  for (int i = bestLoc.y; i < bestLoc.y + effective_height; ++i) {
    //free_segment_rows[i].Show();
    //std::cout << "Space to use, x, y, length: "
    //          << bestLoc.x << " " << bestLoc.y*row_height_ + bottom_ << " " << blockWidth << std::endl;
    free_segment_rows[i].useSpace(bestLoc.x, blockWidth);
  }

  int overhead = effective_height * row_height_ - blockHeight;
  bestLoc.y = bestLoc.y*row_height_ + bottom_ + overhead/2;
  result_loc = bestLoc;
  return true;
}

void TetrisSpace::Show() {
  for (auto &&row: free_segment_rows) {
    row.Show();
  }
}