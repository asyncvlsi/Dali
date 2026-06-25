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

#ifndef DALI_CIRCUIT_PLACEMENT_BLOCKAGE_H_
#define DALI_CIRCUIT_PLACEMENT_BLOCKAGE_H_

#include "dali/common/misc.h"

namespace dali {

/** Placement region unavailable for movable cells. */
class PlacementBlockage {
 public:
  explicit PlacementBlockage(RectI const& rect) : rect_(rect) {}
  PlacementBlockage(int lx, int ly, int ux, int uy) : rect_(lx, ly, ux, uy) {}

  /** Return the blockage rectangle in Dali grid units. */
  const RectI& GetRect() const { return rect_; }

  /** Replace the blockage rectangle. */
  void SetRect(RectI const& rect) { rect_ = rect; }

 private:
  RectI rect_;
};

}  // namespace dali

#endif  // DALI_CIRCUIT_PLACEMENT_BLOCKAGE_H_
