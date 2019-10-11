//
// Created by Yihang Yang on 2019-05-20.
//

#include "bin.h"

Bin::Bin() {
  left_ = 0;
  bottom_ = 0;
  right_ = 0;
  top_ = 0;
}

Bin::Bin(int left, int bottom, int right, int top) : left_(left), bottom_(bottom), right_(right),
                                                     top_(top) {}

void Bin::SetLeft(int left) {
  left_ = left;
}

void Bin::SetBottom(int bottom) {
  bottom_ = bottom;
}

void Bin::SetRight(int right) {
  right_ = right;
}

void Bin::SetTop(int top) {
  top_ = top;
}

int Bin::Left() const {
  return left_;
}

int Bin::Bottom() const {
  return bottom_;
}

int Bin::Right() const {
  return right_;
}

int Bin::Top() const {
  return top_;
}

int Bin::Width() const {
  return right_ - left_;
}

int Bin::Height() const {
  return  top_ - bottom_;
}

