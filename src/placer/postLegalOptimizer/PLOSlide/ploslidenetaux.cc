//
// Created by Yihang Yang on 1/29/20.
//

#include "ploslidenetaux.h"

void PLOSlideNetAux::Init() {
  int sz = net_->blk_pin_list.size();
  Block *blk_ptr;
  for (int i = 0; i < sz; ++i) {
    blk_ptr = net_->blk_pin_list[i].GetBlock();
    blk2num_map_.insert(std::make_pair(blk_ptr, i));
  }
}

void PLOSlideNetAux::UpdateMaxMinX() {
  net_->UpdateMaxMinX();
  max_x_ = net_->MaxX();
  min_x_ = net_->MinX();
}

void PLOSlideNetAux::UpdateMaxMinY() {
  net_->UpdateMaxMinY();
  max_y_ = net_->MaxY();
  min_y_ = net_->MinY();
}

int PLOSlideNetAux::GetPinNum(Block *block) {
  auto res = blk2num_map_.find(block);
  if (res == blk2num_map_.end()) {
    return -1;
  } else {
    return res->second;
  }
}