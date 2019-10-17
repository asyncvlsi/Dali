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
 private:
  BinIndex ll_index_, ur_index_;
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
  BinIndex LLIndex();
  BinIndex URIndex();
  void SetLLIndex(BinIndex ll);
  void SetURIndex(BinIndex ur);
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

inline BinIndex MDBlkAux::LLIndex() {
  return ll_index_;
}

inline BinIndex MDBlkAux::URIndex() {
  return ur_index_;
}

inline void MDBlkAux::SetLLIndex(BinIndex ll) {
  ll_index_ = ll;
}

inline void MDBlkAux::SetURIndex(BinIndex ur) {
  ur_index_ = ur;
}

#endif //HPCC_SRC_PLACER_DETAILEDPLACER_MDPLACER_MDBLKAUX_H_
