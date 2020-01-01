//
// Created by Yihang Yang on 12/22/19.
//

#ifndef DALI_SRC_CIRCUIT_TECH_H_
#define DALI_SRC_CIRCUIT_TECH_H_

#include "layer.h"

class Tech {
 private:
  bool n_set_;
  bool p_set_;
  WellLayer *n_layer_;
  WellLayer *p_layer_;
 public:
  Tech();
  ~Tech();
  WellLayer *GetNLayer() const {return n_layer_;}
  WellLayer *GetPLayer() const {return p_layer_;}
  void SetNLayer(double width, double spacing, double op_spacing, double max_plug_dist);
  void SetPLayer(double width, double spacing, double op_spacing, double max_plug_dist);

  void Report();
};

#endif //DALI_SRC_CIRCUIT_TECH_H_
