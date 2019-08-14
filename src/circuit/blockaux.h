//
// Created by Yihang Yang on 2019-08-05.
//

#ifndef HPCC_SRC_BLOCKAUX_H_
#define HPCC_SRC_BLOCKAUX_H_

#include "block.h"

class Block;

class BlockAux {
 private:
  Block *block_;
 public:
  BlockAux(Block *block);
  Block *GetBlock();
};

#endif //HPCC_SRC_BLOCKAUX_H_
