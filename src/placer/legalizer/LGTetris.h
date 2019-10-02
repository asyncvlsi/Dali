//
// Created by Yihang Yang on 8/4/2019.
//

#ifndef HPCC_SRC_PLACER_LEGALIZER_LGTETRIS_H_
#define HPCC_SRC_PLACER_LEGALIZER_LGTETRIS_H_

#include "../placer.h"
#include "LGTetris/tetrisspace.h"

class TetrisLegalizer: public Placer {
 private:
  int max_interation_;
  int current_interation_;
  bool flipped_;
 public:
  TetrisLegalizer();
  void FlipPlacement();
  bool TetrisLegal();
  void StartPlacement() override;
};

#endif //HPCC_SRC_PLACER_LEGALIZER_LGTETRIS_H_
