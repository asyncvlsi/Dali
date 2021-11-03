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
#include <phydb/phydb.h>

#include "dali/dali.h"
#include "helper.h"

#define SUCCESS 0
#define FAIL 1

using namespace dali;

int main() {
  std::string lef_file_name = "ispd19_test3.input.lef";
  std::string def_file_name = "ispd19_test3.input.def";

  // initialize PhyDB
  auto *p_phy_db = new phydb::PhyDB;
  p_phy_db->ReadLef(lef_file_name);
  p_phy_db->ReadDef(def_file_name);

  // un-place all iopins
  SetAllIoPinsToUnplaced(p_phy_db);

  // initialize Dali
  Dali dali(p_phy_db, boost::log::trivial::info, "", true);

  /****
   *  this is the part of code to implement
   */

  bool is_ioplace_success = dali.StartIoPinAutoPlacement();
  if (!is_ioplace_success) {
    return FAIL;
  }

  // export the result to PhyDB and save the result to a DEF file
  dali.ExportToPhyDB();
  std::string out_def_file_name = "all_iopin_use_same_metal_layer.def";
  p_phy_db->WriteDef(out_def_file_name);
  delete p_phy_db;

  // read the DEF file back to the memory, and perform checking
  p_phy_db = new phydb::PhyDB;
  p_phy_db->ReadLef(lef_file_name);
  p_phy_db->ReadDef(out_def_file_name);

  bool is_legal;

  is_legal = IsEveryIoPinPlacedOnBoundary(p_phy_db);
  if (!is_legal) return FAIL;

  is_legal = IsNoIoPinOverlapAndSpacingViolation(p_phy_db);
  if (!is_legal) return FAIL;

  is_legal = IsEveryIoPinManufacturable(p_phy_db);
  if (!is_legal) return FAIL;

  is_legal = IsEveryIoPinInsideDieArea(p_phy_db);
  if (!is_legal) return FAIL;

  return SUCCESS;
}
