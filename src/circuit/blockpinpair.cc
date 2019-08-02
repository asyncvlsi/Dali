//
// Created by yihan on 7/27/2019.
//

#include "blockpinpair.h"

BlockPinPair::BlockPinPair(Block *block, int pin): block_(block), pin_(pin) {}

Block *BlockPinPair::GetBlock() const {
  return block_;
}

Pin *BlockPinPair::GetPin() const {
  return &(block_->Type()->pin_list[pin_]);
}

double BlockPinPair::XOffset() {
  if (block_->Orient() == N) {
    return block_->Type()->pin_list[pin_].XOffset();
  } else {
    std::cout << "Currently, only N orientation is supported\n";
    exit(1);
  }
}

double BlockPinPair::YOffset() {
  if (block_->Orient() == N) {
    return block_->Type()->pin_list[pin_].YOffset();
  } else {
    std::cout << "Currently, only N orientation is supported\n";
    exit(1);
  }
}

const std::string *BlockPinPair::BlockName() const {
  return GetBlock()->Name();
}

const std::string *BlockPinPair::PinName() const {
  return GetPin()->Name();
}
