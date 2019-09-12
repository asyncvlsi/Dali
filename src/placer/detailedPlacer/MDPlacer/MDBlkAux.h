//
// Created by yihan on 9/11/2019.
//

#ifndef HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_MDBLKAUX_H_
#define HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_MDBLKAUX_H_

#include "../../../circuit/blockaux.h"

class MDBlkAux: public BlockAux {
 private:
  double fx_;
  double fy_;
  double vx_;
  double vy_;
 public:
  explicit MDBlkAux(Block* blk_ptr);
  void SetFx(double fx);
  void SetFy(double fy);
  void SetVx(double vx);
  void SetVy(double vy);
  double Fx();
  double Fy();
  double Vx();
  double Vy();
};

#endif //HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_MDBLKAUX_H_
