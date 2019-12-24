//
// Created by Yihang Yang on 2019/12/23.
//

#include "blocktypeaux.h"

BlockTypeAux::BlockTypeAux(BlockType * block_type): block_type_(block_type) {
  block_type_->SetAux(this);
}