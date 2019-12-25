//
// Created by Yihang Yang on 2019-08-05.
//

#ifndef DALI_SRC_BLOCKAUX_H_
#define DALI_SRC_BLOCKAUX_H_

#include "block.h"

class Block;

class BlockAux {
 protected:
  Block *block_;
 public:
  explicit BlockAux(Block *block);
  Block *GetBlock();
};

#endif //DALI_SRC_BLOCKAUX_H_
