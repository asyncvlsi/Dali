//
// Created by Yihang Yang on 11/28/19.
//

#ifndef HPCC_SRC_CIRCUIT_LAYER_H_
#define HPCC_SRC_CIRCUIT_LAYER_H_

#include <string>

class Layer {
 protected:
  std::string name_;
  double width_;
  double spacing_;
 public:
  Layer(std::string &name, double width, double spacing): name_(name), width_(width), spacing_(spacing) {}
  double Width() {return width_;}
  double Spacing() {return spacing_;}
};

class MetalLayer: public Layer {
 private:
  bool direction_;
  double area_;
  double pitch_;
 public:
  MetalLayer(std::string &name, double width, double spacing): Layer(name, width, spacing) {};
};

#endif //HPCC_SRC_CIRCUIT_LAYER_H_
