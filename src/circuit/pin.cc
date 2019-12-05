//
// Created by Yihang Yang on 10/31/19.
//

#include "pin.h"
#include "../common/misc.h"

Pin::Pin(std::pair<const std::string, int>* name_num_pair_ptr, BlockType *blk_type):
        name_num_pair_ptr_(name_num_pair_ptr),
        blk_type_(blk_type)
        {
  manual_set_ = false;
  x_offset_.resize(8,0);
  y_offset_.resize(8,0);
}

Pin::Pin(std::pair<const std::string, int>* name_num_pair_ptr, BlockType *blk_type, int x_offset, int y_offset):
        name_num_pair_ptr_(name_num_pair_ptr),
        blk_type_(blk_type)
        {
  manual_set_ = true;
  x_offset_.resize(8,0);
  y_offset_.resize(8,0);

  CalculateOffset(x_offset, y_offset);
}

void Pin::InitOffset() {
  Assert(!rect_list_.empty(), "Empty rect_list cannot set x_offset_ and y_offset_!");
  if (!manual_set_) {
    CalculateOffset((rect_list_[0].LLX()+rect_list_[0].URX())/2.0, (rect_list_[0].LLY()+rect_list_[0].URY())/2.0);
  }
}

void Pin::CalculateOffset(double x_offset, double y_offset) {
  x_offset_[0] = x_offset;
  y_offset_[0] = y_offset;

  x_offset_[S-N] = x_offset;
  y_offset_[S-N] = y_offset;

  x_offset_[W-N] = x_offset;
  y_offset_[W-N] = y_offset;

  x_offset_[E-N] = x_offset;
  y_offset_[E-N] = y_offset;

  x_offset_[FN-N] = x_offset;
  y_offset_[FN-N] = y_offset;

  x_offset_[FS-N] = x_offset;
  y_offset_[FS-N] = blk_type_->Height() - y_offset;

  x_offset_[FW-N] = x_offset;
  y_offset_[FW-N] = y_offset;

  x_offset_[FE-N] = x_offset;
  y_offset_[FE-N] = y_offset;

}

void Pin::SetOffset(double x_offset, double y_offset) {
  CalculateOffset(x_offset, y_offset);
  manual_set_ = true;
}

double Pin::XOffset(BlockOrient orient) const {
  return x_offset_[orient-N];
}

double Pin::YOffset(BlockOrient orient) const {
  return y_offset_[orient-N];
}

void Pin::AddRect(Rect &rect) {
  rect_list_.push_back(rect);
}

void Pin::AddRect(double llx, double lly, double urx, double ury) {
  if (rect_list_.empty()) {
    CalculateOffset((llx + urx)/2.0, (lly + ury)/2.0);
  }
  rect_list_.emplace_back(llx, lly, urx, ury);
}

void Pin::Report() const {
  std::cout << *Name() << " (" << XOffset() << ", " << YOffset() << ")";
}
