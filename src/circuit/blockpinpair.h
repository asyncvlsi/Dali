//
// Created by yihan on 7/27/2019.
//

#ifndef HPCC_SRC_CIRCUIT_BLOCKPINPAIR_H_
#define HPCC_SRC_CIRCUIT_BLOCKPINPAIR_H_

#include "block.h"
#include "pin.h"

class BlockPinPair {
 private:
  Block *block_;
  int pin_;
 public:
  BlockPinPair(Block *block, int pin);
  Block *GetBlock() const;
  Pin *GetPin() const;
  double XOffset();
  double YOffset();
  friend std::ostream& operator<<(std::ostream& os, const BlockPinPair &block_pin_pair) {
    os << " (" << *(block_pin_pair.GetBlock()->Name()) << ", " << *(block_pin_pair.GetPin()->Name()) << ") ";
    return os;
  }
};

#endif //HPCC_SRC_CIRCUIT_BLOCKPINPAIR_H_
