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

/** Continuous placement segment inside a row. */
class GeneralRowSegment {
  friend class GeneralRow;

 public:
  GeneralRowSegment() = default;

  /** Set lower-left x coordinate in grid units. */
  void SetLX(int lx);

  /** Set segment width in grid units. */
  void SetWidth(int width);

  /** Add a block assigned to this segment. */
  void AddBlock(Block* blk_ptr);

  /** Return lower-left x coordinate in grid units. */
  int LX() const;

  /** Return upper-right x coordinate in grid units. */
  int UX() const;

  /** Return segment width in grid units. */
  int Width() const;

  /** Return blocks assigned to this segment. */
  std::vector<Block*>& Blocks();

  /** Sort assigned blocks by x location. */
  void SortBlocks();

 private:
  int lx_ = 0;                  // lower left x coordinate
  int width_ = -1;              // width of this row segment
  std::vector<Block*> blocks_;  // list of blocks in this row segment
};

/** Placement row shared by standard-cell and gridded-cell flows. */
class GeneralRow {
 public:
  GeneralRow() = default;

  /** Set lower-left y coordinate in grid units. */
  void SetLY(int ly);

  /** Set row height in grid units. */
  void SetHeight(int height);

  /** Set row orientation, true for N orientation. */
  void SetOrient(bool is_orient_N);

  /** Set P-well height in grid units. */
  void SetPwellHeight(int p_well_height);

  /** Set N-well height in grid units. */
  void SetNwellHeight(int n_well_height);

  /** Return lower-left y coordinate in grid units. */
  int LY() const;

  /** Return row height in grid units. */
  int Height() const;

  /** Return true when row orientation is N. */
  bool IsOrientN() const;

  /** Return P-well height in grid units. */
  int PwellHeight() const;

  /** Return N-well height in grid units. */
  int NwellHeight() const;

  /** Return row segments. */
  std::vector<GeneralRowSegment>& RowSegments();

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
