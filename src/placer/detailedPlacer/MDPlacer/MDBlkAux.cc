//
// Created by yihan on 9/11/2019.
//

#include "MDBlkAux.h"
#include <algorithm>
#include <cmath>

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

Value2D MDBlkAux::GetForce(Block *blk) {
  /****
   * a rectangle can be described by its (llx, lly,) (urx, ury), and using these four values, one can calculate the area
   * ****/
  Value2D force(0,0);
  double epsilon = 1e-5;
  if (block_->IsOverlap(*blk)) {
    double force_amp;
    double llx, urx, lly, ury;
    llx = std::max(block_->LLX(), blk->LLX());
    urx = std::min(block_->URX(), blk->URX());
    lly = std::max(block_->LLY(), blk->LLY());
    ury = std::min(block_->URY(), blk->URY());
    force_amp = (urx - llx)*(ury - lly);
    Value2D direction(block_->X() - blk->X(), block_->Y() - blk->Y());
    // default direction is center to center direction
    if ((std::fabs(direction.x) < epsilon) && (std::fabs(direction.y) < epsilon)) {
      // when two blocks have very close center, chose a different direction to avoid numerical problems
      Value2D default_direction(1, 0);
      direction = default_direction;
    }
    direction.Normalize();
    force = direction.LinearScale(force_amp);
  }
  return force;
}
