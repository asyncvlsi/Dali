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
  for (auto &pair: block_type_map_) {
    delete pair.second;
  }
}

double Tech::GetManufacturingGrid() const {
  DaliExpects(manufacturing_grid_ > 0, "Manufacturing grid not set");
  return manufacturing_grid_;
}

std::vector<BlockType *> &Tech::WellTapCellPtrs() {
  return well_tap_cell_ptrs_;
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
