//
// Created by yihan on 8/4/2019.
//

#ifndef HPCC_SRC_PLACER_LEGALIZER_LGTETRIS_H_
#define HPCC_SRC_PLACER_LEGALIZER_LGTETRIS_H_

#include "placer/placer.h"
#include "LGTetris/tetrisspace.h"

class TetrisLegalizer: public Placer {
 private:

 public:
  TetrisLegalizer();
  bool TetrisLegal();
  void StartPlacement() override;
};

#endif //HPCC_SRC_PLACER_LEGALIZER_LGTETRIS_H_
