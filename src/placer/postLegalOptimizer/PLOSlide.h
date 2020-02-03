//
// Created by Yihang Yang on 1/29/20.
//

#ifndef DALI_SRC_PLACER_POSTLEGALOPTIMIZER_PLOSLIDE_H_
#define DALI_SRC_PLACER_POSTLEGALOPTIMIZER_PLOSLIDE_H_

#include "PLOSlide/ploslidenetaux.h"
#include "placer/placer.h"

class PLOSlide : public Placer {
 private:
  std::vector<int> row_start_;
  std::vector<IndexLocPair<int>> index_loc_list_;

  std::vector<PLOSlideNetAux> net_aux_list_;

 public:
  PLOSlide();

  void InitPostLegalOptimizer();

  void FindOptimalRegionX(Block &block, int &start, int &end);
  void MoveBlkTowardOptimalRegion(Block &block, int start, int end);
  void OptimizationFromLeft();

  void MoveBlkTowardOptimalRegionRight(Block &block, int start, int end);
  void OptimizationFromRight();

  void StartPlacement() override;

};

#endif //DALI_SRC_PLACER_POSTLEGALOPTIMIZER_PLOSLIDE_H_
