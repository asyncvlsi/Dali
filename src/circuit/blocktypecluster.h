//
// Created by Yihang Yang on 2019/12/23.
//

#ifndef DALI_SRC_BLKTYPECLUSTER_H_
#define DALI_SRC_BLKTYPECLUSTER_H_

#include <vector>
#include "blocktypewell.h"

class BlockTypeWell;

class BlockTypeCluster {
 private:
  std::vector<BlockTypeWell*> cluster_list_;
 public:
  BlockTypeCluster();

};

#endif //DALI_SRC_BLKTYPECLUSTER_H_
