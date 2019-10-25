//
// Created by Yihang Yang on 9/3/19.
//

#include <iostream>
#include "common/global.h"
#include "circuit.h"
#include "placer.h"

VerboseLevel globalVerboseLevel = LOG_CRITICAL;

void ReportUsage();

int main(int argc, char *argv[]) {
  if (argc < 4) {
    ReportUsage();
    return 1;
  }
  std::string lef_file_name;
  std::string def_file_name;
  std::string output_name;
  std::string str_grid_value_x, str_grid_value_y;
  std::string str_verbose_level;
  std::string str_x_grid;
  std::string str_y_grid;
  double x_grid = 0, y_grid = 0;

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
    } else if (arg == "-o" && i < argc) {
      output_name = std::string(argv[i++]);
      if (output_name.empty()) {
        std::cout << "Invalid output name!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-grid" && i < argc) {
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
    } else if (arg == "-v" && i < argc) {
      str_verbose_level = std::string(argv[i++]);
      int tmp = -1;
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

  time_t Time = clock();
  Circuit circuit;
  circuit.SetGridValue(x_grid, y_grid);
  circuit.ReadLefFile(lef_file_name);
  circuit.ReadDefFile(def_file_name);

  Time = clock() - Time;
  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "File loading complete, time: " << (float) Time / CLOCKS_PER_SEC << std::endl;
  }
  circuit.ReportBriefSummary();
  circuit.ReportHPWL();

  Time = clock();
  Placer *gb_placer = new GPSimPL;
  gb_placer->SetInputCircuit(&circuit);
  gb_placer->SetBoundaryDef();
  gb_placer->SetFillingRate(1);
  gb_placer->ReportBoundaries();
  gb_placer->StartPlacement();

  /*Placer *d_placer = new MDPlacer;
  d_placer->TakeOver(gb_placer);
  d_placer->StartPlacement();
  */

  Placer *legalizer = new TetrisLegalizer;
  legalizer->TakeOver(gb_placer);
  legalizer->StartPlacement();

  delete gb_placer;
  //delete d_placer;
  delete legalizer;

  Time = clock() - Time;
  std::cout << "Execution time " << (float)Time/CLOCKS_PER_SEC << "s.\n";
  if (!output_name.empty()) {
    circuit.SaveDefFile(output_name, def_file_name);
  }

  return 0;
}

void ReportUsage() {
  std::cout << "Usage: hpcc\n"
               " -lef <file.lef>\n"
               " -def <file.def>\n"
               " -o <output_name.def>\n"
               " -grid grid_value_x grid_value_y\n"
               " -v verbosity_level (optional, 0-5, default 0)\n"
               "(order does not matter)"
               << std::endl;
}