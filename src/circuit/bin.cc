//
// Created by Yihang Yang on 2019-05-20.
//

#include "bin.h"

Bin::Bin() {
  left_ = 0;
  bottom_ = 0;
  width_ = 0;
  height_ = 0;
}

Bin::Bin(int left, int bottom, int width, int height) : left_(left), bottom_(bottom), width_(width),
                                                        height_(height) {}

void Bin::SetLeft(int left) {
  left_ = left;
}

void Bin::SetBottom(int bottom) {
  bottom_ = bottom;
}

void Bin::SetWidth(int width) {
  width_ = width;
}

void Bin::SetHeight(int height) {
  height_ = height;
}

int Bin::Left() const {
  return left_;
}

int Bin::Bottom() const {
  return bottom_;
}

int Bin::Width() const {
  return width_;
}

int Bin::Height() const {
  return  height_;
}

int Bin::Right() const {
  return left_ + width_;
}

int Bin::Top() const {
  return bottom_ + height_;
}
