//
// Created by yihan on 9/11/2019.
//

#ifndef HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_
#define HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_

#include <vector>
#include "../placer.h"
#include "MDPlacer/MDBlkAux.h"
#include "../../common/misc.h"

class MDPlacer: public Placer {
 private:
  double learning_rate = 0.1;
  double momentum_term = 0.9;
 public:
  std::vector<MDPlacer> blk_aux_list;
  void CreateBlkAuxList();
  Value2D UpdateForce(Block &blk);
  Value2D UpdateVelocity(Value2D &force);
  Value2D UpdateLoc(Value2D &velocity);
  void StartPlacement();
};

#endif //HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_
