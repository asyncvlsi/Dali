//
// Created by Yihang Yang on 9/3/19.
//

#include <iostream>
#include <chrono>
#include <ratio>
#include <ctime>
#include <algorithm>
#include "common/global.h"
#include "opendb.h"
#include "circuit.h"
#include "placer.h"

VerboseLevel globalVerboseLevel = LOG_CRITICAL;

void ReportUsage();

int main(int argc, char *argv[]) {
  using std::chrono::system_clock;
  system_clock::time_point today = system_clock::now();
  std::time_t tt = system_clock::to_time_t (today);
  std::cout << "today is: " << ctime(&tt) << std::endl;

  if (argc < 5) {
    ReportUsage();
    return 1;
  }
  std::string lef_file_name;
  std::string def_file_name;
  std::string cell_file_name;
  std::string output_name = std::to_string(tt) + ".def";
  std::string str_grid_value_x, str_grid_value_y;
  std::string str_target_density;
  std::string str_verbose_level;
  std::string str_x_grid;
  std::string str_y_grid;
  double x_grid = 0, y_grid = 0;
  double target_density = -1;

  time_t Time = clock();
  Circuit circuit;

  for( int i = 1; i < argc; ) {
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
    } else if (arg == "-o" && i < argc) {
      output_name = std::string(argv[i++]);
      output_name += ".def";
      if (output_name.empty()) {
        std::cout << "Invalid output name!\n";
        ReportUsage();
        return 1;
      }
    } else if ((arg=="-g" || arg=="-grid") && i < argc) {
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
    } else if ((arg=="-d" || arg=="-density") && i < argc) {
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
      if (tmp>5 || tmp<0) {
        std::cout << "Invalid verbosity level\n";
        ReportUsage();
        return 0;
      }
      globalVerboseLevel = (VerboseLevel)tmp;
    } else {
      std::cout << "Unknown option for file reading\n";
      std::cout << arg << "\n";
      return 1;
    }
  }

  if ((lef_file_name.empty())||(def_file_name.empty())) {
    std::cout << "Invalid input files!\n";
    ReportUsage();
    return 1;
  }

#ifdef USE_OPENDB
  odb::dbDatabase* db = odb::dbDatabase::create();
  std::vector<std::string> defFileVec;
  defFileVec.push_back(def_file_name);
  odb_read_lef(db, lef_file_name.c_str());
  odb_read_def(db, defFileVec);
  circuit.InitializeFromDB(db);
#else
  circuit.ReadLefFile(lef_file_name);
  circuit.ReadDefFile(def_file_name);
#endif

  if (!cell_file_name.empty()) {
    circuit.ReadWellFile(cell_file_name);
  }

  Time = clock() - Time;
  std::cout << "File loading complete, time: " << float(Time)/CLOCKS_PER_SEC << "s\n";
  circuit.ReportBriefSummary();
  circuit.ReportHPWL();

  if (target_density == -1) {
    target_density = std::min(circuit.WhiteSpaceUsage() * 1.2, 1.0);
  }
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "target density: " << target_density << "\n";
  }

  Time = clock();
  Placer *gb_placer = new GPSimPL;
  gb_placer->SetInputCircuit(&circuit);
  gb_placer->SetBoundaryDef();
  gb_placer->SetFillingRate(target_density);
  gb_placer->ReportBoundaries();
  gb_placer->StartPlacement();
  time_t gp_time = clock() - Time;
  std::cout << "global placement complete, time: " << float(gp_time)/CLOCKS_PER_SEC << "s\n";

  /*Placer *d_placer = new MDPlacer;
  d_placer->TakeOver(gb_placer);
  d_placer->StartPlacement();
  */

  Placer *legalizer = new TetrisLegalizer;
  legalizer->TakeOver(gb_placer);
  legalizer->StartPlacement();
  time_t lg_time = clock() - gp_time;
  std::cout << "legalization complete, time: " << float(lg_time)/CLOCKS_PER_SEC << "s\n";

  if (!cell_file_name.empty()) {
    Placer *well_legalizer = new WellLegalizer;
    well_legalizer->TakeOver(gb_placer);
    well_legalizer->StartPlacement();
    delete well_legalizer;
  }

  delete gb_placer;
  //delete d_placer;
  delete legalizer;

  Time = clock() - Time;
  std::cout << "Execution time " << float(Time)/CLOCKS_PER_SEC << "s.\n";
  if (!output_name.empty()) {
    circuit.SaveDefFile(output_name, def_file_name);
  }

  return 0;
}

void ReportUsage() {
  std::cout << "\033[0;36m"
            << "Usage: dali\n"
            << "  -lef        <file.lef>\n"
            << "  -def        <file.def>\n"
            << "  -cell       <file.cell> (optional, if provided, well legalization will be triggered)\n"
            << "  -o          <output_name>.def (optional, default name timestamp.def)\n"
            << "  -g/-grid    grid_value_x grid_value_y (optional, default metal1 and metal 2 pitch values)\n"
            << "  -d/-density density (optional, value interval (0,1], default 1)\n"
            << "  -v          verbosity_level (optional, 0-5, default 0)\n"
            << "(order does not matter)"
            << "\033[0m\n";
}