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
 * Testcase for manual I/O pin placement: `place-io -place`.
 *
 * Manual placement fixes one named pin at an explicit location -- which may be
 * anywhere, including the interior of the die, not only a boundary edge. The
 * test places one pin at the die centre, auto-places the rest, and checks that
 * the manual pin kept its interior location while the others went to the
 * boundary.
 */

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

  // Pick the first pin to place manually, and the die centre as its interior
  // target (in microns).
  std::string pin_name = p_phy_db->design().GetIoPinsRef()[0].GetName();
  auto die = p_phy_db->design().GetDieArea();
  int units = p_phy_db->design().GetUnitsDistanceMicrons();
  double cx_um = 0.5 * (die.LLX() + die.URX()) / units;
  double cy_um = 0.5 * (die.LLY() + die.URY()) / units;

  Dali dali(p_phy_db, severity::info, "");
  dali.InstantiateIoPlacer();

  // Fix the chosen pin at the die centre on Metal1.
  std::string sx = std::to_string(cx_um);
  std::string sy = std::to_string(cy_um);
  char c0[] = "place-io", c1[] = "--place";
  std::vector<std::string> a = {pin_name, "Metal1", "0", "0", "0.1", "2.0",
                                sx,       sy,       "N"};
  std::vector<char*> argv = {c0, c1};
  for (auto& s : a) argv.push_back(&s[0]);
  if (!dali.IoPinPlacement(static_cast<int>(argv.size()), argv.data())) {
    return FAIL;
  }

  // Auto-place the remaining pins on Metal1; the fixed pin must be left alone.
  if (!dali.SetIoPlacerGlobalMetalLayer("Metal1")) return FAIL;
  if (!dali.RunIoPinAutoPlacement()) return FAIL;

  dali.ExportToPhyDB();
  std::string out_def = "manual_place_io_pin.def";
  p_phy_db->WriteDef(out_def);
  delete p_phy_db;

  p_phy_db = new phydb::PhyDB;
  p_phy_db->ReadLef(lef_file_name);
  p_phy_db->ReadDef(out_def);

  // The manually placed pin kept its interior location (not snapped to an edge).
  bool manual_ok = false;
  for (auto& iopin : p_phy_db->design().GetIoPinsRef()) {
    if (iopin.GetName() != pin_name) continue;
    auto loc = iopin.GetLocation();
    auto d = p_phy_db->design().GetDieArea();
    bool on_edge = loc.x == d.LLX() || loc.x == d.URX() ||
                   loc.y == d.LLY() || loc.y == d.URY();
    manual_ok = (iopin.GetPlacementStatus() == phydb::PlaceStatus::FIXED) &&
                !on_edge;
    break;
  }
  if (!manual_ok) {
    delete p_phy_db;
    return FAIL;
  }

  // The rest are legally placed.
  bool is_legal = IsNoIoPinOverlapAndSpacingViolation(p_phy_db) &&
                  IsEveryIoPinInsideDieArea(p_phy_db);
  delete p_phy_db;
  return is_legal ? SUCCESS : FAIL;
}
