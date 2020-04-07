//
// Created by Yihang Yang on 3/5/20.
//

#include <ctime>

#include <iostream>
#include <vector>

#include "circuit.h"
#include "common/opendb.h"
#include "placer.h"

VerboseLevel globalVerboseLevel = LOG_CRITICAL;

#define USE_DB_PARSER 1

int main() {
  Circuit circuit;

  time_t Time = clock();

  std::string lef_file_name = "benchmark_100K.lef";
  std::string def_file_name = "benchmark_100K.def";
  std::string cel_file_name = "benchmark_100K.cell";

#if USE_DB_PARSER
  odb::dbDatabase *db = odb::dbDatabase::create();
  std::vector<std::string> defFileVec;
  defFileVec.push_back(def_file_name);
  odb_read_lef(db, lef_file_name.c_str());
  odb_read_def(db, defFileVec);
  circuit.InitializeFromDB(db);
#else
  circuit.ReadLefFile(lef_file_name);
  circuit.ReadDefFile(def_file_name);
#endif

  circuit.ReadCellFile(cel_file_name);

  std::cout << "File loading complete, time: " << double(clock() - Time) / CLOCKS_PER_SEC << " s\n";
  printf("  Average white space utility: %.4f\n", circuit.WhiteSpaceUsage());
  circuit.ReportBriefSummary();
  //circuit.ReportBlockType();
  circuit.ReportHPWL();

  WellPlaceFlow well_place_flow;
  well_place_flow.SetInputCircuit(&circuit);

  well_place_flow.SetBoundaryDef();
  well_place_flow.SetFillingRate(0.7);
  well_place_flow.ReportBoundaries();
  well_place_flow.StartPlacement();
  well_place_flow.GenMATLABTable("gb_result.txt");

  well_place_flow.EmitDEFWellFile("benchmark_10K_dali", def_file_name);

  Time = clock() - Time;
  std::cout << "Execution time " << double(Time) / CLOCKS_PER_SEC << "s.\n";

  return 0;
}
