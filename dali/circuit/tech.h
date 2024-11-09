/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#ifndef DALI_CIRCUIT_TECH_H_
#define DALI_CIRCUIT_TECH_H_

#include <phydb/phydb.h>

#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

#include "block.h"
#include "block_type.h"
#include "dali/common/named_instance_collection.h"
#include "layer.h"

namespace dali {

class Tech {
  friend class Circuit;

 public:
  Tech();

  double GetManufacturingGrid() const;

  // get all kinds of well tap cells
  std::vector<int> &WellTapCellIds();

  // get all filler cells
  std::vector<std::unique_ptr<BlockType>> &FillerCellPtrs();

  // get the dummy BlockType for IOPINs
  BlockType *IoDummyBlkTypePtr();

  // get Nwell layer
  WellLayer &NwellLayer();

  // get Pwell layer
  WellLayer &PwellLayer();

  // is Nwell parameters available?
  bool IsNwellSet() const;

  // is Pwell parameters available?
  bool IsPwellSet() const;

  // is any well layer available?
  bool IsWellInfoSet() const;

  static bool IsGndAtBottom(phydb::Macro *macro);

  // get block types
  std::vector<BlockType> &BlockTypes();
  NamedInstanceCollection<BlockType> &BlockTypeCollection() {
    return block_type_collection_;
  }
  NamedInstanceCollection<BlockType> &EndCapCellTypeCollection() {
    return end_cap_cell_type_collection_;
  }

  // create fake well for standard cells
  void CreateFakeWellForStandardCell(phydb::PhyDB *phy_db);

  // get end cap cell info
  int PreEndCapMinWidth() const;
  int PreEndCapMinPHeight() const;
  int PreEndCapMinNHeight() const;
  int PostEndCapMinWidth() const;
  int PostEndCapMinPHeight() const;
  int PostEndCapMinNHeight() const;

  // print information
  void Report() const;

 private:
  /**** manufacturing grid ****/
  double manufacturing_grid_ = 0;
  int database_microns_ = 0;

  /**** grid value along X and Y ****/
  bool is_grid_set_ = false;
  double grid_value_x_ = 1;
  double grid_value_y_ = 1;

  /**** metal layers ****/
  std::vector<MetalLayer> metal_list_;
  std::unordered_map<std::string, int> metal_name_map_;

  /**** macros ****/
  NamedInstanceCollection<BlockType> block_type_collection_;
  BlockType *io_dummy_blk_type_ptr_ = nullptr;
  std::vector<int> well_tap_cell_type_ids_;
  std::vector<std::unique_ptr<BlockType>> filler_ptrs_;
  // pre and post end cap cell types are for standard cell placement
  BlockType *pre_end_cap_cell_ptr_ = nullptr;
  BlockType *post_end_cap_cell_ptr_ = nullptr;
  NamedInstanceCollection<BlockType> end_cap_cell_type_collection_;

  /**** row height ****/
  double row_height_ = 0;
  bool row_height_set_ = false;

  /**** N/P well info ****/
  bool n_set_ = false;
  bool p_set_ = false;
  WellLayer nwell_layer_;
  WellLayer pwell_layer_;
  double same_diff_spacing_;
  double any_diff_spacing_;

  /**** end cap width and height info ****/
  std::optional<double> pre_end_cap_min_width_ = std::nullopt;
  std::optional<double> pre_end_cap_min_p_height_ = std::nullopt;
  std::optional<double> pre_end_cap_min_n_height_ = std::nullopt;
  std::optional<double> post_end_cap_min_width_ = std::nullopt;
  std::optional<double> post_end_cap_min_p_height_ = std::nullopt;
  std::optional<double> post_end_cap_min_n_height_ = std::nullopt;
};

}  // namespace dali

#endif  // DALI_CIRCUIT_TECH_H_
