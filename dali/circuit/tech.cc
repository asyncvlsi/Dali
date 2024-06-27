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

#include "tech.h"

#include <cctype>
#include <cfloat>

#include <algorithm>
#include <unordered_set>

#include "dali/common/helper.h"

namespace dali {

Tech::Tech()
    : n_set_(false),
      p_set_(false),
      same_diff_spacing_(-1),
      any_diff_spacing_(-1) {}

/****
* This destructor free the memory allocated for unordered_map<key, *T>
* because T is initialized by
*    auto *T = new T();
* ****/
Tech::~Tech() {
  //for (auto &pair: block_type_map_) {
  //  delete pair.second;
  //}
}

double Tech::GetManufacturingGrid() const {
  DaliExpects(manufacturing_grid_ > 0, "Manufacturing grid not set");
  return manufacturing_grid_;
}

std::vector<BlockType *> &Tech::WellTapCellPtrs() {
  return well_tap_cell_ptrs_;
}

std::vector<std::unique_ptr<BlockType>> &Tech::FillerCellPtrs() {
  return filler_ptrs_;
}

BlockType *Tech::IoDummyBlkTypePtr() {
  return io_dummy_blk_type_ptr_;
}

WellLayer &Tech::NwellLayer() {
  return nwell_layer_;
}

WellLayer &Tech::PwellLayer() {
  return pwell_layer_;
}

bool Tech::IsNwellSet() const {
  return n_set_;
}

bool Tech::IsPwellSet() const {
  return p_set_;
}

bool Tech::IsWellInfoSet() const {
  return n_set_ || p_set_;
}

std::list<BlockTypeWell> &Tech::MultiWells() {
  return multi_well_list_;
}

bool Tech::IsGndAtBottom(phydb::Macro *macro) {
  const std::unordered_set<std::string> gnd_names = {"vss", "gnd"};
  const std::unordered_set<std::string> vdd_names = {"vdd"};

  double gnd_bottom_line = DBL_MAX;
  double vdd_bottom_line = DBL_MAX;
  for (auto &pin : macro->GetPinsRef()) {
    std::string pin_name = pin.GetName();
    std::for_each(
        pin_name.begin(),
        pin_name.end(),
        [](char &c) { c = tolower(c); }
    );
    if (gnd_names.find(pin_name) != gnd_names.end()) {
      for (auto &layer_rect : pin.GetLayerRectRef()) {
        for (auto &rect : layer_rect.GetRects()) {
          gnd_bottom_line = std::min(gnd_bottom_line, rect.LLY());
        }
      }
    } else if (vdd_names.find(pin_name) != vdd_names.end()) {
      for (auto &layer_rect : pin.GetLayerRectRef()) {
        for (auto &rect : layer_rect.GetRects()) {
          vdd_bottom_line = std::min(vdd_bottom_line, rect.LLY());
        }
      }
    } else {
      continue;
    }
  }

  if ((gnd_bottom_line < DBL_MAX) && (vdd_bottom_line < DBL_MAX)) {
    return gnd_bottom_line < vdd_bottom_line;
  }
  if (gnd_bottom_line < DBL_MAX) {
    return (gnd_bottom_line - 0) < (gnd_bottom_line - macro->GetHeight());
  }
  if (vdd_bottom_line < DBL_MAX) {
    return (vdd_bottom_line - 0) > (vdd_bottom_line - macro->GetHeight());
  }
  DaliExpects(false,
              "Cannot find a power signal in this macro: " + macro->GetName());
  return true;
}

void Tech::CreateFakeWellForStandardCell(phydb::PhyDB *phy_db) {
  std::unordered_set<int> height_set;
  for (auto &[name, blk_type] : block_type_map_) {
    if (blk_type == io_dummy_blk_type_ptr_) continue;
    height_set.insert(blk_type->Height());
    multi_well_list_.emplace_back(blk_type);
  }

  DaliExpects(!height_set.empty(), "No cell height?");

  std::vector<int> height_vec(height_set.begin(), height_set.end());
  std::sort(
      height_vec.begin(),
      height_vec.end(),
      [](const int &h0, const int &h1) { return h0 < h1; }
  );

  int standard_height = height_vec[0];
  int n_height = standard_height / 2;
  int p_height = standard_height - n_height;
  DaliExpects(n_height > 0 || p_height > 0, "Both heights are 0?");

  for (auto &[name, blk_type] : block_type_map_) {
    if (blk_type == io_dummy_blk_type_ptr_) continue;
    auto *macro = phy_db->GetMacroPtr(name);
    int region_cnt = (int) std::round(blk_type->Height() / standard_height);
    auto *well_ptr = blk_type->WellPtr();
    int accumulative_height = 0;
    bool is_pwell = IsGndAtBottom(macro);
    for (int i = 0; i < 2 * region_cnt; ++i) {
      if (is_pwell) {
        well_ptr->AddNwellRect(
            0, accumulative_height,
            blk_type->Width(), accumulative_height + n_height
        );
        accumulative_height += n_height;
      } else {
        well_ptr->AddPwellRect(
            0, accumulative_height,
            blk_type->Width(), accumulative_height + p_height
        );
        accumulative_height += p_height;
      }
      is_pwell = !is_pwell;
    }
  }
}

int Tech::PreEndCapMinWidth() const {
  return static_cast<int>(RoundOrCeiling(pre_end_cap_min_width_.value_or(0) / grid_value_x_));
}

int Tech::PreEndCapMinPHeight() const {
  return static_cast<int>(RoundOrCeiling(pre_end_cap_min_p_height_.value_or(0) / grid_value_y_));
}

int Tech::PreEndCapMinNHeight() const {
  return static_cast<int>(RoundOrCeiling(pre_end_cap_min_n_height_.value_or(0) / grid_value_y_));
}

int Tech::PostEndCapMinWidth() const {
  return static_cast<int>(RoundOrCeiling(post_end_cap_min_width_.value_or(0) / grid_value_x_));
}

int Tech::PostEndCapMinPHeight() const {
  return static_cast<int>(RoundOrCeiling(post_end_cap_min_p_height_.value_or(0) / grid_value_y_));
}

int Tech::PostEndCapMinNHeight() const {
  return static_cast<int>(RoundOrCeiling(post_end_cap_min_n_height_.value_or(0) / grid_value_y_));
}

void Tech::Report() const {
  if (n_set_) {
    BOOST_LOG_TRIVIAL(info) << "  Layer name: nwell\n";
    nwell_layer_.Report();
  }
  if (p_set_) {
    BOOST_LOG_TRIVIAL(info) << "  Layer name: pwell\n";
    pwell_layer_.Report();
  }
}

}
