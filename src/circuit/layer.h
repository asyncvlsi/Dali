//
// Created by Yihang Yang on 11/28/19.
//

#ifndef HPCC_SRC_CIRCUIT_LAYER_H_
#define HPCC_SRC_CIRCUIT_LAYER_H_

#include <string>
#include "status.h"
#include "../common/misc.h"

class Layer {
 protected:
  std::pair<const std::string, int>* name_num_ptr_;
  double width_;
  double spacing_;
 public:
  explicit Layer(std::pair<const std::string, int>* name_num_ptr): name_num_ptr_(name_num_ptr), width_(0), spacing_(0) {}
  Layer(std::pair<const std::string, int>* name_num_ptr, double width, double spacing):
      name_num_ptr_(name_num_ptr), width_(width), spacing_(spacing) {}
  void SetWidth(double width) {Assert(width>=0, "Negative width?"); width_=width;}
  void SetSpacing(double spacing) {Assert(spacing>=0, "Negative spacing?"); spacing_=spacing;}
  double Width() {return width_;}
  double Spacing() {return spacing_;}
  const std::string *Name() const { return &(name_num_ptr_->first);}
  int Num() const {return name_num_ptr_->second;}
};

class MetalLayer: public Layer {
 private:
  double area_;
  double x_pitch_;
  double y_pitch_;
  MetalDirection direction_;
 public:
  explicit MetalLayer(std::pair<const std::string, int>* name_num_ptr): Layer(name_num_ptr), area_(0), x_pitch_(0), y_pitch_(0), direction_(HORIZONTAL) {}
  MetalLayer(std::pair<const std::string, int>* name_num_ptr, double width, double spacing, MetalDirection direction=HORIZONTAL):
      Layer(name_num_ptr, width, spacing), area_(0), x_pitch_(0), y_pitch_(0), direction_(direction) {}
  void SetArea(double area) {Assert(area>=0, "Negative minarea?"); area_ = area;}
  void SetPitch(double x_pitch, double y_pitch) {Assert(x_pitch>=0&&y_pitch>=0, "Negative metal pitch?"); x_pitch_=x_pitch; y_pitch_=y_pitch;}
  void SetDirection(MetalDirection metal_direction) {direction_ = metal_direction;}
  double Area() {return area_;}
  double PitchX() {return x_pitch_;}
  double PitchY() {return y_pitch_;}
  MetalDirection Direction() {return direction_;}

  void Report();
};

#endif //HPCC_SRC_CIRCUIT_LAYER_H_
