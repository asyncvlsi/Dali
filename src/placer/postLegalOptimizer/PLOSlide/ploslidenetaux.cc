//
// Created by Yihang Yang on 1/29/20.
//

#include "ploslidenetaux.h"

PLOSlideNetAux::PLOSlideNetAux(Net *net) :
    NetAux(net),
    max_x_(0),
    min_x_(0),
    max_y_(0),
    min_y_(0) {}

void PLOSlideNetAux::Init() {
  /****
   * Important:
   *    if a net contains several pins in the same block,
   *    only the first pin in this block is considered.
   * ****/
  int sz = net_->blk_pin_list.size();
  Block *blk_ptr;
  Pin *pin_ptr;
  for (int i = 0; i < sz; ++i) {
    blk_ptr = net_->blk_pin_list[i].GetBlock();
    pin_ptr = net_->blk_pin_list[i].GetPin();
    blk2pin_map_.insert(std::make_pair(blk_ptr, pin_ptr));
  }
}

void PLOSlideNetAux::UpdateMaxMinLocX() {
  net_->UpdateMaxMinIndexX();
  max_x_ = net_->MaxX();
  min_x_ = net_->MinX();
}

void PLOSlideNetAux::UpdateMaxMinLocY() {
  net_->UpdateMaxMinIndexY();
  max_y_ = net_->MaxY();
  min_y_ = net_->MinY();
}

Pin *PLOSlideNetAux::GetPin(Block *block) {
  auto res = blk2pin_map_.find(block);
  Assert(res != blk2pin_map_.end(), "Cannot find the block in the net!");
  return res->second;
}
