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
 * Testcase for grouped I/O placement: `place-io -group`.
 *
 * A group is fixed as a contiguous run along one edge. The test groups four
 * pins on the left edge, area-array-places the remaining pins in the interior,
 * and checks the group order, fixed status, adjacency, and final legality.
 */

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "dali/common/phydb_helper.h"
#include "dali/dali.h"
#include "helper.h"

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

  auto& pins = p_phy_db->design().GetIoPinsRef();
  std::vector<std::string> group = {pins[0].GetName(), pins[1].GetName(),
                                    pins[2].GetName(), pins[3].GetName()};

  Dali dali(p_phy_db, severity::info, "");
  dali.InstantiateIoPlacer();

  char c0[] = "place-io", c1[] = "-group";
  std::vector<std::string> a = {"Metal1", "left"};
  for (auto& g : group) a.push_back(g);
  std::vector<char*> argv = {c0, c1};
  for (auto& s : a) argv.push_back(&s[0]);
  if (!dali.IoPinPlacement(static_cast<int>(argv.size()), argv.data())) {
    return FAIL;
  }

  // Place the remaining (non-group) pins on an interior area-array grid, so
  // they do not compete for the group's edge. The fixed group is left alone.
  size_t num_pins = p_phy_db->design().GetIoPinsRef().size();
  int side = static_cast<int>(std::ceil(std::sqrt((double)num_pins)));
  std::string sside = std::to_string(side);
  {
    char b0[] = "place-io", b1[] = "-area";
    std::vector<std::string> b = {"Metal1", sside, sside};
    std::vector<char*> bargv = {b0, b1};
    for (auto& s : b) bargv.push_back(&s[0]);
    if (!dali.IoPinPlacement(static_cast<int>(bargv.size()), bargv.data())) {
      return FAIL;
    }
  }
  dali.ExportToPhyDB();
  std::string out_def = "group_io_pins.def";
  p_phy_db->WriteDef(out_def);
  delete p_phy_db;

  p_phy_db = new phydb::PhyDB;
  p_phy_db->ReadLef(lef_file_name);
  p_phy_db->ReadDef(out_def);

  int left = p_phy_db->design().GetDieArea().LLX();
  auto is_group = [&](std::string const& name) {
    return std::find(group.begin(), group.end(), name) != group.end();
  };

  // Group members retain command order at one uniform pitch on the left edge.
  bool group_ok = true;
  std::vector<int> group_y;
  for (auto const& name : group) {
    phydb::IOPin* iopin = p_phy_db->GetIoPinPtr(name);
    if (iopin == nullptr) {
      group_ok = false;
      continue;
    }
    group_ok = group_ok && iopin->GetLocation().x == left &&
               iopin->GetPlacementStatus() == phydb::PlaceStatus::FIXED &&
               iopin->GetLayerName() == "Metal1" &&
               OrientPhyDB2Dali(iopin->GetOrientation()) == E;
    group_y.push_back(iopin->GetLocation().y);
  }
  if (group_y.size() != group.size()) {
    delete p_phy_db;
    return FAIL;
  }
  int group_pitch = group_y.size() > 1 ? group_y[1] - group_y[0] : 0;
  group_ok = group_ok && group_pitch > 0;
  for (size_t i = 1; i < group_y.size(); ++i) {
    group_ok = group_ok && group_y[i] - group_y[i - 1] == group_pitch;
  }
  if (!group_ok) {
    LOG(info) << "Grouped I/O pin order or attributes do not match:\n";
    for (size_t i = 0; i < group.size(); ++i) {
      LOG(info) << "  " << group[i] << " at y=" << group_y[i] << "\n";
    }
  }

  // No non-group pin also on the left edge falls within the group's y span:
  // the group is a contiguous run.
  bool adjacent = true;
  int group_lo_y = *std::min_element(group_y.begin(), group_y.end());
  int group_hi_y = *std::max_element(group_y.begin(), group_y.end());
  for (auto& iopin : p_phy_db->design().GetIoPinsRef()) {
    if (is_group(iopin.GetName())) continue;
    if (iopin.GetLocation().x != left) continue;
    int y = iopin.GetLocation().y;
    if (y > group_lo_y && y < group_hi_y) adjacent = false;
  }

  bool is_legal = group_ok && adjacent && IsEveryIoPinPlaced(p_phy_db) &&
                  IsNoIoPinOverlapAndSpacingViolation(p_phy_db) &&
                  IsEveryIoPinOnMetal(p_phy_db, "Metal1") &&
                  IsEveryIoPinInsideDieArea(p_phy_db) &&
                  IsEveryIoPinManufacturable(p_phy_db);
  delete p_phy_db;
  return is_legal ? SUCCESS : FAIL;
}
