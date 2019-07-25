//
// Created by yihan on 7/12/2019.
//

#include "tetrisspace.h"

TetrisSpace::TetrisSpace(int left, int right, int bottom, int top, int rowHeight, int minWidth):
                  _left(left), _right(right), _bottom(bottom), _top(top), _rowHeight(rowHeight), _minWidth(minWidth) {
  _totNumRow = std::floor((_top-_bottom)/rowHeight);
  for (int i=0; i<_totNumRow; ++i) {
    FreeSegmentList tmpRow;
    freeSegmentRows.push_back(tmpRow);
  }
  for (auto &&row:freeSegmentRows) {
    auto* seg_ptr = new FreeSegment(_left, _right);
    row.push_back(seg_ptr);
    row.setminWidth(_minWidth);

  }
}

void TetrisSpace::findCommonSegments(int startRowNum, int endRowNum, FreeSegmentList &commonSegments) {
  if (endRowNum < startRowNum) {
    assert (endRowNum >= startRowNum);
  }
  if ((startRowNum < 0) || (endRowNum > (int)(freeSegmentRows.size()))) {
    assert((startRowNum >= 0) && (endRowNum <= (int)freeSegmentRows.size()));
  }
  commonSegments.copyFrom(freeSegmentRows[startRowNum]);
  for (int i = startRowNum+1; i<endRowNum; ++i) {
    commonSegments.applyMask(freeSegmentRows[i]);
    if (commonSegments.empty()) break;
  }
}

Loc2D TetrisSpace::findBlockLocation(double currentX, double currentY, int blockWidth, int blockHeight) {
  //std::cout << "Block dimension: " << blockWidth << " " << blockHeight << std::endl;
  double minDisplacement = 1e30;
  int effectiveHeight = (int)(std::ceil((double)blockHeight/_rowHeight));
  //std::cout << "effective height: " << effectiveHeight << std::endl;
  int topRowNumToCheck = (int)(freeSegmentRows.size()) - effectiveHeight + 1;
  bool allRowFail = true;
  std::vector< Loc2D > candidateList;
  for (int i=0; i<topRowNumToCheck; ++i) {
    FreeSegmentList commonSegments;
    findCommonSegments(i, i+effectiveHeight, commonSegments);
    //std::cout << i*_rowHeight + _bottom << std::endl;
    commonSegments.removeShortSeg(blockWidth);
    //commonSegments.show();
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
    return bestLoc;
  }
  for (auto &&loc: candidateList) {
    if (loc.x == -1 && loc.y == -1) {
      continue;
    }
    double displacement = std::fabs(currentX - loc.x) + std::fabs(currentY - (loc.y*_rowHeight + _bottom));
    if (displacement < minDisplacement) {
      minDisplacement = displacement;
      bestLoc = loc;
    }
  }
  //std::cout << "best loc: " << bestLoc.x << " " << bestLoc.y*_rowHeight + _bottom << std::endl;

  for (int i = bestLoc.y; i < bestLoc.y + effectiveHeight; ++i) {
    //freeSegmentRows[i].show();
    //std::cout << "Space to use, x, y, length: "
    //          << bestLoc.x << " " << bestLoc.y*_rowHeight + _bottom << " " << blockWidth << std::endl;
    freeSegmentRows[i].useSpace(bestLoc.x, blockWidth);
  }

  int overhead = effectiveHeight * _rowHeight - blockHeight;
  bestLoc.y = bestLoc.y*_rowHeight + _bottom + overhead/2;
  return bestLoc;
}

void TetrisSpace::show() {
  for (auto &&row: freeSegmentRows) {
    row.show();
  }
}