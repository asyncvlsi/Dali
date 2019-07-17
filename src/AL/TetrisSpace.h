//
// Created by yihan on 7/12/2019.
//

#ifndef HPCC_TETRISSPACE_H
#define HPCC_TETRISSPACE_H

#include <vector>
#include <cmath>
#include "FreeSegment.h"

struct Loc2D {
  int x;
  int y;
  explicit Loc2D(int initX, int initY): x(initX), y(initY) {};
};

class TetrisSpace {
private:
  int _left;
  int _right;
  int _bottom;
  int _top;
  int _rowHeight;
  int _minWidth;
  std::vector< FreeSegment * > freeSegmentRows;
  /****derived data entry****/
  int _totNumRow;
public:
  TetrisSpace(int left, int right, int bottom, int top, int rowHeight, int minWidth);
  bool trimCommonSegment(int rowNum, FreeSegment *commonSegments);
  bool findCommonSegments(int startRowNum, int endRowNum, int blockWidth, FreeSegment *commonSegments);
  Loc2D findBlockLocation(double currentX, double currentY, int blockWidth, int blockHeight);
};


#endif //HPCC_TETRISSPACE_H
