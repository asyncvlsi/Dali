//
// Created by Yihang on 1/2/20.
//

#ifndef DALI_SRC_PLACER_LEGALIZER_PUSHPULLLEGALIZER_H_
#define DALI_SRC_PLACER_LEGALIZER_PUSHPULLLEGALIZER_H_

#include "placer/placer.h"

class PushPullLegalizer: public Placer {
 private:
  std::vector<int> row_start_;
  bool is_push_;
  std::vector<IndexLocPair<int>> index_loc_list_;
 public:
  PushPullLegalizer();
  void InitLegalizer();
  void PushBlock(Block &block);
  void PushLegalization();
  void PullBlock(Block &block);
  void PullLegalization();
  void StartPlacement() override;
};

#endif //DALI_SRC_PLACER_LEGALIZER_PUSHPULLLEGALIZER_H_
