/*******************************************************************************
 *
 * Copyright (c) 2024 Yihang Yang
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

/**
 * @file
 * Testcase for the argv-style command dispatcher Dali::IoPinPlacement.
 *
 * The interactive flow reaches the I/O placer through Dali::IoPinPlacement,
 * which takes an argv array: argv[0] is the command name and argv[1] onward are
 * its options, e.g. {"place-io", "Metal1"} to auto-place every pin on Metal1.
 * That entry point had no test and, after its only caller was removed, no
 * worked example of the calling convention either. This exercises it end to end
 * and checks the placement it produces.
 */

#include "helper.h"

#include "dali/dali.h"

#define SUCCESS 0
#define FAIL 1

using namespace dali;

int main() {
  std::string lef_file_name = "ispd19_test3.input.lef";
  std::string def_file_name = "ispd19_test3.input.def";

  auto* p_phy_db = new phydb::PhyDB;
  p_phy_db->ReadLef(lef_file_name);
  p_phy_db->ReadDef(def_file_name);

  SetAllIoPinsToUnplaced(p_phy_db);

  Dali dali(p_phy_db, severity::info, "");

  // The argv convention: argv[0] is the command, argv[1] the option. A bare
  // metal-layer name selects auto-placement on that layer, equivalent to
  // `place-io -c` to set the layer then `place-io -ap` to place. This mirrors
  // how the interactive command line invokes the placer.
  char command[] = "place-io";
  char metal_layer[] = "Metal1";
  char* argv[] = {command, metal_layer};
  bool is_ioplace_success = dali.IoPinPlacement(2, argv);
  if (!is_ioplace_success) {
    return FAIL;
  }

  dali.ExportToPhyDB();
  std::string out_def_file_name = "io_pin_placement_argv_command.def";
  p_phy_db->WriteDef(out_def_file_name);
  delete p_phy_db;

  p_phy_db = new phydb::PhyDB;
  p_phy_db->ReadLef(lef_file_name);
  p_phy_db->ReadDef(out_def_file_name);

  bool is_legal = true;
  is_legal = is_legal && IsEveryIoPinPlacedOnBoundary(p_phy_db);
  is_legal = is_legal && IsNoIoPinOverlapAndSpacingViolation(p_phy_db);
  is_legal = is_legal && IsEveryIoPinOnMetal(p_phy_db, "Metal1");
  is_legal = is_legal && IsEveryIoPinInsideDieArea(p_phy_db);
  is_legal = is_legal && IsEveryIoPinManufacturable(p_phy_db);

  delete p_phy_db;

  return is_legal ? SUCCESS : FAIL;
}
