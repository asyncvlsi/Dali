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
 * @brief Testcase for IoPlacement command "place-io --add" and "place-io --place".
 *
 * This is a simple testcase to show that using command
 *     "add-io <pin_name> <net_name> <direction> <use>"
 *     "place-io <pin_name> <metal_name> <lx> <ly> <ux> <uy> <placement_status> <x> <y> <orientation>"
 * we can obtain a placement result satisfying:
 * 1. all I/O pins have final locations, shapes, and orientation as we set.
 *
 * @return 0 if this test is passed, 1 if failed
 */
int main() {
  std::string lef_file_name = "ispd19_test3.input.lef";
  std::string def_file_name = "ispd19_test3.input.def";

  // initialize PhyDB
  auto *p_phy_db0 = new phydb::PhyDB;
  p_phy_db0->ReadLef(lef_file_name);
  p_phy_db0->ReadDef(def_file_name);

  // remove all iopins from a PhyDB instance
  RemoveAllIoPins(p_phy_db0);

  // initialize Dali to use its API to add and place I/O pins
  Dali dali(p_phy_db0, boost::log::trivial::debug, "", true);
  dali.InstantiateIoPlacer();

  // read the DEF file back to the memory, and perform checking
  auto *p_phy_db1 = new phydb::PhyDB;
  p_phy_db1->ReadLef(lef_file_name);
  p_phy_db1->ReadDef(def_file_name);

  // add all I/O pins to the first PhyDB instance
  for (auto &iopin: p_phy_db1->design().GetIoPinsRef()) {
    auto *p_iopin = p_phy_db0->AddIoPin(
        iopin.GetName(),
        iopin.GetDirection(),
        iopin.GetUse()
    );
    p_iopin->SetNetId(iopin.GetNetId());
    p_iopin->SetShape(
        iopin.GetLayerName(),
        iopin.GetRect().LLX(),
        iopin.GetRect().LLY(),
        iopin.GetRect().URX(),
        iopin.GetRect().URY()
    );
    p_iopin->SetPlacement(
        iopin.GetPlacementStatus(),
        iopin.GetLocation().x,
        iopin.GetLocation().y,
        iopin.GetOrientation()
    );
  }

  p_phy_db0->WriteDef("add_and_place_io_pins.def");

  bool is_legal = IsEveryIoPinAddedAndPlacedCorrectly(p_phy_db0, p_phy_db1);

  delete p_phy_db0;
  delete p_phy_db1;

  if (!is_legal) return FAIL;

  return SUCCESS;
}
