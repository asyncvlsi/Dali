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
#include "blockcluster.h"

namespace dali {

BlkCluster::BlkCluster() = default;

BlkCluster::BlkCluster(int well_extension_x_init,
                       int well_extension_y_init,
                       int plug_width_init) :
    well_extension_x_(well_extension_x_init),
    well_extension_y_(well_extension_y_init),
    plug_width_(plug_width_init) {}

void BlkCluster::AppendBlock(Block &block) {
  if (blk_ptr_list_.empty()) {
    lx_ = int(block.LLX()) - well_extension_x_;
    modified_lx_ = lx_ - well_extension_x_ - plug_width_;
    ly_ = int(block.LLY()) - well_extension_y_;
    width_ = block.Width() + well_extension_x_ * 2 + plug_width_;
    height_ = block.Height() + well_extension_y_ * 2;
  } else {
    width_ += block.Width();
    if (block.Height() > height_) {
      ly_ -= (block.Height() - height_ + 1) / 2;
      height_ = block.Height() + well_extension_y_ * 2;
    }
  }
  blk_ptr_list_.push_back(&block);
}

void BlkCluster::OptimizeHeight() {
  /****
   * This function aligns all N/P well boundaries of cells inside a cluster
   * ****/

}

void BlkCluster::UpdateBlockLocation() {
  int current_loc = lx_;
  for (auto &blk_ptr: blk_ptr_list_) {
    blk_ptr->SetLLX(current_loc);
    blk_ptr->SetCenterY(this->CenterY());
    current_loc += blk_ptr->Width();
  }
}

}
