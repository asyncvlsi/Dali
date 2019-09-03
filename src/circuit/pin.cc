//
// Created by Yihang Yang on 2019-05-23.
//

#include "pin.h"
#include "common/misc.h"

Pin::Pin(std::pair<const std::string, int>* name_num_pair_ptr): name_num_pair_ptr_(name_num_pair_ptr), x_offset_(-1), y_offset_(-1) {
  manual_set_ = false;
}

Pin::Pin(std::pair<const std::string, int>* name_num_pair_ptr, int x_offset, int y_offset): name_num_pair_ptr_(name_num_pair_ptr), x_offset_(x_offset), y_offset_(y_offset) {
  manual_set_ = true;
}

const std::string *Pin::Name() const{
  return &(name_num_pair_ptr_->first);
}

int Pin::Num() const {
  return name_num_pair_ptr_->second;
}

void Pin::InitOffset() {
  Assert(!rect_list_.empty(), "Empty rect_list cannot set x_offset_ and y_offset_!");
  if (!manual_set_) {
    x_offset_ = (rect_list_[0].llx() + rect_list_[0].urx()) / 2.0;
    y_offset_ = (rect_list_[0].lly() + rect_list_[0].ury()) / 2.0;
  }
}

void Pin::SetOffset(double x_offset, double y_offset) {
  x_offset_ = x_offset;
  y_offset_ = y_offset;
  manual_set_ = true;
}

double Pin::XOffset() const {
  return x_offset_;
}

double Pin::YOffset() const {
  return y_offset_;
}

void Pin::AddRect(Rect &rect) {
  rect_list_.push_back(rect);
}

void Pin::AddRect(double llx, double lly, double urx, double ury) {
  if (rect_list_.empty()) {
    x_offset_ = (llx + urx)/2.0;
    y_offset_ = (lly + ury)/2.0;
  }
  rect_list_.emplace_back(llx, lly, urx, ury);
}

bool Pin::Empty() {
  return rect_list_.empty();
}
