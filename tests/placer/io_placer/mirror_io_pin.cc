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
 * Testcase for mirrored I/O placement: `place-io -mirror`.
 *
 * One pair is mirrored by x coordinate from left to right; another interior
 * pair is mirrored by y coordinate across the declared DEF die center. The
 * remaining pins are automatically placed, proving boundary mirrors reserve
 * resources and off-grid die extents do not distort interior symmetry.
 */

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

  auto die = p_phy_db->design().GetDieArea();
  int units = p_phy_db->design().GetUnitsDistanceMicrons();
  auto& pins = p_phy_db->design().GetIoPinsRef();
  std::string x_ref_pin = pins[0].GetName();
  std::string x_mirror_pin = pins[1].GetName();
  std::string y_ref_pin = pins[2].GetName();
  std::string y_mirror_pin = pins[3].GetName();

  // One reference is on the left edge; the other is interior. The benchmark's
  // top edge is off Dali's placement grid, making the interior pair sensitive
  // to whether mirroring uses the declared die or the shrunken grid region.
  double lx_um = (double)die.LLX() / units;
  double x_ref_y_um = (die.LLY() + 0.25 * (die.URY() - die.LLY())) / units;
  double y_ref_x_um = (die.LLX() + 0.35 * (die.URX() - die.LLX())) / units;
  double y_ref_y_um = (die.LLY() + 0.30 * (die.URY() - die.LLY())) / units;

  Dali dali(p_phy_db, severity::info, "");
  dali.InstantiateIoPlacer();

  std::string sx = std::to_string(lx_um);
  std::string sy = std::to_string(x_ref_y_um);
  {
    char c0[] = "place-io", c1[] = "--place";
    std::vector<std::string> a = {x_ref_pin, "Metal1", "-0.05", "0", "0.05",
                                  "0.1",     sx,       sy,      "E"};
    std::vector<char*> argv = {c0, c1};
    for (auto& s : a) argv.push_back(&s[0]);
    if (!dali.IoPinPlacement(static_cast<int>(argv.size()), argv.data())) {
      return FAIL;
    }
  }

  // Reflect the second pin's x coordinate across the vertical die centerline.
  {
    char c0[] = "place-io", c1[] = "-mirror";
    std::vector<std::string> a = {x_mirror_pin, x_ref_pin, "x"};
    std::vector<char*> argv = {c0, c1};
    for (auto& s : a) argv.push_back(&s[0]);
    if (!dali.IoPinPlacement(static_cast<int>(argv.size()), argv.data())) {
      return FAIL;
    }
  }

  std::string bx = std::to_string(y_ref_x_um);
  std::string by = std::to_string(y_ref_y_um);
  {
    char c0[] = "place-io", c1[] = "--place";
    std::vector<std::string> a = {y_ref_pin, "Metal1", "-0.05", "0", "0.05",
                                  "0.1",     bx,       by,      "N"};
    std::vector<char*> argv = {c0, c1};
    for (auto& s : a) argv.push_back(&s[0]);
    if (!dali.IoPinPlacement(static_cast<int>(argv.size()), argv.data())) {
      return FAIL;
    }
  }

  {
    char c0[] = "place-io", c1[] = "-mirror";
    std::vector<std::string> a = {y_mirror_pin, y_ref_pin, "y"};
    std::vector<char*> argv = {c0, c1};
    for (auto& s : a) argv.push_back(&s[0]);
    if (!dali.IoPinPlacement(static_cast<int>(argv.size()), argv.data())) {
      return FAIL;
    }
  }

  if (!dali.SetIoPlacerGlobalMetalLayer("Metal1")) return FAIL;
  if (!dali.RunIoPinAutoPlacement()) return FAIL;

  dali.ExportToPhyDB();
  std::string out_def = "mirror_io_pin.def";
  p_phy_db->WriteDef(out_def);
  delete p_phy_db;

  p_phy_db = new phydb::PhyDB;
  p_phy_db->ReadLef(lef_file_name);
  p_phy_db->ReadDef(out_def);

  auto d = p_phy_db->design().GetDieArea();
  phydb::IOPin* x_ref = p_phy_db->GetIoPinPtr(x_ref_pin);
  phydb::IOPin* x_mirror = p_phy_db->GetIoPinPtr(x_mirror_pin);
  phydb::IOPin* y_ref = p_phy_db->GetIoPinPtr(y_ref_pin);
  phydb::IOPin* y_mirror = p_phy_db->GetIoPinPtr(y_mirror_pin);
  bool mirrored_ok =
      x_ref != nullptr && x_mirror != nullptr && y_ref != nullptr &&
      y_mirror != nullptr && x_ref->GetLocation().x == d.LLX() &&
      x_mirror->GetLocation().x == d.URX() &&
      x_ref->GetLocation().y == x_mirror->GetLocation().y &&
      OrientPhyDB2Dali(x_ref->GetOrientation()) == E &&
      OrientPhyDB2Dali(x_mirror->GetOrientation()) == FE &&
      y_ref->GetLocation().x == y_mirror->GetLocation().x &&
      y_ref->GetLocation().y + y_mirror->GetLocation().y == d.LLY() + d.URY() &&
      OrientPhyDB2Dali(y_ref->GetOrientation()) == N &&
      OrientPhyDB2Dali(y_mirror->GetOrientation()) == FS;

  bool remaining_on_boundary = true;
  for (auto& iopin : p_phy_db->design().GetIoPinsRef()) {
    if (iopin.GetName() == y_ref_pin || iopin.GetName() == y_mirror_pin) {
      continue;
    }
    auto location = iopin.GetLocation();
    bool on_boundary = location.x == d.LLX() || location.x == d.URX() ||
                       location.y == d.LLY() || location.y == d.URY();
    remaining_on_boundary = remaining_on_boundary && on_boundary;
  }

  bool is_legal = mirrored_ok && IsEveryIoPinPlaced(p_phy_db) &&
                  remaining_on_boundary &&
                  IsNoIoPinOverlapAndSpacingViolation(p_phy_db) &&
                  IsEveryIoPinOnMetal(p_phy_db, "Metal1") &&
                  IsEveryIoPinInsideDieArea(p_phy_db) &&
                  IsEveryIoPinManufacturable(p_phy_db);
  delete p_phy_db;
  return is_legal ? SUCCESS : FAIL;
}
