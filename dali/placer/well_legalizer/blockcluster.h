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
#ifndef DALI_PLACER_WELLLEGALIZER_BLOCK_CLUSTER_H_
#define DALI_PLACER_WELLLEGALIZER_BLOCK_CLUSTER_H_

#include "dali/circuit/block.h"

namespace dali {

/** Cluster of blocks that share legalized well geometry. */
struct BlkCluster {
  BlkCluster();
  BlkCluster(int well_extension_x_init, int well_extension_y_init,
             int plug_width_init);

  int well_extension_x_;
  int well_extension_y_;
  int plug_width_;

  int p_well_height_;

  int width_;
  int height_;
  int lx_;
  int ly_;
  std::vector<Block*> blk_ptr_list_;

  // Cached legalized x location.
  int modified_lx_;

  /** Return cluster width in Dali grid units. */
  int Width() const { return width_; }

  /** Return cluster height in Dali grid units. */
  int Height() const { return height_; }

  /** Return cluster area in grid-unit squared. */
  long Area() const { return width_ * height_; }

  /** Return upper x based on the legalized x location. */
  int InnerUX() const { return modified_lx_ + width_; }

  /** Return lower-left x in Dali grid units. */
  int LLX() const { return lx_; }

  /** Return lower-left y in Dali grid units. */
  int LLY() const { return ly_; }

  /** Return upper-right x in Dali grid units. */
  int URX() const { return lx_ + width_; }

  /** Return upper-right y in Dali grid units. */
  int URY() const { return ly_ + height_; }
  double CenterX() const { return lx_ + width_ / 2.0; }
  double CenterY() const { return ly_ + height_ / 2.0; }

  int size() const { return blk_ptr_list_.size(); }

  void SetLLX(int lx) { lx_ = lx; }
  void SetLLY(int ly) { ly_ = ly; }
  void SetURX(int ux) { lx_ = ux - width_; }
  void SetURY(int uy) { ly_ = uy - height_; }
  void SetLoc(int lx, int ly) {
    lx_ = lx;
    ly_ = ly;
  }
  void SetCenterX(double center_x) {
    lx_ = (int)std::round(center_x - width_ / 2.0);
  }
  void SetCenterY(double center_y) {
    ly_ = (int)std::round(center_y - height_ / 2.0);
  }
  void IncreX(int displacement) { lx_ += displacement; }
  void IncreY(int displacement) { ly_ += displacement; }

  /** Append one block and update cluster dimensions. */
  void AppendBlock(Block& block);

  /** Optimize cluster height based on contained blocks. */
  void OptimizeHeight();

  /** Write the cluster location back to contained blocks. */
  void UpdateBlockLocation();
};

/** Pairing of a cluster pointer and candidate x/y location. */
struct CluPtrLocPair {
  BlkCluster* clus_ptr;
  int x;
  int y;
  explicit CluPtrLocPair(BlkCluster* clus_ptr_init = nullptr, int x_init = 0,
                         int y_init = 0)
      : clus_ptr(clus_ptr_init), x(x_init), y(y_init) {}
  bool operator<(const CluPtrLocPair& rhs) const {
    return (x < rhs.x) || ((x == rhs.x) && (y < rhs.y));
  }
};

}  // namespace dali

#endif  // DALI_PLACER_WELLLEGALIZER_BLOCK_CLUSTER_H_
