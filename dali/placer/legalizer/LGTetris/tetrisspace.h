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
  int32_t scan_line_;
  int32_t left_;
  int32_t right_;
  int32_t bottom_;
  int32_t top_;
  int32_t row_height_;
  int32_t min_width_;
  std::vector<FreeSegmentList> free_segment_rows;
  /****derived data entry****/
  int32_t tot_num_row_;
 public:
  TetrisSpace(int32_t left,
              int32_t right,
              int32_t bottom,
              int32_t top,
              int32_t rowHeight,
              int32_t minWidth);
  int32_t ToStartRow(int32_t y_loc);
  int32_t ToEndRow(int32_t y_loc);
  void UseSpace(int32_t llx, int32_t lly, int32_t width, int32_t height);
  void FindCommonSegments(int32_t startRowNum,
                          int32_t endRowNum,
                          FreeSegmentList &commonSegments);
  bool IsSpaceAvail(int32_t llx, int32_t lly, int32_t width, int32_t height);
  bool FindBlockLoc(int32_t llx, int32_t lly, int32_t width, int32_t height, int2d &result_loc);
  void Show();
};

}

#endif //DALI_TETRISSPACE_H
