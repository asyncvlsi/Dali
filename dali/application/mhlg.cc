/*******************************************************************************
 *
 * Copyright (c) 2022 Yihang Yang
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
#include <string>
#include <vector>

#include <phydb/phydb.h>

#include "dali/circuit/circuit.h"
#include "dali/common/helper.h"
#include "dali/common/logging.h"

#include "dali/placer.h"

using namespace dali;

void ReportUsage();

int main(int argc, char *argv[]) {
  if (argc < 5) {
    ReportUsage();
    return 1;
  }

  InitLogging("", boost::log::trivial::info, true);
  SaveArgs(argc, argv);
  std::vector<std::vector<std::string>> options =
      ParseArguments(argc, argv, "--");

  std::vector<std::string> lef_files;
  std::vector<std::string> def_files;
  std::string output_name = "dali_out";
  int number_of_threads = 1;
  bool is_export_matlab = false;
  double k_width = 0.0;
  double k_height = 0.0;
  double k_left = 0.5;
  double k_left_step = 0.5;

  for (auto &option : options) {
    std::string &flag = option[0];
    if (flag == "--lef") {
      lef_files.assign(option.begin() + 1, option.end());
      DaliExpects(!lef_files.empty(), "No lef file provided!");
    } else if (flag == "--def") {
      def_files.assign(option.begin() + 1, option.end());
      DaliExpects(!def_files.empty(), "No def file provided!");
    } else if (flag == "--o") {
      DaliExpects(option.size() >= 2, "No output file name provided!");
      output_name = option[1];
    } else if (flag == "--t") {
      DaliExpects(option.size() >= 2, "No #threads provided!");
      try {
        number_of_threads = std::stoi(option[1]);
      } catch (...) {
        DaliExpects(false, "Invalid #threads!");
      }
    } else if (flag == "--clsmatlab") {
      is_export_matlab = true;
    } else if (flag == "--kwidth") {
      try {
        k_width = std::stod(option[1]);
      } catch (...) {
        DaliExpects(false, "Invalid k_width!");
      }
    } else if (flag == "--kheight") {
      try {
        k_height = std::stod(option[1]);
      } catch (...) {
        DaliExpects(false, "Invalid k_height!");
      }
    } else if (flag == "--kleft") {
      try {
        k_left = std::stod(option[1]);
        k_left_step = std::stod(option[2]);
      } catch (...) {
        DaliExpects(false, "Invalid k_left!");
      }
    } else {
      DaliExpects(false, "Unknown flag: " + option[0]);
    }
  }
  options.clear();

  /**** time ****/
  ElapsedTime elapsed_time;
  elapsed_time.RecordStartTime();

  /**** read LEF/DEF/CELL ****/
  // (1). initialize PhyDB
  phydb::PhyDB phy_db;
  phy_db.SetPlacementGrids(0.2, 0.2);
  for (auto &lef_file_name : lef_files) {
    phy_db.ReadLef(lef_file_name);
  }
  for (auto &def_file_name : def_files) {
    phy_db.ReadDef(def_file_name);
  }

  // (2). initialize Circuit
  Circuit circuit;
  circuit.InitializeFromPhyDB(&phy_db);
  circuit.CreateFakeWellForStandardCell();
  circuit.ReportBriefSummary();
  circuit.ReportHPWL();
  circuit.ReportBoundingBox();

  /**** legalization ****/
  auto multi_well_legalizer = std::make_unique<GriddedRowLegalizer>();
  auto tetris_legalizer = std::make_unique<LGTetrisEx>();
  multi_well_legalizer->SetInputCircuit(&circuit);
  multi_well_legalizer->SetBoundaryFromCircuit();
  multi_well_legalizer->ImportStandardRowSegments(phy_db);
  multi_well_legalizer->InitializeBlockAuxiliaryInfo();
  multi_well_legalizer->SaveInitialLoc();

  tetris_legalizer->SetWidthHeightFactor(k_width, k_height);
  tetris_legalizer->SetLeftBoundFactor(k_left, k_left_step);
  tetris_legalizer->InitializeFromGriddedRowLegalizer(multi_well_legalizer.get());
  tetris_legalizer->SetMaxIteration(10);
  tetris_legalizer->StartRowAssignment();

  circuit.GenMATLABTable("lg_result.txt");
  multi_well_legalizer->GenDisplacement("disp_result.txt");

  /*
  multi_well_legalizer->StartStandardLegalization();
  if (is_export_matlab) {
    multi_well_legalizer->GenMATLABTable("sc_result.txt");
    multi_well_legalizer->GenMatlabClusterTable("sc_result");
    //multi_well_legalizer->GenMATLABWellTable("scw", 0);
  }
  multi_well_legalizer->ReportStandardCellDisplacement();
   */


  //tetris_legalizer->InitializeFromGriddedRowLegalizer(multi_well_legalizer.get());
  //tetris_legalizer->IsPrintDisplacement(true);
  //tetris_legalizer->StartMultiHeightLegalization();
  //tetris_legalizer->GenMATLABTable("lg1_result.txt");
  //tetris_legalizer->GenDisplacement("disp1_result.txt");

  if (!output_name.empty()) {
    circuit.SaveDefFile(output_name, "", def_files[0], 1, 1, 2, 1);
  }

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
      << "Usage: mhlg\n"
      << "  -lef         <file.lef>\n"
      << "  -def         <file.def>\n"
      << "  -o           <output_name>.def (optional, default output file name dali_out.def)\n"
      << "  -t           #threads (optional, default 1)\n"
      << "(flag order does not matter)"
      << "\033[0m\n";
}