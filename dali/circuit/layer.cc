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

#include "layer.h"

namespace dali {

Layer::Layer() : width_(0), spacing_(0) {}

Layer::Layer(double width, double spacing) : width_(width), spacing_(spacing) {
  DaliExpects(width >= 0 && spacing >= 0, "Negative width or spacing?");
}

void Layer::SetWidth(double width) {
  DaliExpects(width >= 0, "Negative width not allowed");
  width_ = width;
}

void Layer::SetSpacing(double spacing) {
  DaliExpects(spacing >= 0, "Negative spacing not allowed");
  spacing_ = spacing;
}

MetalLayer::MetalLayer(std::pair<const std::string, int> *name_id_pair_ptr)
    : Layer(),
      name_id_pair_ptr_(name_id_pair_ptr),
      min_area_(0),
      x_pitch_(0),
      y_pitch_(0),
      direction_(HORIZONTAL) {}

MetalLayer::MetalLayer(double width, double spacing,
                       std::pair<const std::string, int> *name_id_pair_ptr,
                       MetalDirection direction)
    : Layer(width, spacing),
      name_id_pair_ptr_(name_id_pair_ptr),
      min_area_(0),
      x_pitch_(0),
      y_pitch_(0),
      direction_(direction) {}

const std::string &MetalLayer::Name() const { return name_id_pair_ptr_->first; }

int MetalLayer::Id() const { return name_id_pair_ptr_->second; }

void MetalLayer::SetMinArea(double min_area) {
  DaliExpects(min_area >= 0, "Negative minarea?");
  min_area_ = min_area;
}

double MetalLayer::MinArea() const { return min_area_; }

double MetalLayer::MinHeight() const { return min_area_ / width_; }

void MetalLayer::SetPitch(double x_pitch, double y_pitch) {
  DaliExpects(x_pitch >= 0 && y_pitch >= 0, "Negative metal pitch?");
  x_pitch_ = x_pitch;
  y_pitch_ = y_pitch;
}

void MetalLayer::SetPitchUsingWidthSpacing() {
  double x_pitch = width_ + spacing_;
  double y_pitch = width_ + spacing_;
  SetPitch(x_pitch, y_pitch);
}

double MetalLayer::PitchX() const { return x_pitch_; }

double MetalLayer::PitchY() const { return y_pitch_; }

void MetalLayer::SetDirection(MetalDirection direction) {
  direction_ = direction;
}

MetalDirection MetalLayer::Direction() const { return direction_; }

std::string MetalLayer::DirectionStr() const {
  return MetalDirectionStr(direction_);
}

void MetalLayer::Report() const {
  BOOST_LOG_TRIVIAL(info) << "  MetalLayer Name: " << Name() << "\n"
                          << "    Assigned Num: " << Id() << "\n"
                          << "    Width and Spacing: " << Width() << " "
                          << Spacing() << "\n"
                          << "    MinArea: " << MinArea() << "\n"
                          << "    Direction: " << DirectionStr() << "\n"
                          << "    Pitch: " << PitchX() << "  " << PitchY()
                          << "\n";
}

WellLayer::WellLayer()
    : Layer(0, 0), opposite_spacing_(-1), max_plug_dist_(-1), overhang_(-1) {}

WellLayer::WellLayer(double width, double spacing, double opposite_spacing,
                     double max_plug_dist, double overhang)
    : Layer(width, spacing),
      opposite_spacing_(opposite_spacing),
      max_plug_dist_(max_plug_dist),
      overhang_(overhang) {
  DaliExpects(opposite_spacing >= 0, "Negative opposite spacing?");
  DaliExpects(max_plug_dist >= 0, "Negative maximum plug distance?");
}

void WellLayer::SetParams(double width, double spacing, double opposite_spacing,
                          double max_plug_dist, double overhang) {
  SetWidth(width);
  SetSpacing(spacing);
  SetOppositeSpacing(opposite_spacing);
  SetMaxPlugDist(max_plug_dist);
  SetOverhang(overhang);
}

void WellLayer::SetOppositeSpacing(double opposite_spacing) {
  DaliExpects(opposite_spacing >= 0, "Negative opposite spacing not allowed");
  opposite_spacing_ = opposite_spacing;
}

double WellLayer::OppositeSpacing() const { return opposite_spacing_; }

void WellLayer::SetMaxPlugDist(double max_plug_dist) {
  DaliExpects(max_plug_dist >= 0, "Negative max plug distance not allowed");
  max_plug_dist_ = max_plug_dist;
}

double WellLayer::MaxPlugDist() const { return max_plug_dist_; }

void WellLayer::SetOverhang(double overhang) {
  DaliExpects(overhang >= 0, "Negative well/diffusion overhang not allowed");
  overhang_ = overhang;
}

double WellLayer::Overhang() const { return overhang_; }

void WellLayer::Report() const {
  BOOST_LOG_TRIVIAL(info) << "    Width:       " << Width() << " um\n"
                          << "    Spacing:     " << Spacing() << " um\n"
                          << "    OppositeSpacing:   " << OppositeSpacing()
                          << " um\n"
                          << "    MaxPlugDist: " << MaxPlugDist() << " um\n"
                          << "    Overhang:    " << Overhang() << "um\n";
}

}  // namespace dali
