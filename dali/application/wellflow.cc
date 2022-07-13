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
#include <ctime>

#include <iostream>

#include "dali/circuit/circuit.h"
#include "dali/common/logging.h"
#include "dali/placer.h"

using namespace dali;

int main(int argc, char *argv[]) {
  InitLogging();
  int num_of_thread = 1;
  omp_set_num_threads(num_of_thread);

  time_t Time = clock();

  std::string lef_file_name = "ICCAD2020/out.lef";
  std::string def_file_name = "ICCAD2020/out.def";
  std::string cel_file_name = "ICCAD2020/out.cell";
  std::string out_file_name = "circuit";

  // load LEF/DEF/CELL
  phydb::PhyDB phy_db;
  phy_db.ReadLef(lef_file_name);
  phy_db.ReadDef(def_file_name);
  phy_db.ReadCell(cel_file_name);
  Circuit circuit;
  circuit.InitializeFromPhyDB(&phy_db);

  BOOST_LOG_TRIVIAL(info) << "File loading complete, time: "
                          << double(clock() - Time) / CLOCKS_PER_SEC << " s\n";
  BOOST_LOG_TRIVIAL(info) << "  Average white space utility: "
                          << circuit.WhiteSpaceUsage() << "\n";
  circuit.ReportBriefSummary();
  //circuit.ReportBlockType();
  circuit.ReportHPWL();

  dali::WellPlaceFlow well_place_flow;
  well_place_flow.SetInputCircuit(&circuit);
  well_place_flow.SetBoundaryFromCircuit();
  well_place_flow.SetPlacementDensity(0.65);
  well_place_flow.ReportBoundaries();
  well_place_flow.StartPlacement();
  circuit.GenMATLABTable("gb_result.txt");

  well_place_flow.EmitDEFWellFile(out_file_name, 1);
  circuit.SaveDefFile(out_file_name, "", def_file_name, 1, 1, 2, 1);

  Time = clock() - Time;
  BOOST_LOG_TRIVIAL(info) << "Execution time "
                          << double(Time) / CLOCKS_PER_SEC << "s.\n";

  return 0;
}
