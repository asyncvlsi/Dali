//
// Created by Yihang Yang on 10/31/19.
//

#include "pin.h"

#include "common/misc.h"

Pin::Pin(std::pair<const std::string, int> *name_num_pair_ptr, BlockType *blk_type) :
    name_num_pair_ptr_(name_num_pair_ptr),
    blk_type_(blk_type),
    is_input_(true) {
  manual_set_ = false;
  x_offset_.resize(8, 0);
  y_offset_.resize(8, 0);
}

Pin::Pin(std::pair<const std::string, int> *name_num_pair_ptr, BlockType *blk_type, double x_offset, double y_offset) :
    name_num_pair_ptr_(name_num_pair_ptr),
    blk_type_(blk_type),
    is_input_(true) {
  manual_set_ = true;
  x_offset_.resize(8, 0);
  y_offset_.resize(8, 0);

  CalculateOffset(x_offset, y_offset);
}

void Pin::InitOffset() {
  Assert(!rect_list_.empty(), "Empty rect_list cannot set x_offset_ and y_offset_!");
  if (!manual_set_) {
    CalculateOffset((rect_list_[0].LLX() + rect_list_[0].URX()) / 2.0,
                    (rect_list_[0].LLY() + rect_list_[0].URY()) / 2.0);
  }
}

void Pin::CalculateOffset(double x_offset, double y_offset) {
  x_offset_[0] = x_offset;
  y_offset_[0] = y_offset;

  /****
   * rotate 180 degree counterclockwise
   *    x' = -x; y' = -y;
   * shift back to the first quadrant
   *    x' += width; y' += height;
   * sum effect
   *    x' = width - x;
   *    y' = height - y;
   ****/
  x_offset_[S_ - N_] = blk_type_->Width() - x_offset;
  y_offset_[S_ - N_] = blk_type_->Height() - y_offset;

  /****
   * rotate 90 degree counterclockwise
   *    x' = -y; y' = x;
   * width, height switch
   *    width' = height;
   *    height' = width;
   * shift back to the first quadrant
   *    x' += width';
   * sum effect
   *    x' = height - y;
   *    y' = x;
   * ****/
  x_offset_[W_ - N_] = blk_type_->Height() - y_offset;
  y_offset_[W_ - N_] = x_offset;


  /****
   * rotate 270 degree counterclockwise
   *    x' = y; y' = -x;
   * width, height switch
   *    width' = height;
   *    height' = width;
   * shift back to the first quadrant
   *    y' += height';
   * sum effect
   *    x' = y;
   *    y' = width - x;
   * ****/
  x_offset_[E_ - N_] = y_offset;
  y_offset_[E_ - N_] = blk_type_->Width() - x_offset;

  /****
   * Flip along the line through the middle of width
   *    x' = width - x; y = y;
   * ****/
  x_offset_[FN_ - N_] = blk_type_->Width() - x_offset;
  y_offset_[FN_ - N_] = y_offset;

  /****
   * rotate 180 degree counterclockwise
   *    x' = width - x;
   *    y' = height - y;
   * then flip
   *    x' = width - x';
   *    y' = y';
   * sum effect
   *    x' = x;
   *    y' = height - y;
   * ****/
  x_offset_[FS_ - N_] = x_offset;
  y_offset_[FS_ - N_] = blk_type_->Height() - y_offset;

  /****
   * rotate 90 degree counterclockwise
   *    x' = height - y;
   *    y' = x;
   * then flip
   *    x' = width' - x';
   *    y' = y';
   * sum effect
   *    x' = y
   *    y' = x;
   * ****/
  x_offset_[FW_ - N_] = y_offset;
  y_offset_[FW_ - N_] = x_offset;

  /****
   * rotate 270 degree counterclockwise
   *    x' = y;
   *    y' = width - x;
   * then flip
   *    x' = width' - x';
   *    y' = y';
   * sum effect
   *    x' = height - y;
   *    y = width - x;
   * ****/
  x_offset_[FE_ - N_] = blk_type_->Height() - y_offset;
  y_offset_[FE_ - N_] = blk_type_->Width() - x_offset;

}
