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
 * Exercise pre-placement I/O commands through a `.dali` recipe.
 *
 * The manually fixed pin and the edge constraint are issued before
 * `run placement`. This catches either reloading the circuit at placement
 * startup or replacing the configured IoPlacer at the automatic I/O stage.
 */

#include <cstdio>
#include <fstream>
#include <string>

#include "dali/dali.h"
#include "helper.h"

using namespace dali;

int main() {
  const std::string lef_file_name = "ispd19_test3.input.lef";
  const std::string def_file_name = "ispd19_test3.input.def";
  const std::string script_file_name = "io_command_file_flow.dali";

  phydb::PhyDB phy_db;
  phy_db.ReadLef(lef_file_name);
  phy_db.ReadDef(def_file_name);
  SetAllIoPinsToUnplaced(&phy_db);

  auto& io_pins = phy_db.design().GetIoPinsRef();
  if (io_pins.size() < 2) {
    return 1;
  }
  const std::string fixed_pin_name = io_pins[0].GetName();
  const std::string constrained_pin_name = io_pins[1].GetName();
  const auto die = phy_db.design().GetDieArea();
  const int units = phy_db.design().GetUnitsDistanceMicrons();
  const double center_x = 0.5 * (die.LLX() + die.URX()) / units;
  const double center_y = 0.5 * (die.LLY() + die.URY()) / units;

  {
    std::ofstream script(script_file_name);
    script << "# Pre-placement I/O edits must survive run placement.\n"
           << "set target_density 0.7\n"
           << "set disable_global_place true\n"
           << "set disable_legalization true\n"
           << "set disable_detailed_place true\n"
           << "place-io -place " << fixed_pin_name << " Metal1 0 0 0.1 2.0 "
           << center_x << " " << center_y << " N\n"
           << "place-io -constraint " << constrained_pin_name << " top\n"
           << "run placement\n";
  }

  Dali dali(&phy_db, severity::info);
  const bool command_success = dali.RunCommandFile(script_file_name);
  std::remove(script_file_name.c_str());
  if (!command_success) {
    dali.Close();
    return 1;
  }
  dali.ExportToPhyDB();

  bool fixed_pin_preserved = false;
  bool constrained_pin_on_top = false;
  for (auto& io_pin : phy_db.design().GetIoPinsRef()) {
    if (io_pin.GetName() == fixed_pin_name) {
      const auto location = io_pin.GetLocation();
      fixed_pin_preserved =
          io_pin.GetPlacementStatus() == phydb::PlaceStatus::FIXED &&
          location.x != die.LLX() && location.x != die.URX() &&
          location.y != die.LLY() && location.y != die.URY();
    }
    if (io_pin.GetName() == constrained_pin_name) {
      constrained_pin_on_top =
          io_pin.GetPlacementStatus() != phydb::PlaceStatus::UNPLACED &&
          io_pin.GetLocation().y == die.URY();
    }
  }

  const bool all_pins_placed = IsEveryIoPinPlaced(&phy_db);
  dali.Close();
  return fixed_pin_preserved && constrained_pin_on_top && all_pins_placed ? 0
                                                                          : 1;
}
