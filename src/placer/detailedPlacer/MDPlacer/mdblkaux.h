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
  BinIndex ll_index, ur_index;
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

inline void MDBlkAux::SetVx(double vx) {
  v_.x = vx;
}

inline void MDBlkAux::SetVy(double vy) {
  v_.y = vy;
}

inline double MDBlkAux::Vx() {
  return v_.x;
}

inline double MDBlkAux::Vy() {
  return v_.y;
}

inline void MDBlkAux::SetVelocity(Value2D velocity) {
  v_ = velocity;
}

inline Value2D MDBlkAux::Velocity() {
  return v_;
}

#endif //HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_MDBLKAUX_H_
