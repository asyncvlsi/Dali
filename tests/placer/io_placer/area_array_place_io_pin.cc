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
 * Testcase for interior area-array I/O placement: `place-io -area`.
 *
 * Area-array (flip-chip) placement puts pins on an interior lattice on a top
 * layer rather than on the perimeter edges. The test area-places every pin on a
 * grid sized to cover them, then checks each pin landed strictly inside the die
 * (off every boundary), on the requested layer, without overlap or spacing
 * violations.
 */

#include <cmath>
#include <string>
#include <vector>

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

  size_t num_pins = p_phy_db->design().GetIoPinsRef().size();
  // Smallest square lattice that covers every pin.
  int side = static_cast<int>(std::ceil(std::sqrt((double)num_pins)));
  std::string rows = std::to_string(side);
  std::string cols = std::to_string(side);

  Dali dali(p_phy_db, severity::info, "");
  dali.InstantiateIoPlacer();

  char c0[] = "place-io", c1[] = "-area";
  std::vector<std::string> a = {"Metal1", rows, cols};
  std::vector<char*> argv = {c0, c1};
  for (auto& s : a) argv.push_back(&s[0]);
  if (!dali.IoPinPlacement(static_cast<int>(argv.size()), argv.data())) {
    return FAIL;
  }

  dali.ExportToPhyDB();
  std::string out_def = "area_array_place_io_pin.def";
  p_phy_db->WriteDef(out_def);
  delete p_phy_db;

  p_phy_db = new phydb::PhyDB;
  p_phy_db->ReadLef(lef_file_name);
  p_phy_db->ReadDef(out_def);

  // Every pin is interior: on no boundary edge.
  auto die = p_phy_db->design().GetDieArea();
  bool all_interior = true;
  for (auto& iopin : p_phy_db->design().GetIoPinsRef()) {
    auto loc = iopin.GetLocation();
    bool on_edge = loc.x == die.LLX() || loc.x == die.URX() ||
                   loc.y == die.LLY() || loc.y == die.URY();
    if (on_edge) {
      all_interior = false;
      break;
    }
  }

  bool is_legal = all_interior &&
                  IsNoIoPinOverlapAndSpacingViolation(p_phy_db) &&
                  IsEveryIoPinOnMetal(p_phy_db, "Metal1") &&
                  IsEveryIoPinInsideDieArea(p_phy_db) &&
                  IsEveryIoPinManufacturable(p_phy_db);
  delete p_phy_db;
  return is_legal ? SUCCESS : FAIL;
}
