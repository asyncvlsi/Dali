//
// Created by yihan on 9/11/2019.
//

#ifndef HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_
#define HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_

#include <vector>
#include "../placer.h"
#include "MDPlacer/MDBlkAux.h"

class MDPlacer: public Placer {
 private:
  double learning_rate;
  double momentum_term = 0.9;
 public:
  std::vector<MDPlacer> blk_aux_list;
  void CreateBlkAuxList();
  void UpdateForce();
  void UpdateVelocity();
  void UpdateLoc();
  void StartPlacement();
};

#endif //HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_
