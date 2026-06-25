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
// clang-format off
#include <stdio.h>
#include <common/config.h>
// clang-format on
#include <phydb/phydb.h>

#include <chrono>
#include <ctime>
#include <iostream>
#include <string>

#include "dali/common/elapsed_time.h"
#include "dali/common/git_version.h"
#include "dali/common/helper.h"
#include "dali/common/logging.h"
#include "dali/dali.h"

using namespace dali;

namespace {

/** Parsed command-line settings for the main `dali` application. */
struct CommandLineOptions {
  std::string lef_file_name;
  std::string def_file_name;
  std::string cell_file_name;
  std::string ignored_mcell_file_name;
  std::string output_name = "dali_out";
  std::string log_file_name;
  severity verbose_level = severity::info;
  double x_grid = 0;
  double y_grid = 0;
};

/** Print command-line usage for the main placement application. */
void ReportUsage() {
  std::cout
      // clang-format off
      << "\033[0;36m"
      << "Usage: dali\n"
      << "  -lef <file.lef>\n"
      << "  -def <file.def>\n"
      << "  -cell <file.cell>                          (optional, if provided, well placement flow will be triggered)\n"
      << "  -o/-output_name <output_name>.def          (optional, default output def file name dali_out.def)\n"
      << "  -g/-grid <grid_value_x> <grid_value_y>     (optional, default metal1 and metal2 pitch values)\n"
      << "  -d/-target_density <density>               (optional, value interval (0,1], default max(space_utility, 0.7))\n"
      << "  -disable_legalization                      optional, if this flag is present, then legalization is skipped\n"
      << "  -io_metal_layer                            metal layer number for I/O placement (optional, default 1 for m1)\n"
      << "  -well_legalization_mode <scavenge/strict>  determine whether the last column use unassigned space\n"
      << "  -num_threads <n>                           number of OpenMP threads to use\n"
      << "  -v                                         verbosity_level (optional, 0-5, default 1)\n"
      << "  -disable_log_prefix                        optional, if this flag is present, then only messages will be saved to the log file\n"
      << "(flag order does not matter)"
      << "\033[0m\n";
  // clang-format on
}

/** Print the application banner shown at startup. */
void PrintSoftwareStatement() {
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

/** Fetch the next positional value for an option. */
bool TryGetValue(int argc, char* argv[], int* index, std::string* value) {
  if (*index >= argc) {
    return false;
  }
  *value = argv[(*index)++];
  return !value->empty();
}

/** Parse a floating-point option value. */
bool TryParseDouble(const std::string& text, double* value) {
  try {
    *value = std::stod(text);
    return true;
  } catch (...) {
    return false;
  }
}

/** Parse an integer option value. */
bool TryParseInt(const std::string& text, int* value) {
  try {
    *value = std::stoi(text);
    return true;
  } catch (...) {
    return false;
  }
}

/** Enable a boolean flag in the ACT config database. */
void EnableConfigFlag(const char* config_name) {
  config_set_int(config_name, 1);
}

/** Load LEF/DEF/CELL inputs into PhyDB using parsed command-line options. */
void InitializePhyDb(const CommandLineOptions& options, phydb::PhyDB* phy_db) {
  if (options.x_grid > 0 && options.y_grid > 0) {
    phy_db->SetPlacementGrids(options.x_grid, options.y_grid);
  }
  phy_db->ReadLef(options.lef_file_name);
  phy_db->ReadDef(options.def_file_name);
  if (!options.cell_file_name.empty()) {
    phy_db->ReadCell(options.cell_file_name);
  }
}

/** Parse all supported command-line options and populate the config database.
 */
bool ParseCommandLine(int argc, char* argv[], CommandLineOptions* options) {
  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    std::string value;

    if (arg == "-lef") {
      if (!TryGetValue(argc, argv, &i, &options->lef_file_name)) {
        std::cout << "Invalid input lef file!\n";
        return false;
      }
    } else if (arg == "-def") {
      if (!TryGetValue(argc, argv, &i, &options->def_file_name)) {
        std::cout << "Invalid input def file!\n";
        return false;
      }
    } else if (arg == "-cell") {
      if (!TryGetValue(argc, argv, &i, &options->cell_file_name)) {
        std::cout << "Invalid input cell file!\n";
        return false;
      }
    } else if (arg == "-mcell") {
      if (!TryGetValue(argc, argv, &i, &options->ignored_mcell_file_name)) {
        std::cout << "Invalid input mcell file!\n";
        return false;
      }
      std::cout << "Warning: -mcell is currently accepted for compatibility "
                   "but ignored\n";
    } else if (arg == "-o" || arg == "-output_name") {
      if (!TryGetValue(argc, argv, &i, &options->output_name)) {
        std::cout << "Invalid output name!\n";
        return false;
      }
      config_set_string("dali.output_name", options->output_name.c_str());
    } else if (arg == "-v") {
      if (!TryGetValue(argc, argv, &i, &value)) {
        std::cout << "Invalid verbosity level!\n";
        return false;
      }
      options->verbose_level = StrToLoggingLevel(value);
    } else if (arg == "-g" || arg == "-grid") {
      std::string x_grid;
      std::string y_grid;
      if (!TryGetValue(argc, argv, &i, &x_grid) ||
          !TryGetValue(argc, argv, &i, &y_grid) ||
          !TryParseDouble(x_grid, &options->x_grid) ||
          !TryParseDouble(y_grid, &options->y_grid)) {
        std::cout << "Invalid placement grid!\n";
        return false;
      }
    } else if (arg == "-d" || arg == "-target_density") {
      double target_density = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseDouble(value, &target_density)) {
        std::cout << "Invalid target density!\n";
        return false;
      }
      config_set_real("dali.target_density", target_density);
    } else if (arg == "-io_metal_layer" || arg == "io_metal_layer") {
      int io_metal_layer = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseInt(value, &io_metal_layer) || io_metal_layer <= 0) {
        std::cout << "Invalid metal layer number!\n";
        return false;
      }
      config_set_int("dali.io_metal_layer", io_metal_layer - 1);
    } else if (arg == "-disable_log_prefix") {
      EnableConfigFlag("dali.disable_log_prefix");
    } else if (arg == "-well_legalization_mode") {
      if (!TryGetValue(argc, argv, &i, &value)) {
        std::cout << "Invalid well legalization mode!\n";
        return false;
      }
      config_set_string("dali.well_legalization_mode", value.c_str());
    } else if (arg == "-disable_legalization") {
      EnableConfigFlag("dali.disable_legalization");
    } else if (arg == "-disable_global_place") {
      EnableConfigFlag("dali.disable_global_place");
    } else if (arg == "-max_row_width") {
      double max_row_width = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseDouble(value, &max_row_width)) {
        std::cout << "Invalid max row width!\n";
        return false;
      }
      config_set_real("dali.max_row_width", max_row_width);
    } else if (arg == "-disable_welltap") {
      EnableConfigFlag("dali.disable_welltap");
    } else if (arg == "-disable_cell_flip") {
      EnableConfigFlag("dali.disable_cell_flip");
    } else if (arg == "-disable_io_place") {
      EnableConfigFlag("dali.disable_io_place");
    } else if (arg == "-export_well_cluster_matlab") {
      EnableConfigFlag("dali.export_well_cluster_matlab");
    } else if (arg == "-log_file_name") {
      if (!TryGetValue(argc, argv, &i, &options->log_file_name)) {
        std::cout << "Invalid name for log file!\n";
        return false;
      }
      config_set_string("dali.log_file_name", options->log_file_name.c_str());
    } else if (arg == "-num_threads") {
      int num_threads = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseInt(value, &num_threads)) {
        std::cout << "Invalid number of threads!\n";
        return false;
      }
      config_set_int("dali.num_threads", num_threads);
    } else if (arg == "-is_standard_cell") {
      EnableConfigFlag("dali.is_standard_cell");
    } else if (arg == "-enable_filler_cell") {
      EnableConfigFlag("dali.enable_filler_cell");
    } else if (arg == "-enable_end_cap_cell") {
      EnableConfigFlag("dali.enable_end_cap_cell");
    } else if (arg == "-enable_shrink_off_grid_die_area") {
      EnableConfigFlag("dali.enable_shrink_off_grid_die_area");
    } else {
      std::cout << "Unknown arg: " << arg << "\n";
      return false;
    }
  }

  if (options->lef_file_name.empty() || options->def_file_name.empty()) {
    std::cout << "Invalid input files!\n";
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char* argv[]) {
  PrintSoftwareStatement();

  CommandLineOptions options;
  if (!ParseCommandLine(argc, argv, &options)) {
    ReportUsage();
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

  // print the current time
  using std::chrono::system_clock;
  system_clock::time_point today = system_clock::now();
  std::time_t tt = system_clock::to_time_t(today);
  LOG(info) << "Today is: " << ctime(&tt) << "\n";

  // save command line arguments for future reference
  SaveArgs(argc, argv);

  bool is_success = dali.StartPlacement();
  if (!is_success) {
    return 1;
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

  return 0;
}
