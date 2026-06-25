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
#ifndef DALI_CIRCUIT_BLOCK_H_
#define DALI_CIRCUIT_BLOCK_H_

#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <vector>

#include "block_type.h"
#include "dali/common/logging.h"
#include "dali/common/misc.h"
#include "enums.h"

namespace dali {

class BlockAux;

/**
 * Physical instance in a design.
 *
 * A block can represent a standard cell, macro, filler, well tap, or generated
 * helper instance. Its location is stored as the lower-left corner of the
 * placement bounding rectangle, matching DEF COMPONENT placement semantics.
 */
class Block {
 public:
  explicit Block(std::string const* name_ptr) : name_ptr_(name_ptr) {}

  /** Return the block instance name. */
  const std::string& Name() const { return *name_ptr_; }

  /** Return the master/type that defines this block's dimensions and pins. */
  BlockType* TypePtr() const { return type_ptr_; }

  /** Return the block's internal design id. */
  int Id() const { return id_; }

  /** Return the block width in Dali grid units. */
  int Width() const { return type_ptr_->Width(); }

  /** Set effective height and update the cached effective area. */
  void SetHeight(int height);

  /** Reset effective height and area from the block type. */
  void ResetHeight();

  /** Return the effective height in Dali grid units. */
  int Height() const { return eff_height_; }

  /** Return lower-left x coordinate in Dali grid units. */
  double LLX() const { return llx_; }

  /** Return lower-left y coordinate in Dali grid units. */
  double LLY() const { return lly_; }

  /** Return upper-right x coordinate in Dali grid units. */
  double URX() const { return llx_ + Width(); }

  /** Return upper-right y coordinate, including any stretch length. */
  double URY() const { return lly_ + Height() + tot_stretch_length; }

  /** Return center x coordinate in Dali grid units. */
  double X() const { return llx_ + Width() / 2.0; }

  /** Return center y coordinate in Dali grid units. */
  double Y() const { return lly_ + Height() / 2.0; }

  /** Return the ids of nets connected to this block. */
  std::vector<int>& NetList() { return nets_; }

  /** Return true if this block has a placed, fixed, or cover status. */
  bool IsPlaced() const {
    return place_status_ == PLACED || place_status_ == FIXED ||
           place_status_ == COVER;
  }

  /** Return the placement status. */
  PlaceStatus Status() const { return place_status_; }

  /** Return the placement status as a DEF-style string. */
  std::string StatusStr() const { return PlaceStatusStr(place_status_); }

  /** Return true for statuses Dali treats as movable: UNPLACED or PLACED. */
  bool IsMovable() const {
    return place_status_ == UNPLACED || place_status_ == PLACED;
  }

  /** Return true when this block is fixed or cover. */
  bool IsFixed() const { return !IsMovable(); }

  /** Return cached effective area in grid-unit squared. */
  long long Area() const { return eff_area_; }

  /** Return the current block orientation. */
  BlockOrient Orient() const { return orient_; }

  /** Return true when the orientation mirrors the block. */
  bool IsFlipped() const;

  /** Return optional auxiliary placement data attached by a later flow. */
  BlockAux* AuxPtr() const { return aux_ptr_; }

  /** Set the internal design id. */
  void SetId(size_t id) { id_ = id; }

  /** Set the block type and reset effective dimensions from it. */
  void SetType(BlockType* type_ptr);

  /** Set lower-left location in Dali grid units. */
  void SetLoc(double lx, double ly);

  // set the lower left x coordinate
  void SetLLX(double lx) { llx_ = lx; }

  // set the lower left y coordinate
  void SetLLY(double ly) { lly_ = ly; }

  // set the upper right x coordinate
  void SetURX(double ux) { llx_ = ux - Width(); }

  // set the upper right y coordinate
  void SetURY(double uy) { lly_ = uy - Height(); }

  // set the center x coordinate
  void SetCenterX(double center_x) { llx_ = center_x - Width() / 2.0; }

  // set the center y coordinate
  void SetCenterY(double center_y) { lly_ = center_y - Height() / 2.0; }

