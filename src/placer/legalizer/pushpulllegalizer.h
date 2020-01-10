//
// Created by Yihang Yang on 1/2/20.
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
  bool IsSpaceLegal(Block const &block);
  void UseSpace(Block const &block);
  bool PushBlock(Block &block);
  void PushLegalization();
  double GetPartialHPWL(Block &block, int x, int y);
  bool PullBlockLeft(Block &block);
  void PullLegalizationFromLeft();
  bool PullBlockRight(Block &block);
  void PullLegalizationFromRight();
  void StartPlacement() override;
};

#endif //DALI_SRC_PLACER_LEGALIZER_PUSHPULLLEGALIZER_H_
