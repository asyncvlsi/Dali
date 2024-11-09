/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#ifndef DALI_TETRISSPACE_H
#define DALI_TETRISSPACE_H

#include <cmath>
#include <vector>

#include "dali/common/misc.h"
#include "freesegmentlist.h"

namespace dali {

class TetrisSpace {
 private:
  int scan_line_;
  int left_;
  int right_;
  int bottom_;
  int top_;
  int row_height_;
  int min_width_;
  std::vector<FreeSegmentList> free_segment_rows;
  /****derived data entry****/
  int tot_num_row_;

 public:
  TetrisSpace(int left, int right, int bottom, int top, int rowHeight,
              int minWidth);
  int ToStartRow(int y_loc);
  int ToEndRow(int y_loc);
  void UseSpace(int llx, int lly, int width, int height);
  void FindCommonSegments(int startRowNum, int endRowNum,
                          FreeSegmentList &commonSegments);
  bool IsSpaceAvail(int llx, int lly, int width, int height);
  bool FindBlockLoc(int llx, int lly, int width, int height, int2d &result_loc);
  void Show();
};

}  // namespace dali

#endif  // DALI_TETRISSPACE_H
