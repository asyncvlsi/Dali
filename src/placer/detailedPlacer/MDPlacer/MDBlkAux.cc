//
// Created by Yihang Yang on 9/11/2019.
//

#include "MDBlkAux.h"
#include <algorithm>
#include <cmath>

MDBlkAux::MDBlkAux(Block* blk_ptr): BlockAux(blk_ptr), v_(0,0) {}

void MDBlkAux::SetVx(double vx) {
  v_.x = vx;
}

void MDBlkAux::SetVy(double vy) {
  v_.y = vy;
}

double MDBlkAux::Vx() {
  return v_.x;
}

double MDBlkAux::Vy() {
  return v_.y;
}

void MDBlkAux::SetVelocity(Value2D velocity) {
  v_ = velocity;
}

Value2D MDBlkAux::Velocity() {
  return v_;
}

Value2D MDBlkAux::GetForce(Block *blk) {
  /****
   * A rectangle can be described by its (llx, lly,) (urx, ury), and using these four values, one can calculate the area;
   * The force between two blocks are in  proportional to the overlapping area, thus we need to find the overlap rectangle;
   * The direction of force can be chosen in many ways, we chose the force direction along the line of the center of two blocks;
   * If the center of two blocks are the same, the block with a lower number will move left, and the block with a higher number will move right;
   * Now we have the force amplitude and its direction, done.
   * ****/
  Value2D force(0,0);
  double epsilon = 1e-5;
  if ((blk != GetBlock())&&(block_->IsOverlap(*blk))) {
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
      // when the centers of two blocks are very close, chose a default direction to avoid numerical issues
      Value2D default_direction(0,0);
      if (block_->Num() < blk->Num()) {
        default_direction.x = -1;
      } else {
        default_direction.x = 1;
      }
      direction = default_direction;
    }
    direction.Normalize();
    force = direction*force_amp;
  }
  return force;
}
