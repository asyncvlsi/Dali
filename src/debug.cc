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

#define TEST_WELL 0
#define PP 1
#define TEST_ADA 1

int main() {
  Circuit circuit;

  time_t Time = clock();

#if !TEST_ADA
  std::string lef_file_name = "benchmark_200K.lef";
  std::string def_file_name = "benchmark_200K.def";

#ifdef USE_OPENDB
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

#else
  std::string adaptec1_lef = "../test/adaptec1/adaptec1.lef";
  std::string adaptec1_def = "../test/adaptec1/adaptec1.def";

  circuit.SetGridValue(0.01,0.01);
  circuit.ReadLefFile(adaptec1_lef);
  circuit.ReadDefFile(adaptec1_def);
  circuit.def_left = 459;
  circuit.def_right = 10692 + 459;
  circuit.def_bottom = 459;
  circuit.def_top = 11127 + 12;
#endif

  std::cout << "File loading complete, time: " << double(clock() - Time) / CLOCKS_PER_SEC << "s\n";

  circuit.ReportBriefSummary();
  //circuit.ReportBlockType();
  circuit.ReportHPWL();

  Placer *gb_placer = new GPSimPL;
  gb_placer->SetInputCircuit(&circuit);

  gb_placer->SetBoundaryDef();
  gb_placer->SetFillingRate(1);
  gb_placer->ReportBoundaries();
  gb_placer->StartPlacement();
  gb_placer->GenMATLABTable("gb_result.txt");
  //gb_placer->GenMATLABWellTable("gb_result");
  //gb_placer->SaveDEFFile("circuit.def", adaptec1_def);

#if TEST_ADA
  circuit.SaveBookshelfPl("adaptec1bs.pl");
  exit(128);
#endif

  /*Placer *d_placer = new MDPlacer;
  d_placer->TakeOver(gb_placer);
  d_placer->StartPlacement();
  d_placer->GenMATLABScript("dp_result.txt");*/

#if PP
  Placer *legalizer = new LGTetrisEx;
#else
  Placer *legalizer = new TetrisLegalizer;
#endif
  legalizer->TakeOver(gb_placer);
  legalizer->StartPlacement();
  legalizer->GenMATLABTable("lg_result.txt");
  //legalizer->SaveDEFFile("circuit.def", def_file);

#if !TEST_ADA
  Placer *post_optimizer = new PLOSlide;
  post_optimizer->TakeOver(legalizer);
  post_optimizer->StartPlacement();
  post_optimizer->GenMATLABTable("po_result.txt");

#if TEST_WELL
  std::string cell_file_name("benchmark_10K.cell");
  circuit.ReadCellFile(cell_file_name);
  Placer *well_legalizer = new WellLegalizer;
  well_legalizer->TakeOver(gb_placer);
  well_legalizer->StartPlacement();
  circuit.GenMATLABWellTable("lg_result");
  delete well_legalizer;
#endif
  //delete d_placer;
  delete post_optimizer;
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
