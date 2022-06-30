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

#include <phydb/phydb.h>

#include "dali/circuit/circuit.h"
#include "dali/common/elapsed_time.h"
#include "dali/common/helper.h"
#include "dali/common/logging.h"

#include "dali/placer.h"

using namespace dali;

void PrintSoftwareStatement();
void ReportUsage();

int main(int argc, char *argv[]) {
  // parameters that can be configured via the command line
  std::string lef_file_name;
  std::string def_file_name;
  std::string cell_file_name;
  std::string m_cell_file_name;
  std::string output_name = "dali_out";
  std::string str_grid_value_x, str_grid_value_y;
  std::string str_target_density;
  std::string str_verbose_level;
  std::string str_x_grid;
  std::string str_y_grid;
  std::string log_file_name;
  int well_legalization_mode = static_cast<int>(DefaultPartitionMode::STRICT);
  bool is_no_global = false;
  bool is_no_legal = false;
  bool is_no_io_place = false;
  severity verbose_level = boost::log::trivial::info;
  bool has_log_prefix = true;
  double x_grid = 0, y_grid = 0;
  double target_density = -1;
  int io_metal_layer = 0;
  bool export_well_cluster_for_matlab = false;
  bool is_well_tap_needed = true;
  double max_row_width = 0;
  int lg_threads = 1;
  int gb_maxiter = 100;
  bool lg_cplex = false;
  int num_threads = 1;

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
    } else if (arg == "-o" && i < argc) {
      output_name = std::string(argv[i++]);
      if (output_name.empty()) {
        std::cout << "Invalid output name!\n";
        ReportUsage();
        return 1;
      }
    } else if ((arg == "-g" || arg == "-grid") && i < argc) {
      str_x_grid = std::string(argv[i++]);
      str_y_grid = std::string(argv[i++]);
      try {
        x_grid = std::stod(str_x_grid);
        y_grid = std::stod(str_y_grid);
      } catch (...) {
        std::cout << "Invalid input files!\n";
        ReportUsage();
        return 1;
      }
    } else if ((arg == "-d" || arg == "-density") && i < argc) {
      str_target_density = std::string(argv[i++]);
      try {
        target_density = std::stod(str_target_density);
      } catch (...) {
        std::cout << "Invalid target density!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "iolayerd" && i < argc) {
      std::string str_metal_layer_num = std::string(argv[i++]);
      try {
        io_metal_layer = std::stoi(str_metal_layer_num) - 1;
      } catch (...) {
        std::cout << "Invalid metal layer number!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-v" && i < argc) {
      str_verbose_level = std::string(argv[i++]);
      try {
        verbose_level = StrToLoggingLevel(str_verbose_level);
      } catch (...) {
        std::cout << "Invalid stoi conversion: " << str_verbose_level << "\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-lognoprefix") {
      has_log_prefix = false;
    } else if (arg == "-wlgmode" && i < argc) {
      std::string str_wlg_mode = std::string(argv[i++]);
      if (str_wlg_mode == "scavenge") {
        well_legalization_mode =
            static_cast<int>(DefaultPartitionMode::SCAVENGE);
      } else if (str_wlg_mode == "strict") {
        well_legalization_mode = static_cast<int>(DefaultPartitionMode::STRICT);
      } else {
        std::cout << "Unknown well legalization mode: " << str_wlg_mode << "\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-nolegal") {
      is_no_legal = true;
    } else if (arg == "-noglobal") {
      is_no_global = true;
    } else if (arg == "-maxrowwidth" && i < argc) {
      try {
        max_row_width = std::stod(argv[i++]);
      } catch (...) {
        std::cout << "Invalid max row width!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-nowelltap") {
      is_well_tap_needed = false;
    } else if (arg == "-noioplace") {
      is_no_io_place = true;
    } else if (arg == "-clsmatlab") {
      export_well_cluster_for_matlab = true;
    } else if (arg == "-log" && i < argc) {
      log_file_name = std::string(argv[i++]);
      if (lef_file_name.empty()) {
        std::cout << "Invalid name for log file!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-lgthreads" && i < argc) {
      std::string str_lgthreads = std::string(argv[i++]);
      try {
        lg_threads = std::max(std::stoi(str_lgthreads), 1);
      } catch (...) {
        std::cout << "Invalid metal layer number!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-lgcplex") {
      lg_cplex = true;
    } else if (arg == "-gbmaxit" && i < argc) {
      std::string str_gb_maxiter = std::string(argv[i++]);
      try {
        gb_maxiter = std::stoi(str_gb_maxiter);
      } catch (...) {
        std::cout << "Invalid metal layer number!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-nthreads" && i < argc) {
      std::string str_nthreads = std::string(argv[i++]);
      try {
        num_threads = std::stoi(str_nthreads);
      } catch (...) {
        std::cout << "Invalid number of threads!\n";
        ReportUsage();
        return 1;
      }
    } else {
      std::cout << "Unknown flag\n";
      std::cout << arg << "\n";
      return 1;
    }
  }

  // initialize logger
  InitLogging(
      log_file_name,
      verbose_level,
      has_log_prefix
  );

  // print the software statement
  PrintSoftwareStatement();

  // print the current time
  using std::chrono::system_clock;
  system_clock::time_point today = system_clock::now();
  std::time_t tt = system_clock::to_time_t(today);
  BOOST_LOG_TRIVIAL(info) << "Today is: " << ctime(&tt) << "\n";

  // save command line arguments for future reference
  SaveArgs(argc, argv);

  // start the timer to record the runtime
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  // load LEF/DEF/CELL files
  // (1). initialize PhyDB
  if ((lef_file_name.empty()) || (def_file_name.empty())) {
    BOOST_LOG_TRIVIAL(info) << "Invalid input files!\n";
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
  Circuit circuit;
  circuit.InitializeFromPhyDB(&phy_db);
  if (!m_cell_file_name.empty()) {
    circuit.ReadMultiWellCell(m_cell_file_name);
  }
  circuit.ReportBriefSummary();

  // set the placement density
  if (target_density == -1) {
    double default_density = 0.7;
    target_density = std::max(circuit.WhiteSpaceUsage(), default_density);
    BOOST_LOG_TRIVIAL(info)
      << "Target density not provided, set it to default value: "
      << target_density << "\n";
  }

  // start placement
  // (1). global placement
  auto gb_placer = std::make_unique<GlobalPlacer>();
  gb_placer->SetInputCircuit(&circuit);
  gb_placer->SetNumThreads(num_threads);
  gb_placer->SetBoundaryDef();
  gb_placer->SetMaxIteration(gb_maxiter);
  if (!is_no_global) {
    gb_placer->SetPlacementDensity(target_density);
    //gb_placer->ReportBoundaries();
    gb_placer->StartPlacement();
  }
  if (export_well_cluster_for_matlab) {
    gb_placer->GenMATLABTable("gb_result.txt");
  }

  // (2). legalization
  if (!cell_file_name.empty()) {
    // (a). single row gridded cell legalization
    auto well_legalizer = std::make_unique<StdClusterWellLegalizer>();
    well_legalizer->TakeOver(gb_placer.get());
    well_legalizer->SetStripePartitionMode(well_legalization_mode);
    well_legalizer->StartPlacement();
    if (export_well_cluster_for_matlab) {
      well_legalizer->GenMATLABTable("sc_result.txt");
      well_legalizer->GenMatlabClusterTable("sc_result");
      well_legalizer->GenMATLABWellTable("scw", 0);
    }

    if (!output_name.empty()) {
      well_legalizer->EmitDEFWellFile(output_name, 1);
    }
  } else if (!m_cell_file_name.empty()) {
    // (b). multi row gridded cell legalization
    auto multi_well_legalizer = std::make_unique<GriddedRowLegalizer>();
    multi_well_legalizer->SetThreads(lg_threads);
    multi_well_legalizer->SetUseCplex(lg_cplex);
    multi_well_legalizer->TakeOver(gb_placer.get());
    multi_well_legalizer->SetWellTapCellParameters(
        is_well_tap_needed, false, -1, ""
    );
    multi_well_legalizer->SetMaxRowWidth(max_row_width);
    multi_well_legalizer->SetPartitionMode(well_legalization_mode);
    multi_well_legalizer->StartPlacement();
    if (export_well_cluster_for_matlab) {
      multi_well_legalizer->GenMATLABTable("sc_result.txt");
      multi_well_legalizer->GenMatlabClusterTable("sc_result");
      multi_well_legalizer->GenMATLABWellTable("scw", 0);
      multi_well_legalizer->GenDisplacement("disp_result.txt");
    }

    if (!output_name.empty()) {
      multi_well_legalizer->EmitDEFWellFile(output_name, 1);
    }
  } else {
    // (c). Tetris Legalization
    if (!is_no_legal) {
      auto legalizer = std::make_unique<LGTetrisEx>();
      legalizer->TakeOver(gb_placer.get());
      legalizer->StartPlacement();
    }
  }

  if (!is_no_io_place) {
    auto io_placer = std::make_unique<IoPlacer>(&phy_db, &circuit);
    bool is_ioplacer_config_success =
        io_placer->ConfigSetGlobalMetalLayer(io_metal_layer);
    DaliExpects(is_ioplacer_config_success,
                "Cannot successfully configure I/O placer");
    io_placer->AutoPlaceIoPin();
  }

  // save placement result
  circuit.SaveDefFile(output_name, "", def_file_name, 1, 1, 2, 1);
  circuit.SaveDefFile(output_name, "_io", def_file_name, 1, 1, 1, 1);
  circuit.SaveDefFile(output_name, "_filling", def_file_name, 1, 4, 2, 0);
  circuit.SaveDefFileComponent(output_name + "_comp.def", def_file_name);

  circuit.InitNetFanoutHistogram();
  circuit.ReportNetFanoutHistogram();
  circuit.ReportHPWLHistogramLinear();
  circuit.ReportHPWLHistogramLogarithm();

  elapsed_time.RecordEndTime();
  BOOST_LOG_TRIVIAL(info)
    << "****End of placement "
    << "(wall time: " << elapsed_time.GetWallTime() << "s, "
    << "cpu time: " << elapsed_time.GetCpuTime() << "s)****\n";

  return 0;
}

void ReportUsage() {
  std::cout
      << "\033[0;36m"
      << "Usage: dali\n"
      << "  -lef         <file.lef>\n"
      << "  -def         <file.def>\n"
      << "  -cell        <file.cell> (optional, if provided, well placement flow will be triggered)\n"
      << "  -mcell       <file.cell> (multiwell gridded cell)\n"
      << "  -o           <output_name>.def (optional, default output file name dali_out.def)\n"
      << "  -g/-grid     grid_value_x grid_value_y (optional, default metal1 and metal2 pitch values)\n"
      << "  -d/-density  density (optional, value interval (0,1], default max(space_utility, 0.7))\n"
      << "  -nolegal     optional, if this flag is present, then only perform global placement\n"
      << "  -iolayer     metal layer number for I/O placement (optional, default 1 for m1)\n"
      << "  -wlgmode     <scavenge/strict> determine whether the last column use unassigned space\n"
      << "  -v           verbosity_level (optional, 0-5, default 1)\n"
      << "  -lognoprefix optional, if this flag is present, then only messages will be saved to the log file\n"
      << "(flag order does not matter)"
      << "\033[0m\n";
}

void PrintSoftwareStatement() {
  BOOST_LOG_TRIVIAL(info)
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
