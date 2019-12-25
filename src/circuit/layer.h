//
// Created by Yihang Yang on 11/28/19.
//

#ifndef DALI_SRC_CIRCUIT_LAYER_H_
#define DALI_SRC_CIRCUIT_LAYER_H_

#include <string>
#include "status.h"
#include "../common/misc.h"

class Layer {
 protected:
  double width_;
  double spacing_;
 public:
  Layer(): width_(0), spacing_(0) {}
  Layer(double width, double spacing): width_(width), spacing_(spacing) {}
  void SetWidth(double width) {Assert(width>=0, "Negative width?"); width_=width;}
  void SetSpacing(double spacing) {Assert(spacing>=0, "Negative spacing?"); spacing_=spacing;}
  double Width() {return width_;}
  double Spacing() {return spacing_;}
};

class MetalLayer: public Layer {
 private:
  std::pair<const std::string, int>* name_num_ptr_;
  double area_;
  double x_pitch_;
  double y_pitch_;
  MetalDirection direction_;
 public:
  explicit MetalLayer(std::pair<const std::string, int>* name_num_ptr);
  MetalLayer(double width, double spacing, std::pair<const std::string, int>* name_num_ptr, MetalDirection direction=HORIZONTAL);
  void SetArea(double area) {Assert(area>=0, "Negative minarea?"); area_ = area;}
  void SetPitch(double x_pitch, double y_pitch) {Assert(x_pitch>=0&&y_pitch>=0, "Negative metal pitch?"); x_pitch_=x_pitch; y_pitch_=y_pitch;}
  void SetDirection(MetalDirection metal_direction) {direction_ = metal_direction;}
  const std::string *Name() const {return &(name_num_ptr_->first);}
  int Num() const {return name_num_ptr_->second;}
  double Area() {return area_;}
  double PitchX() {return x_pitch_;}
  double PitchY() {return y_pitch_;}
  MetalDirection Direction() {return direction_;}
  void Report();
};

class WellLayer: public Layer {
 private:
  double op_spacing_;
  double max_plug_dist_;
 public:
  WellLayer(double width, double spacing, double op_spacing, double max_plug_dist):
      Layer(width, spacing),
      op_spacing_(op_spacing),
      max_plug_dist_(max_plug_dist) {
    Assert(op_spacing >= 0, "Negative opposite spacing?");
    Assert(max_plug_dist_>=0, "Negative maximum plug distance?");
  }
  double Width() const {return width_;}
  double Spacing() const {return spacing_;}
  double OpSpacing() const {return op_spacing_;}
  double MaxPlugDist() const {return max_plug_dist_;}

  void SetOpSpacing(double op_spacing) {Assert(op_spacing>=0, "Negative opposite spacing?"); op_spacing_=op_spacing;}
  void SetMaxPlugDist(double max_plug_dist) {Assert(max_plug_dist>=0, "Negative max plug distance?"); max_plug_dist_=max_plug_dist;}
  void SetParams(double width, double height, double op_spacing, double max_plug_dist);
  void Report();
};

#endif //DALI_SRC_CIRCUIT_LAYER_H_
