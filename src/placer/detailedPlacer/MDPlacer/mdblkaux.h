//
// Created by Yihang Yang on 9/11/2019.
//

#ifndef HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_MDBLKAUX_H_
#define HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_MDBLKAUX_H_

#include <vector>
#include <set>
#include "../../../circuit/net.h"
#include "../../../circuit/blockaux.h"
#include "../../../common/misc.h"
#include "bin.h"

class MDBlkAux: public BlockAux {
 public:
  Value2D v_;
  std::vector<Net*> net_list;
  explicit MDBlkAux(Block* blk_ptr);
  void SetVx(double vx);
  void SetVy(double vy);
  double Vx();
  double Vy();
  void SetVelocity(Value2D velocity);
  Value2D Velocity();
  Value2D GetForce(Block *blk);
  void ReportNet();
};

#endif //HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_MDBLKAUX_H_
