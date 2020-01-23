//
// Created by Yihang Yang on 1/2/20.
//

#ifndef DALI_SRC_PLACER_LEGALIZER_LGHILLEX_H_
#define DALI_SRC_PLACER_LEGALIZER_LGHILLEX_H_

#include "placer/placer.h"

class LGHillEx : public Placer {
 private:
  std::vector<int> row_start_;
  bool is_push_;
  std::vector<IndexLocPair<int>> index_loc_list_;

  bool legalize_from_left_;

  int cur_iter_;
  int max_iter_;

  double k_width_;
  double k_height_;
  double k_left_;
 public:
  LGHillEx();
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

  bool LocalLegalization();
  bool LocalLegalizationRight();

  void StartPlacement() override;

  void SetMaxIteration(int max_iter) {
    assert(max_iter >= 0);
    max_iter_ = max_iter;
  }
  void SetWidthHeightFactor(double k_width, double k_height) {
    k_width_ = k_width;
    k_height_ = k_height;
  }
  void SetLeftBoundFactor(double k_left) { k_left_ = k_left; }
};

#endif //DALI_SRC_PLACER_LEGALIZER_LGHILLEX_H_
