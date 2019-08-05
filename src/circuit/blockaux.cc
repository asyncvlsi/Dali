//
// Created by Yihang Yang on 2019-08-05.
//

#include "blockaux.h"

BlockAux::BlockAux(Block *block): block_(block) {
  block->SetAux(this);
}