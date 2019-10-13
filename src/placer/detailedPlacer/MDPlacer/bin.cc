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
