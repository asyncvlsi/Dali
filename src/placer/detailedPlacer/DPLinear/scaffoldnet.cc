//
// Created by Yihang Yang on 9/3/19.
//

#include <cmath>
#include "scaffoldnet.h"
#include "../../../common/global.h"
#include "../../../common/misc.h"

ScaffoldNet::ScaffoldNet(double weight,
                         Block *block_0,
                         Block *block_1,
                         double x_offset_0,
                         double y_offset_0,
                         double x_offset_1,
                         double y_offset_1)
    : weight_(weight),
      block_0_(block_0),
      block_1_(block_1),
      x_offset_0_(x_offset_0),
      y_offset_0_(y_offset_0),
      x_offset_1_(x_offset_1),
      y_offset_1_(y_offset_1) {}

ScaffoldNet::ScaffoldNet(Block *block_0, Block *block_1, double weight) {
  /****
   * As two given points are diagonals of a rectangle. so, x1 < x2, y1 < y2. similarly x3 < x4, y3 < y4.
   * so, bottom-left and top-right points of intersection rectangle can be found by using formula.
   * x5 = max(x1, x3);
   * y5 = max(y1, y3);
   * x6 = min(x2, x4);
   * y6 = min(y2, y4);
   * In case of no intersection, x5 and y5 will always exceed x6 and y5 respectively.
   * The other two points of the rectangle can be found by using simple geometry.
   * source: https://www.geeksforgeeks.org/intersecting-rectangle-when-bottom-left-and-top-right-corners-of-two-rectangles-are-given/
   *
   * What we need here is a net pushing two blocks away.
   * This constructor assumes two blocks are overlapping with each other.
   * The pseudo-net is added in this way:
   *    1. Finding the intersection rectangle;
   *    2. Adding a pseudo-net connecting the lower left corner and the lower right corner;
   *    3. The lower left point is on the right block, the lower right corner is on the left block;
   *    4. The left right block is determined by the location of their left edges.
   * ****/

  bool is_overlap = block_0->IsOverlap(block_1);
  block_0_ = block_0;
  block_1_ = block_1;
  if (is_overlap) {
    weight_ = weight;
    /*double llx = std::max(block_0->LLX(), block_1->LLX());
    double lly = std::max(block_0->LLY(), block_1->LLY());
    double urx = std::max(block_0->URX(), block_1->URX());
    double ury = std::max(block_0->URY(), block_1->URY());
    double overlap_width = urx - llx;
    double overlap_height = ury - lly;
    if (overlap_height >= overlap_width) { // the following only works well if one block is not inside another.
      if (block_0->LLX() <= block_1->LLX()) {
        x_offset_0_ = urx - block_0->LLX(); // x_offset_0_ = block_0->Width();
        y_offset_0_ = 0;
        x_offset_1_ = 0;
        y_offset_1_ = lly - block_1->LLY();
      } else {
        x_offset_0_ = 0;
        y_offset_0_ = lly - block_0->LLY();
        x_offset_1_ = urx - block_1->LLX(); // x_offset_1_ = block_1->Width();
        y_offset_1_ = 0;
      }
    } else {
      if (block_0->LLY() <= block_1->LLY()) {
        x_offset_0_ = llx - block_0->LLX();
        y_offset_0_ = 0;
        x_offset_1_ = 0;
        y_offset_1_ = ury - block_1->LLY();
      } else {
        x_offset_0_ = 0;
        y_offset_0_ = ury - block_0->LLY();
        x_offset_1_ = llx - block_1->LLX();
        y_offset_1_ = 0;
      }
    }*/
    if (true) { // if one block is fully inside another block

    } else { // if one block is partially overlap with another block

    }

  } else {
    if (globalVerboseLevel >= LOG_WARNING) {
      Warning(true, "Constructor ScaffoldNet(double weight, Block *block_0, Block *block_1) will construct 0-weight net!");
    }
    weight_ = 0;
    x_offset_0_ = 0;
    y_offset_0_ = 0;
    x_offset_1_ = 0;
    y_offset_1_ = 0;
  }
}

ScaffoldNet::ScaffoldNet(Block &block_0, Block &block_1, double weight): ScaffoldNet(&block_0, &block_1, weight) {}

double ScaffoldNet::Weight() const {
  return weight_;
}

Block *ScaffoldNet::Block0() const {
  return block_0_;
}

Block *ScaffoldNet::Block1() const {
  return block_1_;
}

double ScaffoldNet::XOffset0() const {
  return x_offset_0_;
}

double ScaffoldNet::YOffset0() const {
  return  y_offset_0_;
}

double ScaffoldNet::XOffset1() const {
  return  x_offset_1_;
}

double ScaffoldNet::YOffset1() const {
  return y_offset_1_;
}

double ScaffoldNet::XAbsLoc0() const {
  return block_0_->LLX() + x_offset_0_;
}

double ScaffoldNet::YAbsLoc0() const {
  return block_0_->LLY() + y_offset_0_;
}

double ScaffoldNet::XAbsLoc1() const {
  return block_1_->LLX() + x_offset_1_;
}

double ScaffoldNet::YAbsLoc1() const {
  return block_1_->LLY() + y_offset_1_;
}

double ScaffoldNet::HPWLX() const {
  return weight_ * std::fabs((block_0_->LLX() + x_offset_0_) - (block_1_->LLX() + x_offset_1_));
}

double ScaffoldNet::HPWLY() const {
  return weight_ * std::fabs((block_0_->LLY() + y_offset_0_) - (block_1_->LLY() + y_offset_1_));
}

double ScaffoldNet::HPWL() const {
  return HPWLX() + HPWLY();
}
