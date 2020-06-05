//
// Created by Yihang Yang on 11/28/19.
//

#ifndef DALI_SRC_CIRCUIT_LAYER_H_
#define DALI_SRC_CIRCUIT_LAYER_H_

#include <string>

#include "common/misc.h"
#include "status.h"

class Layer {
 protected:
  double width_;
  double spacing_;

 public:
  Layer();
  Layer(double width, double spacing);
  void SetWidth(double width) {
    Assert(width >= 0, "Negative width not allowed: Layer::SetWidth()\n");
    width_ = width;
  }
  void SetSpacing(double spacing) {
    Assert(spacing >= 0, "Negative spacing not allowed: Layer::SetSpacing()\n");
    spacing_ = spacing;
  }
  double Width() const { return width_; }
  double Spacing() const {
    return spacing_;
  }
};

class MetalLayer : public Layer {
 private:
  std::pair<const std::string, int> *name_num_pair_ptr_;
  double min_area_;
  double x_pitch_;
  double y_pitch_;
  MetalDirection direction_;

 public:
  explicit MetalLayer(std::pair<const std::string, int> *name_num_pair_ptr);
  MetalLayer(double width,
             double spacing,
             std::pair<const std::string, int> *name_num_pair_ptr,
             MetalDirection direction = HORIZONTAL_);
  void SetArea(double area) {
    Assert(area >= 0, "Negative minarea?");
    min_area_ = area;
  }
  void SetPitch(double x_pitch, double y_pitch) {
    Assert(x_pitch >= 0 && y_pitch >= 0, "Negative metal pitch?");
    x_pitch_ = x_pitch;
    y_pitch_ = y_pitch;
  }
  void SetPitchAuto() {
    x_pitch_ = width_ + spacing_;
    y_pitch_ = width_ + spacing_;
  }
  void SetDirection(MetalDirection direction) { direction_ = direction; }
  const std::string *Name() const { return &(name_num_pair_ptr_->first); }
  int Num() const { return name_num_pair_ptr_->second; }
  double Area() const { return min_area_; }
  double MinHeight() const { return min_area_ / width_; }
  double PitchX() const { return x_pitch_; }
  double PitchY() const { return y_pitch_; }
  MetalDirection Direction() const { return direction_; }
  void Report() const;
};

class WellLayer : public Layer {
 private:
  double op_spacing_;
  double max_plug_dist_;
  double overhang_;
 public:
  WellLayer(double width, double spacing, double op_spacing, double max_plug_dist, double overhang);
  double OpSpacing() const { return op_spacing_; }
  double MaxPlugDist() const { return max_plug_dist_; }
  double Overhang() const { return overhang_; }

  void SetOpSpacing(double op_spacing) {
    Assert(op_spacing >= 0, "Negative opposite spacing not allowed: WellLayer::SetOpSpacing\n");
    op_spacing_ = op_spacing;
  }
  void SetMaxPlugDist(double max_plug_dist) {
    Assert(max_plug_dist >= 0, "Negative max plug distance not allowed: WellLayer::SetMaxPlugDist\n");
    max_plug_dist_ = max_plug_dist;
  }
  void SetOverhang(double overhang) {
    Assert(overhang >= 0, "Negative well/diffusion overhang not allowed: WellLayer::SetOverhang()\n");
    overhang_ = overhang;
  }
  void SetParams(double width, double height, double op_spacing, double max_plug_dist, double overhang);
  void Report();
};

#endif //DALI_SRC_CIRCUIT_LAYER_H_
