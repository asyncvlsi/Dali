/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/

/**
 * @file
 * Exercise the script-facing manual I/O signoff workflow.
 *
 * This starts from an already placed design so it also verifies that Dali
 * imports existing pin geometry and orientation before users inspect or edit
 * them.
 */

#include <string>

#include "dali/common/phydb_helper.h"
#include "dali/dali.h"
#include "helper.h"

using namespace dali;

int main() {
  phydb::PhyDB phy_db;
  phy_db.ReadLef("ispd19_test3.input.lef");
  phy_db.ReadDef("ispd19_test3.input.def");

  auto& phydb_pins = phy_db.design().GetIoPinsRef();
  if (phydb_pins.size() < 2) {
    return 1;
  }
  phydb::IOPin& first_phydb_pin = phydb_pins[0];
  phydb::IOPin& second_phydb_pin = phydb_pins[1];
  const std::string first_name = first_phydb_pin.GetName();
  const std::string second_name = second_phydb_pin.GetName();
  const std::string metal_name = second_phydb_pin.GetLayerName();
  const double units = phy_db.design().GetUnitsDistanceMicrons();
  const auto second_location = second_phydb_pin.GetLocation();
  const std::string second_x = std::to_string(second_location.x / units);
  const std::string second_y = std::to_string(second_location.y / units);
  const std::string second_orientation =
      OrientStr(OrientPhyDB2Dali(second_phydb_pin.GetOrientation()));

  Dali dali(&phy_db, severity::info);
  if (!dali.ExecuteCommand({"show-io", first_name}) ||
      !dali.ExecuteCommand({"check-io"})) {
    dali.Close();
    return 1;
  }

  const auto first_location = first_phydb_pin.GetLocation();
  const std::string first_x = std::to_string(first_location.x / units);
  const std::string first_y = std::to_string(first_location.y / units);
  const std::string first_orientation =
      OrientStr(OrientPhyDB2Dali(first_phydb_pin.GetOrientation()));
  if (!dali.ExecuteCommand(
          {"move-io", second_name, first_x, first_y, first_orientation}) ||
      dali.ExecuteCommand({"check-io"})) {
    dali.Close();
    return 1;
  }

  if (!dali.ExecuteCommand(
          {"move-io", second_name, second_x, second_y, second_orientation}) ||
      !dali.ExecuteCommand({"check-io"}) ||
      !dali.ExecuteCommand({"unfix-io", second_name}) ||
      dali.ExecuteCommand({"check-io"})) {
    dali.Close();
    return 1;
  }

  if (!dali.ExecuteCommand({"place-io", "-config", metal_name}) ||
      !dali.ExecuteCommand({"place-io", "-auto-place"}) ||
      !dali.ExecuteCommand({"check-io"})) {
    dali.Close();
    return 1;
  }

  dali.ExportToPhyDB();
  phydb::IOPin* exported_pin = phy_db.GetIoPinPtr(second_name);
  bool exported_as_placed =
      exported_pin->GetPlacementStatus() == phydb::PlaceStatus::PLACED ||
      exported_pin->GetPlacementStatus() == phydb::PlaceStatus::FIXED;
  dali.Close();
  return exported_as_placed ? 0 : 1;
}
