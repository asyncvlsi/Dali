//
// Created by Yihang Yang on 10/31/19.
//

#ifndef DALI_SRC_CIRCUIT_BLOCKPINPAIR_H_
#define DALI_SRC_CIRCUIT_BLOCKPINPAIR_H_

#include "block.h"

class BlockPinPair {
 private:
  Block *block_;
  Pin *pin_;
 public:
  BlockPinPair(Block *block, Pin *pin) : block_(block), pin_(pin) {}

  Block *GetBlock() const;
  int BlockNum() const;

  Pin *GetPin() const;
  int PinNum() const;

  double XOffset() const;
  double YOffset() const;
  double AbsX() const;
  double AbsY() const;

  const std::string *BlockName() const;
  const std::string *PinName() const;

  // some boolean operators
  bool operator<(const BlockPinPair &rhs) const;
  bool operator>(const BlockPinPair &rhs) const;
  bool operator==(const BlockPinPair &rhs) const;
};

inline Block *BlockPinPair::GetBlock() const {
  return block_;
}

inline int BlockPinPair::BlockNum() const {
  return block_->Num();
}

inline Pin *BlockPinPair::GetPin() const {
  return pin_;
}

inline int BlockPinPair::PinNum() const {
  return pin_->Num();
}

inline double BlockPinPair::XOffset() const {
  return pin_->XOffset(block_->Orient());
}

inline double BlockPinPair::YOffset() const {
  return pin_->YOffset(block_->Orient());
}

inline double BlockPinPair::AbsX() const {
  return XOffset() + block_->LLX();
}

inline double BlockPinPair::AbsY() const {
  return YOffset() + block_->LLY();
}

inline const std::string *BlockPinPair::BlockName() const {
  return GetBlock()->Name();
}

inline const std::string *BlockPinPair::PinName() const {
  return GetPin()->Name();
}

inline bool BlockPinPair::operator<(const BlockPinPair &rhs) const {
  return (BlockNum() < rhs.BlockNum()) || ((BlockNum() == rhs.BlockNum()) && (PinNum() < rhs.PinNum()));
}
inline bool BlockPinPair::operator>(const BlockPinPair &rhs) const {
  return (BlockNum() < rhs.BlockNum()) || ((BlockNum() == rhs.BlockNum()) && (PinNum() > rhs.PinNum()));
}
inline bool BlockPinPair::operator==(const BlockPinPair &rhs) const {
  return (BlockNum() == rhs.BlockNum()) && (PinNum() == rhs.PinNum());
}

#endif //DALI_SRC_CIRCUIT_BLOCKPINPAIR_H_
