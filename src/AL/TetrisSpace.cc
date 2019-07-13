//
// Created by yihan on 7/12/2019.
//

#include "TetrisSpace.h"

TetrisSpace::TetrisSpace(int left, int right, int bottom, int top, int rowHeight, int minWidth):
                  _left(left), _right(right), _bottom(bottom), _top(top), _rowHeight(rowHeight), _minWidth(minWidth) {
  _rowNum = std::floor((_top-_bottom)/rowHeight);
  SpaceRange *tmpSpaceRangeptr = nullptr;
  for (int i=0; i<_rowNum; ++i) {
    availSpaceMap.push_back(tmpSpaceRangeptr);
  }
  for (auto &&spaceRangeptr: availSpaceMap) {
    spaceRangeptr = new SpaceRange(_left, _right);
  }
}

bool TetrisSpace::placeBlock(block_al_t &block) {
  // findLocation();
  // setBlockLocation();
  // updateAvailSpace();
  block.set_dllx(0);
  block.set_dlly(0);

  return true;
}

