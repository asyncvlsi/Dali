//
// Created by Yihang Yang on 2019/12/23.
//

#ifndef HPCC_SRC_PLACER_WELLLEGALIZER_BLKTYPECLUSTER_H_
#define HPCC_SRC_PLACER_WELLLEGALIZER_BLKTYPECLUSTER_H_

#include <vector>
#include "circuit/wellblktypeaux.h"

class BlkTypeCluster {
 private:
  std::vector<WellBlkTypeAux*> cluster_list_;
 public:
  BlkTypeCluster();

};

#endif //HPCC_SRC_PLACER_WELLLEGALIZER_BLKTYPECLUSTER_H_
