//
// Created by Yihang on 10/21/2019.
//

#ifndef HPCC_SRC_PLACER_DETAILEDPLACER_DPSIMANNEAL_H_
#define HPCC_SRC_PLACER_DETAILEDPLACER_DPSIMANNEAL_H_

#include "../placer.h"

class DPSimAnneal: public Placer {
 public:
  DPSimAnneal();
  void SingleSegmentCluster();
  void GlobalSwap();
  void VerticalSwap();
  void LocalReOrder();
  void StartPlacement() override;
};

#endif //HPCC_SRC_PLACER_DETAILEDPLACER_DPSIMANNEAL_H_
