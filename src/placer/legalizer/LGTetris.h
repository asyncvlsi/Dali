//
// Created by Yihang Yang on 8/4/2019.
//

#ifndef HPCC_SRC_PLACER_LEGALIZER_LGTETRIS_H_
#define HPCC_SRC_PLACER_LEGALIZER_LGTETRIS_H_

#include "../placer.h"
#include "LGTetris/tetrisspace.h"

struct indexLocPair{
  int num;
  double x;
  double y;
  explicit indexLocPair(int num_init=0, double x_init=0, double y_init=0): num(num_init), x(x_init), y(y_init) {}
};

class TetrisLegalizer: public Placer {
 private:
  int max_iteration_;
  int current_iteration_;
  bool flipped_;
 public:
  TetrisLegalizer();
  std::vector< indexLocPair > blockXOrder;
  void Init();
  void SetMaxItr(int max_iteration);
  void ResetItr();
  void FastShift();
  void FlipPlacement();
  bool TetrisLegal();
  void StartPlacement() override;
};

#endif //HPCC_SRC_PLACER_LEGALIZER_LGTETRIS_H_
