//
// Created by yihan on 7/12/2019.
//

#ifndef HPCC_TETRISSPACE_H
#define HPCC_TETRISSPACE_H

#include <vector>
#include <cmath>
#include "freesegmentlist.h"
#include "../../../common/misc.h"

class TetrisSpace {
private:
  int scan_line_;
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
  int ToStartRow(int y_loc);
  int ToEndRow(int y_loc);
  void UseSpace(int llx, int lly, int width, int height);
  void FindCommonSegments(int startRowNum, int endRowNum, FreeSegmentList &commonSegments);
  bool IsSpaceAvail(int llx, int lly, int width, int height);
  bool FindBlockLoc(int llx, int lly, int width, int height, int2d &result_loc);
  void Show();
};


#endif //HPCC_TETRISSPACE_H
