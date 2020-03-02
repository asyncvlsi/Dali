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
  WellLayer *GetNLayer() const;
  WellLayer *GetPLayer() const;
  void SetNLayer(double width, double spacing, double op_spacing, double max_plug_dist, double overhang);
  void SetPLayer(double width, double spacing, double op_spacing, double max_plug_dist, double overhang);

  void Report();
};

inline WellLayer *Tech::GetNLayer() const {
  return n_layer_;
}

inline WellLayer *Tech::GetPLayer() const {
  return p_layer_;
}

#endif //DALI_SRC_CIRCUIT_TECH_H_
