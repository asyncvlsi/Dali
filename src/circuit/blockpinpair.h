//
// Created by Yihang Yang on 10/31/19.
//

#ifndef DALI_SRC_CIRCUIT_BLOCKPINPAIR_H_
#define DALI_SRC_CIRCUIT_BLOCKPINPAIR_H_

#include "block.h"

class BlockPinPair {
 private:
  Block *pblk_;
  Pin *ppin_;
 public:
  BlockPinPair(Block *pblock, Pin *ppin) : pblk_(pblock), ppin_(ppin) {}

  Block *GetBlock() const { return pblk_; }
  int BlockNum() const { return pblk_->Num(); }

  Pin *GetPin() const { return ppin_; }
  int PinNum() const { return ppin_->Num(); }

  double XOffset() const { return ppin_->OffsetX(pblk_->Orient()); }
  double YOffset() const { return ppin_->OffsetY(pblk_->Orient()); }
  double AbsX() const { return XOffset() + pblk_->LLX(); }
  double AbsY() const { return YOffset() + pblk_->LLY(); }

  const std::string *BlockName() const { return GetBlock()->Name(); }
  const std::string *PinName() const { return GetPin()->Name(); }

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
