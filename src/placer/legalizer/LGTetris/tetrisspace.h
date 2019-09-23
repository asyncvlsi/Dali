//
// Created by yihan on 7/12/2019.
//

#ifndef HPCC_TETRISSPACE_H
#define HPCC_TETRISSPACE_H

#include <vector>
#include <cmath>
#include "freesegmentlist.h"
#include "../../../common/misc.h"

struct Loc2D {
  int x;
  int y;
  explicit Loc2D(int initX, int initY): x(initX), y(initY) {};
};

class TetrisSpace {
private:
  int left_;
  int right_;
  int bottom_;
  int top_;
  int row_height_;
  int min_width_;
  std::vector< FreeSegmentList > free_segment_rows;
  /****derived data entry****/
  int tot_num_row_;
public:
  TetrisSpace(int left, int right, int bottom, int top, int rowHeight, int minWidth);
  void FindCommonSegments(int startRowNum, int endRowNum, FreeSegmentList &commonSegments);
  bool IsSpaceAvail(int x_loc, int y_loc, int width, int height);
  bool FindBlockLoc(double currentX, double currentY, int blockWidth, int blockHeight, Loc2D result_loc);
  void Show();
};


#endif //HPCC_TETRISSPACE_H
