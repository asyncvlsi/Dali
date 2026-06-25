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
#ifndef DALI_CIRCUIT_BLOCKPINPAIR_H_
#define DALI_CIRCUIT_BLOCKPINPAIR_H_

#include "block.h"
#include "dali/common/misc.h"

namespace dali {

/**
 * Connection between a block instance and one of its block-type pins.
 *
 * The class stores raw pointers for speed. Circuit construction must reserve
 * block and pin storage before creating net pins so vector growth does not
 * invalidate these pointers.
 */
class NetPin {
 public:
  NetPin(Block* block_ptr, Pin* pin_ptr)
      : blk_ptr_(block_ptr), pin_ptr_(pin_ptr) {}

  /** Return the connected block. */
  Block* BlkPtr() const { return blk_ptr_; }

  /** Return the connected block id. */
  int BlkId() const { return blk_ptr_->Id(); }

  /** Return the connected pin. */
  Pin* PinPtr() const { return pin_ptr_; }

  /** Return the connected pin id. */
  int PinId() const { return pin_ptr_->Id(); }

  /** Return orientation-aware x offset from the block origin. */
  double OffsetX() const { return pin_ptr_->OffsetX(blk_ptr_->Orient()); }

  /** Return orientation-aware y offset from the block origin. */
  double OffsetY() const { return pin_ptr_->OffsetY(blk_ptr_->Orient()); }

  /** Return absolute x location of this pin. */
  double AbsX() const { return OffsetX() + blk_ptr_->LLX(); }

  /** Return absolute y location of this pin. */
  double AbsY() const { return OffsetY() + blk_ptr_->LLY(); }

  /** Return absolute pin location. */
  double2d Location() const { return double2d(AbsX(), AbsY()); }

  /** Return the connected block name. */
  const std::string& BlockName() const { return blk_ptr_->Name(); }

  /** Return the connected pin name. */
  const std::string& PinName() const { return pin_ptr_->Name(); }

  bool operator<(const NetPin& rhs) const {
    return (BlkId() < rhs.BlkId()) ||
           ((BlkId() == rhs.BlkId()) && (PinId() < rhs.PinId()));
  }
  bool operator>(const NetPin& rhs) const {
    return (BlkId() > rhs.BlkId()) ||
           ((BlkId() == rhs.BlkId()) && (PinId() > rhs.PinId()));
  }
  bool operator==(const NetPin& rhs) const {
    return (BlkId() == rhs.BlkId()) && (PinId() == rhs.PinId());
  }

 private:
  Block* blk_ptr_;
  Pin* pin_ptr_;
};

}  // namespace dali

#endif  // DALI_CIRCUIT_BLOCKPINPAIR_H_
