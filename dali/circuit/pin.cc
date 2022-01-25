/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#include "pin.h"

#include "blocktype.h"

#define NUM_OF_ORIENT 8

namespace dali {

Pin::Pin(
    std::pair<const std::string, int> *name_id_pair_ptr,
    BlockType *blk_type_ptr
) :
    name_id_pair_ptr_(name_id_pair_ptr),
    blk_type_ptr_(blk_type_ptr),
    is_input_(true) {
  manual_set_ = false;
  x_offset_.resize(NUM_OF_ORIENT, 0);
  y_offset_.resize(NUM_OF_ORIENT, 0);
}

Pin::Pin(
    std::pair<const std::string, int> *name_id_pair_ptr,
    BlockType *blk_type_ptr,
    double x_offset,
    double y_offset
) :
    name_id_pair_ptr_(name_id_pair_ptr),
    blk_type_ptr_(blk_type_ptr),
    is_input_(true) {
  manual_set_ = true;
  x_offset_.resize(NUM_OF_ORIENT, 0);
  y_offset_.resize(NUM_OF_ORIENT, 0);

  CalculateOffset(x_offset, y_offset);
}

const std::string &Pin::Name() const {
  return name_id_pair_ptr_->first;
}

int Pin::Id() const {
  return name_id_pair_ptr_->second;
}

void Pin::SetOffset(double x_offset, double y_offset) {
  CalculateOffset(x_offset, y_offset);
  manual_set_ = true;
}

void Pin::SetBoundingBoxSize(double width, double height) {
  DaliExpects(width >= 0 && height >= 0,
              "Negative width or height?");
  half_bbox_width_ = width / 2.0;
  half_bbox_height_ = height / 2.0;
}

double Pin::OffsetX(BlockOrient orient) const {
  return x_offset_[orient - N];
}

double Pin::OffsetY(BlockOrient orient) const {
  return y_offset_[orient - N];
}

void Pin::SetIoType(bool is_input) {
  is_input_ = is_input;
}

bool Pin::IsInput() const {
  return is_input_;
}

double Pin::HalfBboxWidth() {
  return half_bbox_width_;
}

double Pin::HalfBboxHeight() {
  return half_bbox_height_;
}

void Pin::Report() const {
  BOOST_LOG_TRIVIAL(info)
    << Name() << " (" << OffsetX() << ", " << OffsetY() << ")";
  for (int i = 0; i < 8; ++i) {
    BOOST_LOG_TRIVIAL(info)
      << "   (" << x_offset_[i] << ", " << y_offset_[i] << ")";
  }
  BOOST_LOG_TRIVIAL(info) << "\n";
}

void Pin::CalculateOffset(double x_offset, double y_offset) {
  /****
   * rotate 0 degree
   *    x' = x; y' = y;
   ****/
  x_offset_[N - N] = x_offset;
  y_offset_[N - N] = y_offset;

  /****
   * rotate 180 degree counterclockwise
   *    x' = -x; y' = -y;
   * shift back to the first quadrant
   *    x' += width; y' += height;
   * sum effect
   *    x' = width - x;
   *    y' = height - y;
   ****/
  x_offset_[S - N] = blk_type_ptr_->Width() - x_offset;
  y_offset_[S - N] = blk_type_ptr_->Height() - y_offset;

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
  x_offset_[W - N] = blk_type_ptr_->Height() - y_offset;
  y_offset_[W - N] = x_offset;


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
  x_offset_[E - N] = y_offset;
  y_offset_[E - N] = blk_type_ptr_->Width() - x_offset;

  /****
   * Flip along the line through the middle of width
   *    x' = width - x; y = y;
   * ****/
  x_offset_[FN - N] = blk_type_ptr_->Width() - x_offset;
  y_offset_[FN - N] = y_offset;

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
  x_offset_[FS - N] = x_offset;
  y_offset_[FS - N] = blk_type_ptr_->Height() - y_offset;

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
  x_offset_[FW - N] = y_offset;
  y_offset_[FW - N] = x_offset;

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
  x_offset_[FE - N] = blk_type_ptr_->Height() - y_offset;
  y_offset_[FE - N] = blk_type_ptr_->Width() - x_offset;

}

}

