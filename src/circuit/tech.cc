//
// Created by Yihang Yang on 12/22/19.
//

#include "tech.h"

Tech::Tech(): n_set_(false), p_set_(false), n_well_(nullptr), p_well_(nullptr) {}

Tech::~Tech() {
  free(n_well_);
  free(p_well_);
}

void Tech::SetNWell(double width, double spacing, double op_spacing, double max_plug_dist) {
  if (n_set_) {
    n_well_->SetParams(width, spacing, op_spacing, max_plug_dist);
  } else {
    n_well_ = new WellLayer(width, spacing, op_spacing, max_plug_dist);
    n_set_ = true;
  }
}

void Tech::SetPWell(double width, double spacing, double op_spacing, double max_plug_dist) {
  if (p_set_) {
    p_well_->SetParams(width, spacing, op_spacing, max_plug_dist);
  } else {
    p_well_ = new WellLayer(width, spacing, op_spacing, max_plug_dist);
    p_set_ = true;
  }
}

void Tech::Report() {
  if (n_set_) {
    std::cout << "  Layer name: nwell\n";
    n_well_->Report();
  }
  if (p_set_) {
    std::cout << "  Layer name: pwell\n";
    p_well_->Report();
  }
}
