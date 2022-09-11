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

struct BlkCluster {
  BlkCluster();
  BlkCluster(
      int32_t well_extension_x_init,
      int32_t well_extension_y_init,
      int32_t plug_width_init
  );

  int32_t well_extension_x_;
  int32_t well_extension_y_;
  int32_t plug_width_;

  int32_t p_well_height_;

  int32_t width_;
  int32_t height_;
  int32_t lx_;
  int32_t ly_;
  std::vector<Block *> blk_ptr_list_;

  // cached value;
  int32_t modified_lx_;

  int32_t Width() const { return width_; }
  int32_t Height() const { return height_; }
  long Area() const { return width_ * height_; }
  int32_t InnerUX() const { return modified_lx_ + width_; }
  int32_t LLX() const { return lx_; }
  int32_t LLY() const { return ly_; }
  int32_t URX() const { return lx_ + width_; }
  int32_t URY() const { return ly_ + height_; }
  double CenterX() const { return lx_ + width_ / 2.0; }
  double CenterY() const { return ly_ + height_ / 2.0; }

  int32_t size() const { return blk_ptr_list_.size(); }

  void SetLLX(int32_t lx) { lx_ = lx; }
  void SetLLY(int32_t ly) { ly_ = ly; }
  void SetURX(int32_t ux) { lx_ = ux - width_; }
  void SetURY(int32_t uy) { ly_ = uy - height_; }
  void SetLoc(int32_t lx, int32_t ly) {
    lx_ = lx;
    ly_ = ly;
  }
  void SetCenterX(double center_x) {
    lx_ = (int32_t) std::round(
        center_x - width_ / 2.0);
  }
  void SetCenterY(double center_y) {
    ly_ = (int32_t) std::round(
        center_y - height_ / 2.0);
  }
  void IncreX(int32_t displacement) { lx_ += displacement; }
  void IncreY(int32_t displacement) { ly_ += displacement; }

  void AppendBlock(Block &block);
  void OptimizeHeight();
  void UpdateBlockLocation();
};

struct CluPtrLocPair {
  BlkCluster *clus_ptr;
  int32_t x;
  int32_t y;
  explicit CluPtrLocPair(
      BlkCluster *clus_ptr_init = nullptr,
      int32_t x_init = 0,
      int32_t y_init = 0
  ) : clus_ptr(clus_ptr_init),
      x(x_init),
      y(y_init) {}
  bool operator<(const CluPtrLocPair &rhs) const {
    return (x < rhs.x) || ((x == rhs.x) && (y < rhs.y));
  }
};

}

#endif //DALI_PLACER_WELLLEGALIZER_BLOCK_CLUSTER_H_
