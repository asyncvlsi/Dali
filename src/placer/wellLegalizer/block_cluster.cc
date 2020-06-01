//
// Created by Yihang Yang on 3/14/20.
//

#include "block_cluster.h"

BlkCluster::BlkCluster() = default;

BlkCluster::BlkCluster(int well_extension_x_init, int well_extension_y_init, int plug_width_init) :
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
    blk_ptr->setLLX(current_loc);
    blk_ptr->setCenterY(this->CenterY());
    current_loc += blk_ptr->Width();
  }
}
