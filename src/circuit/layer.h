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
  void SetWidth(double width);
  void SetSpacing(double spacing);
  double Width();
  double Spacing();
};

class MetalLayer : public Layer {
 private:
  std::pair<const std::string, int> *name_num_ptr_;
  double area_;
  double x_pitch_;
  double y_pitch_;
  MetalDirection direction_;

 public:
  explicit MetalLayer(std::pair<const std::string, int> *name_num_ptr);
  MetalLayer(double width,
             double spacing,
             std::pair<const std::string, int> *name_num_ptr,
             MetalDirection direction = HORIZONTAL);
  void SetArea(double area);
  void SetPitch(double x_pitch, double y_pitch);
  void SetDirection(MetalDirection metal_direction);
  const std::string *Name() const;
  int Num() const;
  double Area();
  double PitchX();
  double PitchY();
  MetalDirection Direction();
  void Report();
};

class WellLayer : public Layer {
 private:
  double op_spacing_;
  double max_plug_dist_;
 public:
  WellLayer(double width, double spacing, double op_spacing, double max_plug_dist);
  double Width() const;
  double Spacing() const;
  double OpSpacing() const;
  double MaxPlugDist() const;

  void SetOpSpacing(double op_spacing);
  void SetMaxPlugDist(double max_plug_dist);
  void SetParams(double width, double height, double op_spacing, double max_plug_dist);
  void Report();
};

inline void Layer::SetWidth(double width) {
  Assert(width >= 0, "Negative width?");
  width_ = width;
}

inline void Layer::SetSpacing(double spacing) {
  Assert(spacing >= 0, "Negative spacing?");
  spacing_ = spacing;
}

inline double Layer::Width() {
  return width_;
}

inline double Layer::Spacing() {
  return spacing_;
}

inline void MetalLayer::SetArea(double area) {
  Assert(area >= 0, "Negative minarea?");
  area_ = area;
}

inline void MetalLayer::SetPitch(double x_pitch, double y_pitch) {
  Assert(x_pitch >= 0 && y_pitch >= 0, "Negative metal pitch?");
  x_pitch_ = x_pitch;
  y_pitch_ = y_pitch;
}

inline void MetalLayer::SetDirection(MetalDirection metal_direction) {
  direction_ = metal_direction;
}

inline const std::string *MetalLayer::Name() const {
  return &(name_num_ptr_->first);
}

inline int MetalLayer::Num() const {
  return name_num_ptr_->second;
}

inline double MetalLayer::Area() {
  return area_;
}

inline double MetalLayer::PitchX() {
  return x_pitch_;
}

inline double MetalLayer::PitchY() {
  return y_pitch_;
}

inline MetalDirection MetalLayer::Direction() {
  return direction_;
}

inline double WellLayer::Width() const {
  return width_;
}

inline double WellLayer::Spacing() const {
  return spacing_;
}

inline double WellLayer::OpSpacing() const {
  return op_spacing_;
}

inline double WellLayer::MaxPlugDist() const {
  return max_plug_dist_;
}

inline void WellLayer::SetOpSpacing(double op_spacing) {
  Assert(op_spacing >= 0, "Negative opposite spacing?");
  op_spacing_ = op_spacing;
}

inline void WellLayer::SetMaxPlugDist(double max_plug_dist) {
  Assert(max_plug_dist >= 0, "Negative max plug distance?");
  max_plug_dist_ = max_plug_dist;
}

#endif //DALI_SRC_CIRCUIT_LAYER_H_
