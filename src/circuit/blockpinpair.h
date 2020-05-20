//
// Created by Yihang Yang on 10/31/19.
//

#ifndef DALI_SRC_CIRCUIT_BLOCKPINPAIR_H_
#define DALI_SRC_CIRCUIT_BLOCKPINPAIR_H_

#include "block.h"

class BlockPinPair {
 private:
  Block *blk_ptr_;
  Pin *pin_ptr_;
 public:
  BlockPinPair(Block *block_ptr, Pin *pin_ptr) : blk_ptr_(block_ptr), pin_ptr_(pin_ptr) {}

  Block *GetBlock() const { return blk_ptr_; }
  int BlockNum() const { return blk_ptr_->Num(); }

  Pin *GetPin() const { return pin_ptr_; }
  int PinNum() const { return pin_ptr_->Num(); }

  double XOffset() const { return pin_ptr_->OffsetX(blk_ptr_->Orient()); }
  double YOffset() const { return pin_ptr_->OffsetY(blk_ptr_->Orient()); }
  double AbsX() const { return XOffset() + blk_ptr_->LLX(); }
  double AbsY() const { return YOffset() + blk_ptr_->LLY(); }

  const std::string *BlockName() const { return blk_ptr_->Name(); }
  const std::string *PinName() const { return pin_ptr_->Name(); }

  // boolean operators
  bool operator<(const BlockPinPair &rhs) const {
    return (BlockNum() < rhs.BlockNum()) || ((BlockNum() == rhs.BlockNum()) && (PinNum() < rhs.PinNum()));
  }
  bool operator>(const BlockPinPair &rhs) const {
    return (BlockNum() > rhs.BlockNum()) || ((BlockNum() == rhs.BlockNum()) && (PinNum() > rhs.PinNum()));
  }
  bool operator==(const BlockPinPair &rhs) const {
    return (BlockNum() == rhs.BlockNum()) && (PinNum() == rhs.PinNum());
  }
};

#endif //DALI_SRC_CIRCUIT_BLOCKPINPAIR_H_
