//
// Created by Yihang Yang on 2019/12/23.
//

#ifndef HPCC_SRC_CIRCUIT_BLOCKTYPEAUX_H_
#define HPCC_SRC_CIRCUIT_BLOCKTYPEAUX_H_

#include "blocktype.h"

class BlockTypeAux {
 protected:
  BlockType * block_type_;
 public:
  explicit BlockTypeAux(BlockType * block_type);
  BlockType *GetBlkType() const {return block_type_;}
};

#endif //HPCC_SRC_CIRCUIT_BLOCKTYPEAUX_H_
