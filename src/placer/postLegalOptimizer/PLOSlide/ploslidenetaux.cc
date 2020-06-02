//
// Created by Yihang Yang on 1/29/20.
//

#include "ploslidenetaux.h"

#include <cfloat>

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
  int sz = net_ptr_->blk_pin_list.size();
  Block *blk_ptr;
  Pin *pin_ptr;
  for (int i = 0; i < sz; ++i) {
    blk_ptr = net_ptr_->blk_pin_list[i].BlkPtr();
    pin_ptr = net_ptr_->blk_pin_list[i].PinPtr();
    blk2pin_map_.insert(std::make_pair(blk_ptr, pin_ptr));
  }
}

void PLOSlideNetAux::UpdateMaxMinLocX() {
  if (blk2pin_map_.empty()) return;
  max_x_ = DBL_MIN;
  min_x_ = DBL_MAX;
  double tmp_pin_loc = 0;

  Block *blk;
  Pin *pin;
  for (auto &pair: blk2pin_map_) {
    blk = pair.first;
    pin = pair.second;

    tmp_pin_loc = blk->LLX() + pin->OffsetX(blk->Orient());
    if (max_x_ < tmp_pin_loc) {
      max_x_ = tmp_pin_loc;
    }
    if (min_x_ > tmp_pin_loc) {
      min_x_ = tmp_pin_loc;
    }
  }
}

void PLOSlideNetAux::UpdateMaxMinLocY() {
  if (blk2pin_map_.empty()) return;
  max_y_ = DBL_MIN;
  min_y_ = DBL_MAX;
  double tmp_pin_loc = 0;

  Block *blk;
  Pin *pin;
  for (auto &pair: blk2pin_map_) {
    blk = pair.first;
    pin = pair.second;

    tmp_pin_loc = blk->LLY() + pin->OffsetY(blk->Orient());
    if (max_y_ < tmp_pin_loc) {
      max_y_ = tmp_pin_loc;
    }
    if (min_y_ > tmp_pin_loc) {
      min_y_ = tmp_pin_loc;
    }
  }
}

Pin *PLOSlideNetAux::GetPin(Block *block) {
  auto res = blk2pin_map_.find(block);
  Assert(res != blk2pin_map_.end(), "Cannot find the block in the net!");
  return res->second;
}
