//
// Created by Yihang Yang on 9/11/2019.
//

#ifndef HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_
#define HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_

#include <vector>
#include "../placer.h"
#include "../../common/misc.h"
#include "placer/detailedPlacer/MDPlacer/mdblkaux.h"
#include "../../circuit/bin.h"

class MDPlacer: public Placer {
 private:
  double learning_rate_ = 0.1;
  double momentum_term_ = 0.9;
  int max_iteration_num_ = 100;
 public:
  std::vector<MDBlkAux> blk_aux_list;
  std::vector<std::vector<Bin>> bin_matrix;
  void CreateBlkAuxList();
  void InitGridBin();
  void UpdateVelocityLoc(Block &blk);
  void StartPlacement() override;
};

#endif //HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_