  // set the placement status of this Block
  void SetPlacementStatus(PlaceStatus place_status);

  // set the orientation of this Block
  void SetOrient(BlockOrient orient);

  // set the pointer to the auxiliary information
  void SetAux(BlockAux* aux);

  /** Swap only the lower-left location with another block. */
  void SwapLoc(Block& blk);

  // increase x coordinate by a certain amount
  void IncreaseX(double displacement) { llx_ += displacement; }

  // increase y coordinate by a certain amount
  void IncreaseY(double displacement) { lly_ += displacement; }

  // increase x coordinate by a certain amount, but the final location is
  // bounded by @param lower, and upper
  void IncreaseX(double displacement, double upper, double lower);

  // increase y coordinate by a certain amount, but the final location is
  // bounded by @param lower, and upper
  void IncreaseY(double displacement, double upper, double lower);

  // decrease x coordinate by a certain amount
  void DecreaseX(double displacement) { llx_ -= displacement; }

  // decrease y coordinate by a certain amount
  void DecreaseY(double displacement) { lly_ -= displacement; }

  /** Return true when this block overlaps another block. */
  bool IsOverlap(const Block& blk) const {
    return !(LLX() > blk.URX() || blk.LLX() > URX() || LLY() > blk.URY() ||
             blk.LLY() > URY());
  }

  bool IsOverlap(const RectI& rect) const {
    return !(LLX() > rect.URX() || rect.LLX() > URX() || LLY() > rect.URY() ||
             rect.LLY() > URY());
  }

  /** Return true when this block overlaps another block pointer. */
  bool IsOverlap(const Block* blk) const { return IsOverlap(*blk); }

  /** Return the overlap area with another block. */
  double OverlapArea(const Block& blk) const;

  // set stretch length
  void SetStretchLength(size_t index, int length);

  // returns the stretching lengths
  std::vector<int>& StretchLengths();

  int CumulativeStretchLength(size_t index);

  /** Log detailed block information for debugging. */
  void Report();

  /** Log the nets connected to this block. */
  void ReportNet();

  /** Write this block's well geometry as MATLAB patch rectangles. */
  void ExportWellToMatlabPatchRect(std::ofstream& ost);

 protected:
  BlockType* type_ptr_ = nullptr;  // type
  // name for finding its index in block_list
  std::string const* name_ptr_ = nullptr;
  int id_ = 0;
  double llx_ =
      0;  // lower x coordinate, data type double, for global placement
  double lly_ = 0;         // lower y coordinate
  std::vector<int> nets_;  // the list of nets connected to this cell
  PlaceStatus place_status_ =
      UNPLACED;             // placement status, i.e, PLACED, FIXED, UNPLACED
  BlockOrient orient_ = N;  // orientation, normally, N or FS
  BlockAux* aux_ptr_ = nullptr;  // points to auxiliary information if needed

  // cached height, also used to store effective height, the unit is grid value
  // in the y-direction
  int eff_height_ = 0;
  long long eff_area_ = 0;  // cached effective area

  std::vector<int>
      stretch_length_;  // TODO : move these two attributes to LegalizerBlockAux
  double tot_stretch_length = 0;
};

class BlockAux {
 public:
  explicit BlockAux(Block* blk_ptr) : blk_ptr_(blk_ptr) {
    blk_ptr->SetAux(this);
  }

  /** Return the block that owns this auxiliary data. */
  Block* GetBlockPtr() const { return blk_ptr_; }

  /** Return the block that owns this auxiliary data. */
  Block* getBlockPtr() const { return GetBlockPtr(); }

 protected:
  Block* blk_ptr_;
};

struct BlockInitialLocation {
  Block* blk_ptr;
  double x;
  double y;
  explicit BlockInitialLocation(Block* blk_ptr_init = nullptr,
                                double x_init = 0, double y_init = 0)
      : blk_ptr(blk_ptr_init), x(x_init), y(y_init) {}
};

}  // namespace dali

#endif  // DALI_CIRCUIT_BLOCK_H_
