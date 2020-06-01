//
// Created by Yihang Yang on 9/11/2019.
//

#include "mdblkaux.h"

#include <cmath>

#include <algorithm>

MDBlkAux::MDBlkAux(Block* blk_ptr): BlockAux(blk_ptr), v_(0,0) {}

double2d MDBlkAux::GetForce(Block *blk) {
  /****
   * A rectangle can be described by its (llx, lly,) (urx, ury), and using these four values, one can calculate the area;
   * The force between two blocks are in  proportional to the overlapping area, thus we need to find the overlap rectangle;
   * The direction of force can be chosen in many ways, we chose the force direction along the line of the center of two blocks;
   * If the center of two blocks are the same, the block with a lower number will move left, and the block with a higher number will move right;
   * Now we have the force amplitude and its direction, done.
   * ****/
  double2d force(0,0);
  double epsilon = 1e-5;
  if ((blk != getBlockPtr())&&(blk_ptr_->IsOverlap(*blk))) {
    double force_amp;
    double llx, urx, lly, ury;
    llx = std::max(blk_ptr_->LLX(), blk->LLX());
    urx = std::min(blk_ptr_->URX(), blk->URX());
    lly = std::max(blk_ptr_->LLY(), blk->LLY());
    ury = std::min(blk_ptr_->URY(), blk->URY());
    force_amp = (urx - llx)*(ury - lly);
    double2d direction(blk_ptr_->X() - blk->X(), blk_ptr_->Y() - blk->Y());
    // default direction is center to center direction
    if ((std::fabs(direction.x) < epsilon) && (std::fabs(direction.y) < epsilon)) {
      // when the centers of two blocks are very close, chose a default direction to avoid numerical issues
      double2d default_direction(0,0);
      if (blk_ptr_->Num() < blk->Num()) {
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
