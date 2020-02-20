//
// Created by Yihang Yang on 9/3/19.
//

#include <ctime>

#include <chrono>
#include <algorithm>
#include <iostream>
#include <ratio>

#include "circuit.h"
#include "common/global.h"
#include "common/timing.h"
#include "opendb.h"
#include "placer.h"

VerboseLevel globalVerboseLevel = LOG_CRITICAL;

void ReportUsage();

int main(int argc, char *argv[]) {
  using std::chrono::system_clock;
  system_clock::time_point today = system_clock::now();
  std::time_t tt = system_clock::to_time_t(today);
  std::cout << "today is: " << ctime(&tt) << std::endl;

  if (argc < 5) {
    ReportUsage();
    return 1;
  }
  std::string lef_file_name;
  std::string def_file_name;
  std::string cell_file_name;
  std::string output_name = "dali_out.def";
  std::string str_grid_value_x, str_grid_value_y;
  std::string str_target_density;
  std::string str_verbose_level;
  std::string str_x_grid;
  std::string str_y_grid;
  std::string plot_file;
  double x_grid = 0, y_grid = 0;
  double target_density = -1;
  bool use_naive = false;

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();
  Circuit circuit;

  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-n" && i < argc) {
      use_naive = true;
    } else if (arg == "-lef" && i < argc) {
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
    } else if (arg == "-o" && i < argc) {
      output_name = std::string(argv[i++]);
      output_name += ".def";
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
        circuit.SetGridValue(x_grid, y_grid);
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
    } else if (arg == "-v" && i < argc) {
      str_verbose_level = std::string(argv[i++]);
      int tmp;
      try {
        tmp = std::stoi(str_verbose_level);
      } catch (...) {
        tmp = -1;
      }
      if (tmp > 5 || tmp < 0) {
        std::cout << "Invalid verbosity level\n";
        ReportUsage();
        return 0;
      }
      globalVerboseLevel = (VerboseLevel) tmp;
    } else if (arg == "-wp" && i < argc) {
      plot_file = std::string(argv[i++]);
      if (plot_file.empty()) {
        std::cout << "Invalid output name!\n";
        ReportUsage();
        return 1;
      }
    } else {
      std::cout << "Unknown option for file reading\n";
      std::cout << arg << "\n";
      return 1;
    }
  }

  if ((lef_file_name.empty()) || (def_file_name.empty())) {
    std::cout << "Invalid input files!\n";
    ReportUsage();
    return 1;
  }

  double file_wall_time = get_wall_time();
  double file_cpu_time = get_cpu_time();
  bool use_opendb = false;
  if (!use_naive) {
#ifdef USE_OPENDB
    odb::dbDatabase *db = odb::dbDatabase::create();
    std::vector<std::string> defFileVec;
    defFileVec.push_back(def_file_name);
    odb_read_lef(db, lef_file_name.c_str());
    odb_read_def(db, defFileVec);
    circuit.InitializeFromDB(db);
    use_opendb = true;
#endif
  }
  if (use_naive || !use_opendb) {
    circuit.ReadLefFile(lef_file_name);
    circuit.ReadDefFile(def_file_name);
  }

  file_wall_time = get_wall_time() - file_wall_time;
  file_cpu_time = get_cpu_time() - file_cpu_time;
  std::cout << "File loading complete\n";
  printf("(wall time: %.4fs, cpu time: %.4fs)\n", file_wall_time, file_cpu_time);
  circuit.ReportBriefSummary();
  circuit.ReportHPWL();

  double default_density = 0.8;
  if (target_density == -1) {
    target_density = std::max(circuit.WhiteSpaceUsage(), default_density);
  }
  if (circuit.WhiteSpaceUsage() > target_density) {
    std::cout << "Cannot set target density smaller than average white space utility!\n";
    printf("  Average white space utility: %.4f\n", circuit.WhiteSpaceUsage());
    printf("  Target density: %.4f\n", target_density);

    return 1;
  }
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("  Target density: %.4f", target_density);
    if (target_density == default_density) {
      std::cout << " (default)";
    }
    std::cout << "\n";
  }

  Placer *gb_placer = new GPSimPL;
  gb_placer->SetInputCircuit(&circuit);
  gb_placer->SetBoundaryDef();
  gb_placer->SetFillingRate(target_density);
  gb_placer->ReportBoundaries();
  gb_placer->StartPlacement();

  /*Placer *d_placer = new MDPlacer;
  d_placer->TakeOver(gb_placer);
  d_placer->StartPlacement();
  */

  Placer *legalizer = new LGTetrisEx;
  legalizer->TakeOver(gb_placer);
  legalizer->StartPlacement();

  if (!cell_file_name.empty()) {
    circuit.ReadCellFile(cell_file_name);
    Placer *well_legalizer = new WellLegalizer;
    well_legalizer->TakeOver(gb_placer);
    well_legalizer->StartPlacement();
    delete well_legalizer;

    if (!plot_file.empty()) {
      circuit.GenMATLABWellTable(plot_file);
    }
  }

  delete gb_placer;
  //delete d_placer;
  delete legalizer;

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  /*std::cout << "****End of placement (wall time:"
            << wall_time << "s, cpu time: "
            << cpu_time << "s)****\n";*/
  printf("****End of placement (wall time: %.4fs, cpu time: %.4fs)****\n", wall_time, cpu_time);
  if (!output_name.empty()) {
    circuit.SaveDefFile(output_name, def_file_name);
  }

  return 0;
}

void ReportUsage() {
  std::cout << "\033[0;36m"
            << "Usage: dali\n"
            << "  -n          (if this flag present, use naive LEF/DEF parser)\n"
            << "  -lef        <file.lef>\n"
            << "  -def        <file.def>\n"
            << "  -cell       <file.cell> (optional, if provided, well legalization will be triggered)\n"
            << "  -o          <output_name>.def (optional, default name timestamp.def)\n"
            << "  -g/-grid    grid_value_x grid_value_y (optional, default metal1 and metal 2 pitch values)\n"
            << "  -d/-density density (optional, value interval (0,1], default min(1.2*space_utility, 1))\n"
            << "  -v          verbosity_level (optional, 0-5, default 0)\n"
            << "  -wp         <file>.txt (optional, create files for N/P-well plotting, usually using MATLAB)\n"
            << "(order does not matter)"
            << "\033[0m\n";
}