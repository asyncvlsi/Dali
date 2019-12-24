//
// Created by Yihang Yang on 2019/12/23.
//

#include "common/misc.h"
#include "wellblktypeaux.h"

WellBlkTypeAux::WellBlkTypeAux(BlockType *block_type):
                              BlockTypeAux(block_type),
                              lx_(0),
                              ly_(0),
                              ux_(0),
                              uy_(0),
                              cluster_(nullptr) {}

void WellBlkTypeAux::SetCluster(BlkTypeCluster *cluster) {
  Assert(cluster != nullptr, "Cannot set WellBlkTypeAux pointing to an empty cluster!");
  cluster_ = cluster;
}