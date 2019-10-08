//
// Created by Yihang Yang on 9/11/2019.
//

#ifndef HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_
#define HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_

#include <vector>
#include "../placer.h"
#include "../../common/misc.h"
#include "MDPlacer/mdblkaux.h"
#include "MDPlacer/bin.h"

class MDPlacer: public Placer {
 private:
  double learning_rate_;
  double momentum_term_;
  int max_iteration_num_ ;
  int bin_width_;
  int bin_height_;
 public:
  MDPlacer();
  std::vector<MDBlkAux> blk_aux_list;
  std::vector<std::vector<Bin>> bin_matrix;
  void CreateBlkAuxList();
  void InitGridBin();
  void UpdateVelocityLoc(Block &blk);
  void StartPlacement() override;
};

#endif //HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_H_
