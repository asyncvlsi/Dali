//
// Created by yihan on 7/12/2019.
//

#include "TetrisSpace.h"

TetrisSpace::TetrisSpace(int left, int right, int bottom, int top, int rowHeight, int minWidth):
                  _left(left), _right(right), _bottom(bottom), _top(top), _rowHeight(rowHeight), _minWidth(minWidth) {
  _totNumRow = std::floor((_top-_bottom)/rowHeight);
  FreeSegment *tmpSpaceRange_ptr = nullptr;
  for (int i=0; i<_totNumRow; ++i) {
    freeSegmentRows.push_back(tmpSpaceRange_ptr);
  }
  for (auto &&spaceRange_ptr: freeSegmentRows) {
    spaceRange_ptr = new FreeSegment(_left, _right, _minWidth);
  }
}

bool TetrisSpace::updateCommonSegment(int rowNum, std::vector< int > &commonSegments) {

  return true;
}

bool TetrisSpace::findCommonSegments(int startRowNum, int endRowNum, int blockWidth, std::vector< int > &commonSegments) {
  // find the first common segment which can put new block into
  // 1. some basic checking
  if ((startRowNum < 0) || (endRowNum > (int)(freeSegmentRows.size()) - 1)) {
    return false;
  }
  bool isFindCommonSegSuccess = false;
  for (FreeSegment *curSeg_ptr = freeSegmentRows[startRowNum]; curSeg_ptr != nullptr; curSeg_ptr = curSeg_ptr->next()){
    if (curSeg_ptr->length() < blockWidth){
      continue;
    }
    std::vector< int > tmpCommonSegment;
    tmpCommonSegment.push_back(curSeg_ptr->start());
    tmpCommonSegment.push_back(curSeg_ptr->end());
    // for each of the following rows, update this temporary common segment list
    isFindCommonSegSuccess = true;
    for (int i=startRowNum+1; i<=endRowNum; ++i) {
      if (!updateCommonSegment(i, tmpCommonSegment)) {
        isFindCommonSegSuccess = false;
        break;
      }
    }
    if (isFindCommonSegSuccess) {
      //write data into commonSegments
      break;
    }
  }
  return isFindCommonSegSuccess;
}

Loc2D TetrisSpace::findBlockLocation(double currentX, double currentY, int blockWidth, int blockHeight) {
  double minDisplacement = 1e30;
  int minRowNum = -1;
  int effectiveHeight = (int)(std::ceil((double)blockHeight/_rowHeight));
  int topRowNumToCheck = freeSegmentRows.size() - effectiveHeight;
  for (int i=0; i<topRowNumToCheck; ++i) {
    double costFunction = 0;
    std::vector< int > commonSegments;
    if (findCommonSegments(i, i+effectiveHeight, blockWidth, commonSegments)) {
      // calculate
    }
  }
  // findLocation();
  // setBlockLocation();
  // updateAvailSpace();
  currentX = 0;
  currentY = 0;
  blockWidth = 0;
  blockHeight = 0;
  Loc2D loc(0,0);
  return loc;
}

