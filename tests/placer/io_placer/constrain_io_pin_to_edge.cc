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
 * Testcase for I/O pin edge constraints: `place-io -constraint`.
 *
 * A constrained pin is forced onto the named edge instead of the automatically
 * chosen closest one. The test constrains one pin to the left edge, auto-places
 * every pin, and checks the constrained pin landed on the left boundary.
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

  std::string pin_name = p_phy_db->design().GetIoPinsRef()[0].GetName();

  Dali dali(p_phy_db, severity::info, "");
  dali.InstantiateIoPlacer();

  // Constrain the chosen pin to the left edge.
  char c0[] = "place-io", c1[] = "-cons";
  std::vector<std::string> a = {pin_name, "left"};
  std::vector<char*> argv = {c0, c1};
  for (auto& s : a) argv.push_back(&s[0]);
  if (!dali.IoPinPlacement(static_cast<int>(argv.size()), argv.data())) {
    return FAIL;
  }

  if (!dali.SetIoPlacerGlobalMetalLayer("Metal1")) return FAIL;
  if (!dali.RunIoPinAutoPlacement()) return FAIL;

  dali.ExportToPhyDB();
  std::string out_def = "constrain_io_pin_to_edge.def";
  p_phy_db->WriteDef(out_def);
  delete p_phy_db;

  p_phy_db = new phydb::PhyDB;
  p_phy_db->ReadLef(lef_file_name);
  p_phy_db->ReadDef(out_def);

  // The constrained pin sits on the left boundary.
  bool constrained_ok = false;
  int left = p_phy_db->design().GetDieArea().LLX();
  for (auto& iopin : p_phy_db->design().GetIoPinsRef()) {
    if (iopin.GetName() != pin_name) continue;
    constrained_ok = (iopin.GetLocation().x == left);
    break;
  }

  bool is_legal = constrained_ok &&
                  IsEveryIoPinPlacedOnBoundary(p_phy_db) &&
                  IsNoIoPinOverlapAndSpacingViolation(p_phy_db);
  delete p_phy_db;
  return is_legal ? SUCCESS : FAIL;
}
