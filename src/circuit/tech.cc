//
// Created by Yihang Yang on 12/22/19.
//

#include "tech.h"

Tech::Tech() : n_set_(false), p_set_(false), n_layer_ptr_(nullptr), p_layer_ptr_(nullptr) {}

Tech::~Tech() {
  /****
  * This destructor free the memory allocated for unordered_map<key, *T>
  * because T is initialized by
  *    auto *T = new T();
  * ****/
  for (auto &pair: block_type_map) {
    delete pair.second;
  }

  // free the space for well layers
  delete (n_layer_ptr_);
  delete (p_layer_ptr_);
}

void Tech::SetNLayer(double width, double spacing, double op_spacing, double max_plug_dist, double overhang) {
  if (n_set_) {
    n_layer_ptr_->SetParams(width, spacing, op_spacing, max_plug_dist, overhang);
  } else {
    n_layer_ptr_ = new WellLayer(width, spacing, op_spacing, max_plug_dist, overhang);
    n_set_ = true;
  }
}

void Tech::SetPLayer(double width, double spacing,double op_spacing, double max_plug_dist, double overhang) {
  if (p_set_) {
    p_layer_ptr_->SetParams(width, spacing, op_spacing, max_plug_dist, overhang);
  } else {
    p_layer_ptr_ = new WellLayer(width, spacing, op_spacing, max_plug_dist, overhang);
    p_set_ = true;
  }
}

void Tech::SetDiffSpacing(double same_diff, double any_diff) {
  same_diff_spacing_ = same_diff;
  any_diff_spacing_ = any_diff;
}

void Tech::Report() {
  if (n_set_) {
    std::cout << "  Layer name: nwell\n";
    n_layer_ptr_->Report();
  }
  if (p_set_) {
    std::cout << "  Layer name: pwell\n";
    p_layer_ptr_->Report();
  }
}
