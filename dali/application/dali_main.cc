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
#include <unistd.h>

#include <chrono>
#include <ctime>
#include <iostream>
#include <memory>

#include "dali/application/dali_command_line.h"
#include "dali/common/elapsed_time.h"
#include "dali/common/git_version.h"
#include "dali/common/helper.h"
#include "dali/common/logging.h"
#include "dali/common/placement_metrics.h"
#include "dali/dali.h"
#ifdef DALI_HAS_QT_GUI
#include "dali/gui/qt_placement_snapshot_sink.h"
#endif

using namespace dali;

/** Print the application banner shown at startup. */
static void PrintSoftwareStatement() {
  std::cout
      // clang-format off
      << "\n"
      << "  +----------------------------------------------+\n"
      << "  |                                              |\n"
      << "  |     Dali: a gridded cell placer              |\n"
      << "  |                                              |\n"
      << "  |     Department of Electrical Engineering     |\n"
      << "  |     Yale University                          |\n"
      << "  |                                              |\n"
      << "  |     Developed by                             |\n"
      << "  |     Yihang Yang, Rajit Manohar               |\n"
      << "  |                                              |\n"
      << "  |     This program is for academic use and     |\n"
      << "  |     testing only                             |\n"
      << "  |     THERE IS NO WARRANTY                     |\n"
      << "  |                                              |\n"
      << "  |     build time: " << __DATE__ << " " << __TIME__ << "         |\n"
      << "  |     commit: " << get_git_version_short() << "                     |\n"
      << "  |                                              |\n"
      << "  +----------------------------------------------+\n\n";
  // clang-format on
}

/** Load LEF/DEF/CELL inputs into PhyDB using parsed command-line options. */
static void InitializePhyDb(const DaliCommandLineOptions& options,
                            phydb::PhyDB* phy_db) {
  if (options.x_grid > 0 && options.y_grid > 0) {
    phy_db->SetPlacementGrids(options.x_grid, options.y_grid);
  }
  phy_db->ReadLef(options.lef_file_name);
  phy_db->ReadDef(options.def_file_name);
  if (!options.cell_file_name.empty()) {
    phy_db->ReadCell(options.cell_file_name);
  }
}

int main(int argc, char* argv[]) {
  PrintSoftwareStatement();

  DaliCommandLineOptions options;
  if (!ParseDaliCommandLine(argc, argv, &options, std::cout)) {
    ReportDaliUsage(std::cout);
    return 1;
  }

  // start the timer to record the runtime
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  // Load the physical design database before handing control to the
  // placement flow facade.
  phydb::PhyDB phy_db;
  InitializePhyDb(options, &phy_db);

  Dali dali(&phy_db, options.verbose_level, options.log_file_name);
#ifdef DALI_HAS_QT_GUI
  dali.SetGuiSnapshotSinkFactory(
      [] { return std::make_unique<QtPlacementSnapshotSink>(); });
#endif
  dali.SetInteractiveSessionExpected(options.interactive);

  // print the current time
  using std::chrono::system_clock;
  system_clock::time_point today = system_clock::now();
  std::time_t tt = system_clock::to_time_t(today);
  char time_buffer[64];
  std::tm* local_time = std::localtime(&tt);
  if (local_time != nullptr &&
      std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S %Z",
                    local_time) > 0) {
    LOG(info) << "Run started at: " << time_buffer << "\n";
  } else {
    LOG(info) << "Run started at: " << tt << "\n";
  }

  // save command line arguments for future reference
  SaveArgs(argc, argv);

  // Preserve the original implicit placement flow. Recipes own stage
  // execution, while interactive mode initializes the design and waits for
  // commands without moving components first.
  bool is_success = true;
  if (!options.command_file_name.empty()) {
    is_success = dali.RunCommandFile(options.command_file_name);
  } else if (!options.interactive) {
    is_success = dali.StartPlacement();
  }
  if (is_success && options.interactive) {
    is_success = dali.RunInteractiveSession(std::cin, std::cout,
                                            isatty(STDIN_FILENO) != 0);
  }
  if (!is_success) {
    WritePlacementMetricsJson(options.metrics_file_name, false);
    return 1;
  }

  if (!options.net_hpwl_file_name.empty()) {
    dali.GetCircuit().WriteNetHpwlTable(options.net_hpwl_file_name);
  }

  // Export both Dali's textual outputs and the updated in-memory PhyDB view.
  dali.MaybeExportToLEF(options.lef_file_name, options.output_name);
  dali.ExportToDEF(options.def_file_name, options.output_name);
  dali.ExportToPhyDB();
  phy_db.WriteDef("phydb.def");

  elapsed_time.RecordEndTime();
  LOG(info) << "****End of placement "
            << "(wall time: " << elapsed_time.GetWallTime() << "s, "
            << "cpu time: " << elapsed_time.GetCpuTime() << "s)****\n";
  WritePlacementMetricsJson(options.metrics_file_name, true);

  return 0;
}
