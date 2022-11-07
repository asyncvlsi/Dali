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

/****
 * @brief Testcase for IoPlacement command "place-io metal_layer".
 *
 * This is a simple testcase to show that using command
 *     "place-io m1"
 * we can obtain a placement result satisfying:
 * 1. all IOPINs are placed on placement boundary
 * 2. IOPINs does not overlap with each other and respect spacing rules
 * 3. Physical geometries of all IOPINs are on metal layer "Metal1"
 * 4. All locations respect manufacturing grid specified in LEF
 * 5. Physical geometries of all IOPINs are inside the placement region
 *
 * @return 0 if this test is passed, 1 if failed
 */
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
  Dali dali(p_phy_db, boost::log::trivial::info, "");

  // perform IO placement
  std::string layer_name("Metal1");
  dali.InstantiateIoPlacer();
  dali.ConfigIoPlacerAllInOneLayer(layer_name);
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

  is_legal = IsEveryIoPinOnMetal(p_phy_db, layer_name);
  if (!is_legal) return FAIL;

  is_legal = IsEveryIoPinManufacturable(p_phy_db);
  if (!is_legal) return FAIL;

  is_legal = IsEveryIoPinInsideDieArea(p_phy_db);
  if (!is_legal) return FAIL;

  return SUCCESS;
}
