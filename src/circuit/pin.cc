//
// Created by Yihang Yang on 2019-05-23.
//

#include "pin.h"

Pin::Pin(std::string &name): name_(name), x_offset_(-1), y_offset_(-1) {}

const std::string *Pin::Name() const{
  return &name_;
}

void Pin::InitOffset() {
  Assert(!rect_list_.empty(), "Empty rect_list cannot set x_offset_ and y_offset_!");
  x_offset_ = (rect_list_[0].llx() + rect_list_[0].urx())/2.0;
  y_offset_ = (rect_list_[0].lly() + rect_list_[0].ury())/2.0;
}

double Pin::XOffset() const {
  return x_offset_;
}

double Pin::YOffset() const {
  return y_offset_;
}

void Pin::AddRect(Rect &rect) {

}

void Pin::AddRect(int llx, int lly, int urx, int ury) {

}
