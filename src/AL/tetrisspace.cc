//
// Created by yihan on 7/12/2019.
//

#include "tetrisspace.h"

TetrisSpace::TetrisSpace(int left, int right, int bottom, int top, int rowHeight, int minWidth):
                  _left(left), _right(right), _bottom(bottom), _top(top), _rowHeight(rowHeight), _minWidth(minWidth) {
  _totNumRow = std::floor((_top-_bottom)/rowHeight);
  FreeSegmentList tmpRow(_left, _right, _minWidth);
  for (int i=0; i<_totNumRow; ++i) {
    freeSegmentRows.push_back(tmpRow);
  }
}

void TetrisSpace::findCommonSegments(int startRowNum, int endRowNum, FreeSegmentList &commonSegments) {
  // find the first common segment which can put new block into
  // 1. some basic checking
  if (endRowNum < startRowNum) {
    assert (endRowNum >= startRowNum);
  }
  if ((startRowNum < 0) || (endRowNum >= (int)(freeSegmentRows.size()))) {
    assert((startRowNum >= 0) && (endRowNum < (int)freeSegmentRows.size()));
  }
  commonSegments.copyFrom(freeSegmentRows[startRowNum]);
  for (int i = startRowNum+1; i<endRowNum; ++i) {
    commonSegments.applyMask(freeSegmentRows[i]);
    if (commonSegments.empty()) break;
  }
}

Loc2D TetrisSpace::findBlockLocation(double currentX, double currentY, int blockWidth, int blockHeight) {
  double minDisplacement = 1e30;
  int effectiveHeight = (int)(std::ceil((double)blockHeight/_rowHeight));
  int topRowNumToCheck = freeSegmentRows.size() - effectiveHeight;
  bool allRowFail = true;
  std::vector< Loc2D > candidateList;
  for (int i=0; i<topRowNumToCheck; ++i) {
    FreeSegmentList commonSegments;
    findCommonSegments(i, i+effectiveHeight, commonSegments);
    commonSegments.removeShortSeg(blockWidth);
    if (!commonSegments.empty()) {
      allRowFail = false;
      candidateList.emplace_back(-1,-1);
    } else {
      candidateList.emplace_back(commonSegments.head()->start(),i);
    }
  }

  Loc2D bestLoc(-1,-1);
  if (allRowFail) { // need to change this in the future
    return bestLoc;
  }
  for (auto &&loc: candidateList) {
    double displacement = std::fabs(currentX - loc.x) + std::fabs(currentY - loc.y);
    if (displacement < minDisplacement) {
      bestLoc = loc;
    }
  }

  for (int i = bestLoc.y; i < bestLoc.y + effectiveHeight; ++i) {
    freeSegmentRows[i].useSpace(bestLoc.x, effectiveHeight);
  }

  return bestLoc;
}

