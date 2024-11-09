/*******************************************************************************
 *
 * Copyright (c) 2023 Yihang Yang
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
#ifndef DALI_CIRCUIT_ROW_H_
#define DALI_CIRCUIT_ROW_H_

#include <unordered_map>
#include <vector>

#include "block.h"

namespace dali {

/****
 * This class stands for a segment of continuous white space in a row.
 */
class GeneralRowSegment {
  friend class GeneralRow;

 public:
  GeneralRowSegment() = default;
  void SetLX(int lx);
  void SetWidth(int width);
  void AddBlock(Block *blk_ptr);

  int LX() const;
  int UX() const;
  int Width() const;
  std::vector<Block *> &Blocks();

  void SortBlocks();

 private:
  int lx_ = 0;                   // lower left x coordinate
  int width_ = -1;               // width of this row segment
  std::vector<Block *> blocks_;  // list of blocks in this row segment
};

/****
 * This is a general row for both standard cells and gridded cells.
 * A row consists of several segments sharing the same
 *   - y coordinate
 *   - height
 *   - orientation
 *   - p_well_height
 *   - n_well_height
 */
class GeneralRow {
 public:
  GeneralRow() = default;
  /**** setters ****/
  void SetLY(int ly);
  void SetHeight(int height);
  void SetOrient(bool is_orient_N);
  void SetPwellHeight(int p_well_height);
  void SetNwellHeight(int n_well_height);

  /**** getters ****/
  int LY() const;
  int Height() const;
  bool IsOrientN() const;
  int PwellHeight() const;
  int NwellHeight() const;
  std::vector<GeneralRowSegment> &RowSegments();

 private:
  int ly_ = 0;               // lower left y coordinate
  int height_ = -1;          // height of this row
  bool is_orient_N_ = true;  // orientation of this row
  int p_well_height_ = -1;   // height of P-well
  int n_well_height_ = -1;   // height of N-well
  std::vector<GeneralRowSegment> row_segments_;
};

}  // namespace dali

#endif  // DALI_CIRCUIT_ROW_H_
