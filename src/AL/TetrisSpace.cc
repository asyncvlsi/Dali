//
// Created by yihan on 7/12/2019.
//

#include "TetrisSpace.h"

TetrisSpace::TetrisSpace(int left, int right, int bottom, int top, int rowHeight, int minWidth):
                  _left(left), _right(right), _bottom(bottom), _top(top), _rowHeight(rowHeight), _minWidth(minWidth) {
  _rowNum = std::floor((_top-_bottom)/rowHeight);
  FreeSegment *tmpSpaceRangeptr = nullptr;
  for (int i=0; i<_rowNum; ++i) {
    availSpaceMap.push_back(tmpSpaceRangeptr);
  }
  for (auto &&spaceRangeptr: availSpaceMap) {
    spaceRangeptr = new FreeSegment(_left, _right);
  }
}

Loc2D TetrisSpace::placeBlock(double currentX, double currentY, int blockWidth, int blockHeight) {
  double minDisplacement = 1e30;
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

