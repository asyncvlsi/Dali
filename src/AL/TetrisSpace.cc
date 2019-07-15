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

Loc2D TetrisSpace::placeBlock(double currentX, double currentY, int blockWidth, int blockHeight) {
  double minDisplacement = 1e30;
  int topRowNumToCheck = freeSegmentRows.size() - (int)(std::ceil((double)blockHeight/_rowHeight) - 1);
  for (int i=0; i<topRowNumToCheck; ++i) {
    double costFunction = 0;

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

