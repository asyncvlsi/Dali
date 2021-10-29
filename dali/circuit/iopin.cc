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
#include "iopin.h"

namespace dali {

IoPin::IoPin(std::pair<const std::string, int> *name_id_pair_ptr) :
    name_id_pair_ptr_(name_id_pair_ptr),
    net_ptr_(nullptr),
    signal_direction_(INPUT),
    signal_use_(SIGNAL),
    layer_ptr_(nullptr),
    init_place_status_(UNPLACED),
    place_status_(UNPLACED),
    x_(0),
    y_(0),
    orient_(N) {
  rects_.resize(NUM_OF_ORIENT, RectD(0, 0, 0, 0));
  rects_[0].SetValue(0, 0, 0, 0);
}

IoPin::IoPin(
    std::pair<const std::string, int> *name_id_pair_ptr,
    double loc_x,
    double loc_y
) : name_id_pair_ptr_(name_id_pair_ptr),
    net_ptr_(nullptr),
    signal_direction_(INPUT),
    signal_use_(SIGNAL),
    layer_ptr_(nullptr),
    init_place_status_(PLACED),
    place_status_(PLACED),
    x_(loc_x),
    y_(loc_y),
    orient_(N) {
  rects_.resize(NUM_OF_ORIENT, RectD(0, 0, 0, 0));
  rects_[0].SetValue(0, 0, 0, 0);
}

IoPin::IoPin(
    std::pair<const std::string, int> *name_id_pair_ptr,
    SignalDirection direction,
    PlaceStatus init_place_status,
    double loc_x,
    double loc_y
) : name_id_pair_ptr_(name_id_pair_ptr),
    net_ptr_(nullptr),
    signal_direction_(direction),
    signal_use_(SIGNAL),
    layer_ptr_(nullptr),
    init_place_status_(init_place_status),
    place_status_(init_place_status),
    x_(loc_x),
    y_(loc_y),
    orient_(N) {
  rects_.resize(NUM_OF_ORIENT, RectD(0, 0, 0, 0));
  rects_[0].SetValue(0, 0, 0, 0);
}

IoPin::IoPin(
    double loc_x,
    double loc_y,
    BlockOrient orient,
    double llx,
    double lly,
    double urx,
    double ury
) : name_id_pair_ptr_(nullptr),
    net_ptr_(nullptr),
    signal_direction_(INPUT),
    signal_use_(SIGNAL),
    layer_ptr_(nullptr),
    init_place_status_(PLACED),
    place_status_(PLACED),
    x_(loc_x),
    y_(loc_y),
    orient_(orient) {
  rects_.resize(NUM_OF_ORIENT, RectD(0, 0, 0, 0));
  SetRect(llx, lly, urx, ury);
}

const std::string &IoPin::Name() const {
  return name_id_pair_ptr_->first;
}

int IoPin::Id() const {
  return name_id_pair_ptr_->second;
}

void IoPin::SetNetPtr(Net *net_ptr) {
  DaliExpects(net_ptr != nullptr,
              "Why do you need to set net_ptr in an I/O pin to a nullptr?");
  net_ptr_ = net_ptr;
}

Net *IoPin::NetPtr() const {
  return net_ptr_;
}

const std::string &IoPin::NetName() const {
  DaliExpects(net_ptr_ != nullptr,
              "Cannot get net name because net_ptr_ is a nullptr");
  return net_ptr_->Name();
}

void IoPin::SetSigDirection(SignalDirection direction) {
  signal_direction_ = direction;
}

SignalDirection IoPin::SigDirection() const {
  return signal_direction_;
}

std::string IoPin::SigDirectStr() const {
  return SignalDirectionStr(signal_direction_);
}

void IoPin::SetSigUse(SignalUse use) {
  signal_use_ = use;
}

SignalUse IoPin::SigUse() const {
  return signal_use_;
}

std::string IoPin::SigUseStr() const {
  return SignalUseStr(signal_use_);
}

void IoPin::SetLayerPtr(MetalLayer *layer_ptr) {
  DaliExpects(layer_ptr != nullptr,
              "Why do you need to set layer_ptr in an I/O pin to a nullptr?");
  layer_ptr_ = layer_ptr;
}

MetalLayer *IoPin::LayerPtr() const {
  return layer_ptr_;
}

const std::string &IoPin::LayerName() const {
  DaliExpects(layer_ptr_ != nullptr,
              "Cannot get layer name because layer_ptr_ is a nullptr");
  return layer_ptr_->Name();
}

void IoPin::SetShape(double llx, double lly, double urx, double ury) {
  is_shape_set_ = true;
  SetRect(llx, lly, urx, ury);
}

RectD &IoPin::GetShape() {
  return rects_[0];
}

bool IoPin::IsShapeSet() const {
  return is_shape_set_;
}

void IoPin::SetInitPlaceStatus(PlaceStatus init_place_status) {
  init_place_status_ = init_place_status;
}

bool IoPin::IsPrePlaced() const {
  return init_place_status_ == FIXED || init_place_status_ == PLACED;
}

void IoPin::SetPlaceStatus(PlaceStatus place_status) {
  place_status_ = place_status;
}

bool IoPin::IsPlaced() const {
  return place_status_ == FIXED || place_status_ == PLACED;
}

PlaceStatus IoPin::GetPlaceStatus() const {
  return place_status_;
}

void IoPin::SetLocX(double loc_x) {
  x_ = loc_x;
}

double IoPin::X() const {
  return x_;
}

void IoPin::SetLocY(double loc_y) {
  y_ = loc_y;
}

double IoPin::Y() const {
  return y_;
}

void IoPin::SetLoc(
    double loc_x,
    double loc_y,
    PlaceStatus place_status
) {
  x_ = loc_x;
  y_ = loc_y;
  place_status_ = place_status;
}

double IoPin::LX(double spacing) const {
  return x_ + rects_[orient_ - N].LLX() - spacing;
}

double IoPin::UX(double spacing) const {
  return x_ + rects_[orient_ - N].URX() + spacing;
}

double IoPin::LY(double spacing) const {
  return y_ + rects_[orient_ - N].LLY() - spacing;
}

double IoPin::UY(double spacing) const {
  return y_ + rects_[orient_ - N].URY() + spacing;
}

void IoPin::SetFinalX(int final_x) {
  final_x_ = final_x;
}

int IoPin::FinalX() const {
  return final_x_;
}

void IoPin::SetFinalY(int final_y) {
  final_y_ = final_y;
}

int IoPin::FinalY() const {
  return final_y_;
}

void IoPin::SetOrient(BlockOrient orient) {
  orient_ = orient;
}

BlockOrient IoPin::GetOrient() const {
  return orient_;
}

void IoPin::Report() const {
  std::string net_name = (net_ptr_ == nullptr) ? "NA" : net_ptr_->Name();
  BOOST_LOG_TRIVIAL(info) << "  I/O PIN name: " << Name() << "\n"
                          << "    Net connected: " << net_name << "\n";
}

void IoPin::SetRect(double llx, double lly, double urx, double ury) {
  /****
   * rotate 0 degree
   *    llx' = llx;
   *    lly' = lly;
   *    urx' = urx;
   *    ury' = ury;
   ****/
  rects_[N - N].SetValue(llx, lly, urx, ury);

  /****
   * rotate 180 degree counterclockwise
   *    llx' = -urx;
   *    lly' = -ury;
   *    urx' = -llx;
   *    ury' = -lly;
  ****/
  rects_[S - N].SetValue(-urx, -ury, -llx, -lly);

  /****
   * rotate 90 degree counterclockwise
   *    llx' = -ury;
   *    lly' =  llx;
   *    urx' = -lly;
   *    ury' =  urx;
   * ****/
  rects_[W - N].SetValue(-ury, llx, -lly, urx);

  /****
   * rotate 270 degree counterclockwise
   *    llx' =  lly;
   *    lly' = -urx;
   *    urx' =  ury;
   *    ury' = -llx;
   * ****/
  rects_[E - N].SetValue(lly, -urx, ury, -llx);

  /****
   * rotate 0 degree counterclockwise
   *    llx' = llx;
   *    lly' = lly;
   *    urx' = urx;
   *    ury' = ury;
   * flip along the y-direction through the location of this pin
   *    llx' = -urx;
   *    lly' =  lly;
   *    urx' = -llx;
   *    ury' =  ury;
   * ****/
  rects_[FN - N].SetValue(-urx, lly, -llx, ury);

  /****
   * rotate 180 degree counterclockwise
   *    llx' = -urx;
   *    lly' = -ury;
   *    urx' = -llx;
   *    ury' = -lly;
   * flip along the y-direction through the location of this pin
   *    llx' =  llx;
   *    lly' = -ury;
   *    urx' =  urx;
   *    ury' = -lly;
   * ****/
  rects_[FS - N].SetValue(llx, -ury, urx, -lly);

  /****
   * rotate 90 degree counterclockwise
   *    llx' = -ury;
   *    lly' =  llx;
   *    urx' = -lly;
   *    ury' =  urx;
   * flip along the y-direction through the location of this pin
   *    llx' =  lly;
   *    lly' =  llx;
   *    urx' =  ury;
   *    ury' =  urx;
   * ****/
  rects_[FW - N].SetValue(lly, llx, ury, urx);

  /****
   * rotate 270 degree counterclockwise
   *    llx' =  lly;
   *    lly' = -urx;
   *    urx' =  ury;
   *    ury' = -llx;
   * flip along the y-direction through the location of this pin
   *    llx' = -ury;
   *    lly' = -urx;
   *    urx' = -lly;
   *    ury' = -llx;
   * ****/
  rects_[FE - N].SetValue(-ury, -urx, -lly, -llx);
}

}
