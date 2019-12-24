//
// Created by Yihang Yang on 2019/12/23.
//

#ifndef HPCC_SRC_PLACER_WELLLEGALIZER_WELLBLKTYPEAUX_H_
#define HPCC_SRC_PLACER_WELLLEGALIZER_WELLBLKTYPEAUX_H_

#include "blocktypeaux.h"
#include "rect.h"
#include "blktypecluster.h"

class WellBlkTypeAux: public BlockTypeAux {
 private:
  bool is_normal_;
  Rect *n_well_, *p_well_;
  BlkTypeCluster *cluster_;
 public:
  explicit WellBlkTypeAux(BlockType *block_type);
  void SetCluster(BlkTypeCluster *cluster);
  BlkTypeCluster *GetCluster() const {return cluster_;}
};

#endif //HPCC_SRC_PLACER_WELLLEGALIZER_WELLBLKTYPEAUX_H_
