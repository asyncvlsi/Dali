//
// Created by Yihang Yang on 10/31/19.
//

#include "pin.h"
#include "../common/misc.h"

Pin::Pin(std::pair<const std::string, int>* name_num_pair_ptr): name_num_pair_ptr_(name_num_pair_ptr) {
  manual_set_ = false;
  x_offset_.reserve(8);
  y_offset_.reserve(8);
  x_offset_.emplace_back(-1);
  y_offset_.emplace_back(-1);
}

Pin::Pin(std::pair<const std::string, int>* name_num_pair_ptr, int x_offset, int y_offset): name_num_pair_ptr_(name_num_pair_ptr) {
  manual_set_ = true;
  x_offset_.reserve(8);
  y_offset_.reserve(8);
  x_offset_.emplace_back(x_offset);
  y_offset_.emplace_back(y_offset);
}

void Pin::InitOffset() {
  Assert(!rect_list_.empty(), "Empty rect_list cannot set x_offset_ and y_offset_!");
  if (!manual_set_) {
    x_offset_[0] = (rect_list_[0].LLX() + rect_list_[0].URX()) / 2.0;
    y_offset_[0] = (rect_list_[0].LLY() + rect_list_[0].URY()) / 2.0;
  }
}

void Pin::SetOffset(double x_offset, double y_offset) {
  x_offset_[0] = x_offset;
  y_offset_[0] = y_offset;
  manual_set_ = true;
}

double Pin::XOffset(BlockOrient orient) const {
  if (orient!=N) {
    std::cout << "Currently, only N orientation is supported, exit program\n";
    exit(1);
  }
  return x_offset_[orient-N];
}

double Pin::YOffset(BlockOrient orient) const {
  if (orient!=N) {
    std::cout << "Currently, only N orientation is supported, exit program\n";
    exit(1);
  }
  return y_offset_[orient-N];
}

void Pin::AddRect(Rect &rect) {
  rect_list_.push_back(rect);
}

void Pin::AddRect(double llx, double lly, double urx, double ury) {
  if (rect_list_.empty()) {
    x_offset_[0] = (llx + urx)/2.0;
    y_offset_[0] = (lly + ury)/2.0;
  }
  rect_list_.emplace_back(llx, lly, urx, ury);
}

void Pin::Report() const {
  std::cout << *Name() << " (" << XOffset() << ", " << YOffset() << ")";
}
