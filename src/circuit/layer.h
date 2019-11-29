//
// Created by Yihang Yang on 11/28/19.
//

#ifndef HPCC_SRC_CIRCUIT_LAYER_H_
#define HPCC_SRC_CIRCUIT_LAYER_H_

#include <string>
#include "status.h"

class Layer {
 protected:
  std::pair<const std::string, int>* name_num_ptr_;
  double width_;
  double spacing_;
 public:
  Layer(std::pair<const std::string, int>* name_num_ptr, double width, double spacing):
      name_num_ptr_(name_num_ptr), width_(width), spacing_(spacing) {}
  double Width() {return width_;}
  double Spacing() {return spacing_;}
  const std::string *Name() const { return &(name_num_ptr_->first);}
};

class MetalLayer: public Layer {
 private:
  double area_;
  double pitch_;
  MetalDirection direction_;
 public:
  MetalLayer(std::pair<const std::string, int>* name_num_ptr, double width, double spacing, MetalDirection direction=HORIZONTAL):
      Layer(name_num_ptr, width, spacing), area_(0), pitch_(0), direction_(direction) {};
  double Area() {return area_;}
  double Pitch() {return pitch_;}
  MetalDirection Direction() {return direction_;}
};

#endif //HPCC_SRC_CIRCUIT_LAYER_H_
