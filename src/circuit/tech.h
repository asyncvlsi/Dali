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
  WellLayer *n_well_;
  WellLayer *p_well_;
 public:
  Tech();
  ~Tech();
  WellLayer *GetNWell() const {return n_well_;}
  WellLayer *GetPWell() const {return p_well_;}
  void SetNWell(double width, double spacing, double op_spacing, double max_plug_dist);
  void SetPWell(double width, double spacing, double op_spacing, double max_plug_dist);

  void Report();
};

#endif //DALI_SRC_CIRCUIT_TECH_H_
