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

#include <list>
#include <unordered_map>
#include <vector>

#include <phydb/phydb.h>

#include "block.h"
#include "block_type.h"
#include "layer.h"

namespace dali {

class Tech {
  friend class Circuit;
 public:
  Tech();
  ~Tech();

  double GetManufacturingGrid() const;

  // get all kinds of well tap cells
  std::vector<BlockType *> &WellTapCellPtrs();

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

  // get all multi-well for cells
  std::list<BlockTypeWell> &MultiWells();

  static bool IsGndAtBottom(phydb::Macro *macro);

  // create fake well for standard cells
  void CreateFakeWellForStandardCell(phydb::PhyDB *phy_db);

  // print information
  void Report() const;

 private:
  /**** manufacturing grid ****/
  double manufacturing_grid_ = 0;
  int32_t database_microns_ = 0;

  /*** *grid value along X and Y ****/
  bool is_grid_set_ = false;
  double grid_value_x_ = 1;
  double grid_value_y_ = 1;

  /**** metal layers ****/
  std::vector<MetalLayer> metal_list_;
  std::unordered_map<std::string, int32_t> metal_name_map_;

  /**** macros ****/
  std::unordered_map<std::string, BlockType *> block_type_map_;
  std::list<BlockTypeWell> multi_well_list_;
  BlockType *io_dummy_blk_type_ptr_ = nullptr;
  std::vector<BlockType *> well_tap_cell_ptrs_;

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
};

}

#endif //DALI_CIRCUIT_TECH_H_
