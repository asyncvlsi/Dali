//
// Created by Yihang Yang on 9/3/19.
//

#include <cmath>
#include "scaffoldnet.h"

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

ScaffoldNet::ScaffoldNet(double weight, Block *block_0, Block *block_1) {

}

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

double ScaffoldNet::HPWLX() const {
  return weight_ * std::fabs((block_0_->LLX()+x_offset_0_) - (block_1_->LLX()+x_offset_1_));
}

double ScaffoldNet::HPWLY() const {
  return weight_ * std::fabs((block_0_->LLY()+y_offset_0_) - (block_1_->LLY()+y_offset_1_));
}

double ScaffoldNet::HPWL() const {
  return HPWLX() + HPWLY();
}
