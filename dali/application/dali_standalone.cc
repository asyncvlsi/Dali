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

#include <chrono>
#include <iostream>
#include <memory>

#include <common/config.h>

#include <phydb/phydb.h>

#include "dali/common/elapsed_time.h"
#include "dali/common/helper.h"
#include "dali/common/logging.h"
#include "dali/dali.h"

using namespace dali;

void ReportUsage() {
  std::cout
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
      << "  -v                                         verbosity_level (optional, 0-5, default 1)\n"
      << "  -disable_log_prefix                        optional, if this flag is present, then only messages will be saved to the log file\n"
      << "(flag order does not matter)"
      << "\033[0m\n";
}

void PrintSoftwareStatement() {
  std::cout
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
      << "  |                                              |\n"
      << "  +----------------------------------------------+\n\n";
}

int main(int argc, char *argv[]) {
  // parameters that can be configured via the command line
  std::string lef_file_name;
  std::string def_file_name;
  std::string cell_file_name;
  std::string m_cell_file_name;
  std::string output_name = "dali_out";
  std::string log_file_name;
  severity verbose_level = boost::log::trivial::info;
  double x_grid = 0, y_grid = 0;

  // parsing arguments
  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-lef" && i < argc) {
      lef_file_name = std::string(argv[i++]);
      if (lef_file_name.empty()) {
        std::cout << "Invalid input lef file!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-def" && i < argc) {
      def_file_name = std::string(argv[i++]);
      if (def_file_name.empty()) {
        std::cout << "Invalid input def file!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-cell" && i < argc) {
      cell_file_name = std::string(argv[i++]);
      if (cell_file_name.empty()) {
        std::cout << "Invalid input cell file!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-mcell" && i < argc) {
      m_cell_file_name = std::string(argv[i++]);
      if (m_cell_file_name.empty()) {
        std::cout << "Invalid input mcell file!\n";
        ReportUsage();
        return 1;
      }
    } else if ((arg == "-o" || arg == "-output_name") && i < argc) {
      output_name = std::string(argv[i++]);
      if (output_name.empty()) {
        std::cout << "Invalid output name!\n";
        ReportUsage();
        return 1;
      }
      config_set_string("dali.output_name", output_name.c_str());
    } else if (arg == "-v" && i < argc) {
      std::string str_verbose_level = std::string(argv[i++]);
      verbose_level = StrToLoggingLevel(str_verbose_level);
    } else if ((arg == "-g" || arg == "-grid") && i < argc) {
      std::string str_x_grid = std::string(argv[i++]);
      std::string str_y_grid = std::string(argv[i++]);
      try {
        x_grid = std::stod(str_x_grid);
        y_grid = std::stod(str_y_grid);
      } catch (...) {
        std::cout << "Invalid input files!\n";
        ReportUsage();
        return 1;
      }
    } else if ((arg == "-d" || arg == "-target_density") && i < argc) {
      std::string str_target_density = std::string(argv[i++]);
      try {
        double target_density = std::stod(str_target_density);
        config_set_real("dali.target_density", target_density);
      } catch (...) {
        std::cout << "Invalid target density!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "io_metal_layer" && i < argc) {
      std::string str_metal_layer_num = std::string(argv[i++]);
      try {
        int io_metal_layer = std::stoi(str_metal_layer_num) - 1;
        config_set_int("dali.io_metal_layer", io_metal_layer);
      } catch (...) {
        std::cout << "Invalid metal layer number!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-disable_log_prefix") {
      config_set_int("dali.disable_log_prefix", 1);
    } else if (arg == "-well_legalization_mode" && i < argc) {
      std::string str_wlg_mode = std::string(argv[i++]);
      config_set_string("dali.well_legalization_mode", str_wlg_mode.c_str());
    } else if (arg == "-disable_legalization") {
      config_set_int("dali.disable_legalization", 1);
    } else if (arg == "-disable_global_place") {
      config_set_int("dali.disable_global_place", 1);
    } else if (arg == "-max_row_width" && i < argc) {
      try {
        double max_row_width = std::stod(argv[i++]);
        config_set_real("dali.max_row_width", max_row_width);
      } catch (...) {
        std::cout << "Invalid max row width!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-disable_welltap") {
      config_set_int("dali.disable_welltap", 1);
    } else if (arg == "-disable_io_place") {
      config_set_int("dali.disable_io_place", 1);
    } else if (arg == "-export_well_cluster_matlab") {
      config_set_int("dali.export_well_cluster_matlab", 1);
    } else if (arg == "-log_file_name" && i < argc) {
      log_file_name = std::string(argv[i++]);
      config_set_string("dali.log_file_name", log_file_name.c_str());
      if (log_file_name.empty()) {
        std::cout << "Invalid name for log file!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-num_threads" && i < argc) {
      std::string str_num_threads = std::string(argv[i++]);
      try {
        int num_threads = std::stoi(str_num_threads);
        config_set_int("dali.num_threads", num_threads);
      } catch (...) {
        std::cout << "Invalid number of threads!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-is_standard_cell") {
      config_set_int("dali.is_standard_cell", 1);
    } else if (arg == "-enable_filler_cell") {
      config_set_int("dali.enable_filler_cell", 1);
    }else {
      std::cout << "Unknown flag\n";
      std::cout << arg << "\n";
      return 1;
    }
  }

  // start the timer to record the runtime
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  // load LEF/DEF/CELL files
  // (1). initialize PhyDB
  if ((lef_file_name.empty()) || (def_file_name.empty())) {
    std::cout << "Invalid input files!\n";
    ReportUsage();
    return 1;
  }
  phydb::PhyDB phy_db;
  if (x_grid > 0 && y_grid > 0) {
    phy_db.SetPlacementGrids(x_grid, y_grid);
  }
  phy_db.ReadLef(lef_file_name);
  phy_db.ReadDef(def_file_name);
  if (!cell_file_name.empty()) {
    phy_db.ReadCell(cell_file_name);
  }

  // (2). initialize Circuit
  Dali dali(&phy_db, verbose_level, log_file_name);
  // print the software statement
  PrintSoftwareStatement();

  // print the current time
  using std::chrono::system_clock;
  system_clock::time_point today = system_clock::now();
  std::time_t tt = system_clock::to_time_t(today);
  BOOST_LOG_TRIVIAL(info) << "Today is: " << ctime(&tt) << "\n";

  // save command line arguments for future reference
  SaveArgs(argc, argv);

  dali.StartPlacement();

  // save placement result
  dali.ExportToDEF(def_file_name, output_name);
  dali.ExportToPhyDB();

  phy_db.WriteDef("phydb.def");

  elapsed_time.RecordEndTime();
  BOOST_LOG_TRIVIAL(info)
    << "****End of placement "
    << "(wall time: " << elapsed_time.GetWallTime() << "s, "
    << "cpu time: " << elapsed_time.GetCpuTime() << "s)****\n";

  return 0;
}
