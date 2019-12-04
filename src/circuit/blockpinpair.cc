//
// Created by Yihang Yang on 10/31/19.
//

#include "blockpinpair.h"

BlockPinPair::BlockPinPair(Block *block, int pin): block_(block), pin_num_(pin) {}

Block *BlockPinPair::GetBlock() const {
  return block_;
}

int BlockPinPair::BlockNum() const {
  return block_->Num();
}

Pin *BlockPinPair::GetPin() const {
  return &(block_->Type()->pin_list[pin_num_]);
}

int BlockPinPair::PinNum() const {
  return pin_num_;
}

double BlockPinPair::XOffset() {
  if (block_->Orient() == N) {
    return block_->Type()->pin_list[pin_num_].XOffset();
  } else {
    std::cout << "Currently, only N orientation is supported\n";
    exit(1);
  }
}

double BlockPinPair::YOffset() {
  if (block_->Orient() == N) {
    return block_->Type()->pin_list[pin_num_].YOffset();
  } else {
    std::cout << "Currently, only N orientation is supported\n";
    exit(1);
  }
}

double BlockPinPair::AbsX() const {
  return block_->Type()->pin_list[pin_num_].XOffset(block_->Orient()) + block_->LLX();
}

double BlockPinPair::AbsY() const {
  return block_->Type()->pin_list[pin_num_].YOffset(block_->Orient()) + block_->LLY();
}

const std::string *BlockPinPair::BlockName() const {
  return GetBlock()->Name();
}

const std::string *BlockPinPair::PinName() const {
  return GetPin()->Name();
}

bool BlockPinPair::operator <(const BlockPinPair &rhs) const {
  return (BlockNum() < rhs.BlockNum()) || ((BlockNum() == rhs.BlockNum()) && (PinNum() < rhs.PinNum()));
}

bool BlockPinPair::operator >(const BlockPinPair &rhs) const {
  return (BlockNum() < rhs.BlockNum()) || ((BlockNum() == rhs.BlockNum()) && (PinNum() > rhs.PinNum()));
}

bool BlockPinPair::operator ==(const BlockPinPair &rhs) const {
  return (BlockNum() == rhs.BlockNum()) && (PinNum() == rhs.PinNum());
}