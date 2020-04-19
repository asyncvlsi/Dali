//
// Created by Yihang Yang on 12/22/19.
//

#ifndef DALI_SRC_CIRCUIT_TECH_H_
#define DALI_SRC_CIRCUIT_TECH_H_

#include <unordered_map>
#include <vector>

#include "block.h"
#include "blocktype.h"
#include "layer.h"

class Tech {
 public:
  /****manufacturing grid****/
  double manufacturing_grid = 0;
  int lef_database_microns = 0;

  /****grid value in X and Y****/
  bool grid_set_ = false;
  double grid_value_x_ = 0;
  double grid_value_y_ = 0;

  /****metal layers****/
  std::vector<MetalLayer> metal_list;
  std::unordered_map<std::string, int> metal_name_map;

  /****macros****/
  std::unordered_map<std::string, BlockType *> block_type_map;
  BlockType *well_tap_cell_ = nullptr;

  /****row height****/
  double row_height_ = 0;
  bool row_height_set_ = false;

  /****N/P well info****/
  bool n_set_;
  bool p_set_;
  WellLayer *n_layer_;
  WellLayer *p_layer_;
  double same_diff_spacing_;
  double any_diff_spacing_;

  /********/
  Tech();
  ~Tech();

  BlockType *WellTapCell() const;

  WellLayer *GetNLayer() const;
  WellLayer *GetPLayer() const;
  void SetNLayer(double width, double spacing, double op_spacing, double max_plug_dist, double overhang);
  void SetPLayer(double width, double spacing, double op_spacing, double max_plug_dist, double overhang);
  void SetDiffSpacing(double same_diff, double any_diff);

  bool IsWellInfoSet() const;
  void Report();
};

inline BlockType *Tech::WellTapCell() const {
  return well_tap_cell_;
}

inline WellLayer *Tech::GetNLayer() const {
  return n_layer_;
}

inline WellLayer *Tech::GetPLayer() const {
  return p_layer_;
}

inline bool Tech::IsWellInfoSet() const {
  return (GetNLayer() == nullptr) && (GetPLayer() == nullptr);
}

#endif //DALI_SRC_CIRCUIT_TECH_H_
