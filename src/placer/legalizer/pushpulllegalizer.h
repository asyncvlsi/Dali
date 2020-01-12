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

  bool is_pull_from_left_;
  int cur_iter_;
  int max_iter_;

 public:
  PushPullLegalizer();
  void InitLegalizer();
  bool IsSpaceLegal(Block &block);
  void UseSpace(Block const &block);
  void FastShift(unsigned int failure_point);

  bool PushBlock(Block &block);
  bool PushLegalizationFromLeft();
  bool PushLegalizationFromRight();
  double EstimatedHPWL(Block &block, int x, int y);

  bool PullBlockLeft(Block &block);
  void PullLegalizationFromLeft();
  bool PullBlockRight(Block &block);
  void PullLegalizationFromRight();
  void StartPlacement() override;

  bool ClosePackLegalization();
};

#endif //DALI_SRC_PLACER_LEGALIZER_PUSHPULLLEGALIZER_H_
