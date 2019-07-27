//
// Created by yihan on 7/27/2019.
//

#ifndef HPCC_SRC_CIRCUIT_BLOCKPINPAIR_H_
#define HPCC_SRC_CIRCUIT_BLOCKPINPAIR_H_

#include "block.h"

class BlockPinPair {
 private:
  Block *block;
  int pin;
 public:
  BlockPinPair(Block *block_ptr, int pin_num);
  double XOffset();
  double YOffset();
};

#endif //HPCC_SRC_CIRCUIT_BLOCKPINPAIR_H_
