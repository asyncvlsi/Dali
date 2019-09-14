//
// Created by yihan on 9/11/2019.
//

#include "MDBlkAux.h"

MDBlkAux::MDBlkAux(Block* blk_ptr): BlockAux(blk_ptr), fx_(0), fy_(0), vx_(0), vy_(0) {}

void MDBlkAux::SetFx(double fx) {
  fx_ = fx;
}

void MDBlkAux::SetFy(double fy) {
  fy_ = fy;
}

void MDBlkAux::SetVx(double vx) {
  vx_ = vx;
}

void MDBlkAux::SetVy(double vy) {
  vy_ = vy;
}

double MDBlkAux::Fx() {
  return fx_;
}

double MDBlkAux::Fy() {
  return fy_;
}

double MDBlkAux::Vx() {
  return vx_;
}

double MDBlkAux::Vy() {
  return vy_;
}

Value2D MDBlkAux::GetForce(Block &blk) {
  Value2D force;
  throw NotImplementedException();
  return force;
}
