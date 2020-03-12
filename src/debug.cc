//
// Created by Yihang Yang on 2019-05-14.
//

#include <ctime>

#include <iostream>
#include <vector>

#include "circuit.h"
#include "opendb.h"
#include "placer.h"

VerboseLevel globalVerboseLevel = LOG_CRITICAL;

#define TEST_LG 0
#define TEST_PO 0
#define TEST_WELL 0
#define TEST_CLUSTER_WELL 1
#define USE_DB_PARSER 0

int main() {
  Circuit circuit;

  time_t Time = clock();

  std::string lef_file_name = "benchmark_1K.lef";
  std::string def_file_name = "benchmark_1K.def";

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

  std::cout << "File loading complete, time: " << double(clock() - Time) / CLOCKS_PER_SEC << " s\n";
  printf("  Average white space utility: %.4f\n", circuit.WhiteSpaceUsage());
  circuit.ReportBriefSummary();
  //circuit.ReportBlockType();
  circuit.ReportHPWL();

  Placer *gb_placer = new GPSimPL;
  gb_placer->SetInputCircuit(&circuit);

  gb_placer->SetBoundaryDef();
  gb_placer->SetFillingRate(0.8);
  gb_placer->ReportBoundaries();
  gb_placer->StartPlacement();
  //gb_placer->SaveDEFFile("benchmark_1K_dali.def", def_file_name);
  gb_placer->GenMATLABTable("gb_result.txt");
  //gb_placer->GenMATLABWellTable("gb_result");

  /*Placer *d_placer = new MDPlacer;
  d_placer->TakeOver(gb_placer);
  d_placer->StartPlacement();
  d_placer->GenMATLABScript("dp_result.txt");*/

  Placer *legalizer = new LGTetrisEx;
  legalizer->TakeOver(gb_placer);
  legalizer->StartPlacement();
  legalizer->GenMATLABTable("lg_result.txt");
  //legalizer->SaveDEFFile("circuit.def", def_file);

#if TEST_PO
  Placer *post_optimizer = new PLOSlide;
  post_optimizer->TakeOver(legalizer);
  post_optimizer->StartPlacement();
  post_optimizer->GenMATLABTable("po_result.txt");
  delete post_optimizer;
#endif

#if TEST_WELL
  std::string cell_file_name("benchmark_1K.cell");
  circuit.ReadCellFile(cell_file_name);
  Placer *well_legalizer = new WellLegalizer;
  well_legalizer->TakeOver(gb_placer);
  well_legalizer->StartPlacement();
  circuit.GenMATLABWellTable("lg_result");
  delete well_legalizer;
#endif

#if TEST_CLUSTER_WELL
  Placer *cluster_well_legalizer = new ClusterWellLegalizer;
  std::string cell_file_name("benchmark_1K.cell");
  circuit.ReadCellFile(cell_file_name);
  cluster_well_legalizer->TakeOver(gb_placer);
  cluster_well_legalizer->StartPlacement();
  delete cluster_well_legalizer;
#endif

  delete gb_placer;
  delete legalizer;

/*#ifdef USE_OPENDB
  odb::dbDatabase::destroy(db);
#endif*/

  Time = clock() - Time;
  std::cout << "Execution time " << double(Time) / CLOCKS_PER_SEC << "s.\n";

  return 0;
}
