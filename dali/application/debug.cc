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

#include <unistd.h>

#include <ctime>
#include <iostream>
#include <vector>

#include "dali/dali.h"

#define TEST_LG 0

using namespace dali;

int main() {
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  std::string lef_file_name = "processor.lef";
  std::string def_file_name = "processor.def";
  std::string cell_file_name = "processor.cell";

  // read LEF/DEF/CELL
  phydb::PhyDB phy_db;
  // phy_db.SetPlacementGrids(0.01, 0.01);
  phy_db.ReadLef(lef_file_name);
  phy_db.ReadDef(def_file_name);
  phy_db.ReadCell(cell_file_name);

  // initialize Dali
  Dali dali(&phy_db, boost::log::trivial::info);

  // phydb::Macro *cell = phy_db.GetMacroPtr("WELLTAPX1");
  // dali.AddWellTaps(cell, 60, true);
  dali.StartPlacement(0.65);
  // dali.GlobalPlace(1.00);
  // dali.ExternalDetailedPlaceAndLegalize("innovus");
  // dali.SimpleIoPinPlacement("m1");
  // dali.ExportToDEF(def_file_name);

  // for testing dali APIs for interact
  // dali.InstantiateIoPlacer();
  /*int count = 11;
  char *arguments[11] = {
      "place-io",
      "--auto-place",
      "--metal",
      "left",
      "m1",
      "right",
      "m1",
      "bottom",
      "m1",
      "top",
      "m1"
  };*/

  int count = 2;
  char *arguments[2] = {(char *)"place-io", (char *)"m1"};

  dali.IoPinPlacement(count, arguments);
  // dali.AutoIoPinPlacement();
  dali.ExportToPhyDB();

  int place_count = 12;
  char *place_argvs[12] = {
      (char *)"place-io", (char *)"--place", (char *)"go",     (char *)"m2",
      (char *)"-211",     (char *)"1",       (char *)"211",    (char *)"101",
      (char *)"FIXED",    (char *)"362401",  (char *)"185401", (char *)"E"};
  dali.IoPinPlacement(place_count, place_argvs);

  phy_db.WriteDef("ioplace_default_1.def");
  std::string pp_name("circuitppnp1.rect");
  phy_db.SavePpNpToRectFile(pp_name);
  std::string well_name("circuitwell1.rect");
  phy_db.SaveWellToRectFile(well_name);

  elapsed_time.RecordEndTime();
  BOOST_LOG_TRIVIAL(info) << "Execution time " << elapsed_time.GetWallTime()
                          << "s.\n";
  dali.Close();

  return 0;
}
