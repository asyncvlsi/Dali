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
#ifndef DALI_DALI_PLACER_WELL_LEGALIZER_ROWSEGMENT_H_
#define DALI_DALI_PLACER_WELL_LEGALIZER_ROWSEGMENT_H_

#include <vector>

#include "dali/circuit/block.h"
#include "dali/common/misc.h"

namespace dali {

class RowSegment {
 public:
  RowSegment() = default;

  void SetLoc(int lx, int ly);
  void SetLLX(int lx);
  void SetURX(int ux);
  void SetWidth(int width);
  void SetUsedSize(int used_size);

  int LLX() const;
  int URX() const;
  int Width() const;
  int UsedSize() const;

  std::vector<Block *> &Blocks();
  void AddBlock(Block *blk_ptr);
  void MinDisplacementLegalization();
 private:
  // list of blocks in this segment
  std::vector<Block *> blk_list_;
  // initial location of blocks before putting into this segment
  std::vector<double2d> blk_initial_location_;
  int lx_ = INT_MIN;
  int ly_ = INT_MIN;
  int width_ = 0;
  int used_size_ = 0;
};

}

#endif //DALI_DALI_PLACER_WELL_LEGALIZER_ROWSEGMENT_H_
