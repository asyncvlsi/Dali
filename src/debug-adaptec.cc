//
// Created by Yihang Yang on 2/6/20
// this is for ISPD2005/2006 benchmark suite debugging
//

#include <ctime>

#include <iostream>

#include "circuit.h"
#include "common/si2lefdef.h"
#include "placer.h"

VerboseLevel globalVerboseLevel = LOG_CRITICAL;

#define TEST_LG 0
#define TEST_WLG 0
#define USE_DB_PARSER 0

int main() {
  Circuit circuit;

  time_t Time = clock();
  int num_of_thread_openmp = 1;
  omp_set_num_threads(num_of_thread_openmp);

  std::string adaptec1_lef = "ISPD2005/adaptec1.lef";
#if TEST_LG
  std::string adaptec1_def = "adaptec1_pl.def";
#else
  std::string adaptec1_def = "ISPD2005/adaptec1.def";
#endif

  circuit.setGridValue(0.01, 0.01);
  readLef(adaptec1_lef, circuit);
  readDef(adaptec1_def, circuit);
  //circuit.ReportBlockType();
  //circuit.ReportNetList();
#if USE_DB_PARSER
  odb::dbDatabase *db = odb::dbDatabase::create();
  std::vector<std::string> defFileVec;
  defFileVec.push_back(adaptec1_def);
  dup2(1, 2); // redirect log of OpenDB parser from stderr to stdout, because this stderr log is annoying
  odb_read_lef(db, adaptec1_lef.c_str());
  odb_read_def(db, defFileVec);
  circuit.InitializeFromDB(db);
#else
  //circuit.ReadLefFile(adaptec1_lef);
  //circuit.ReadDefFile(adaptec1_def);
#endif

  //circuit.getDesign()->region_left_ = 459;
  //circuit.getDesign()->region_right_ = 10692 + 459;
  //circuit.getDesign()->region_bottom_ = 459;
  //circuit.getDesign()->region_top_ = 11127 + 12;
  //circuit.GenMATLABTable("_result.txt");

  std::cout << "File loading complete, time: " << double(clock() - Time) / CLOCKS_PER_SEC << " s\n";

  circuit.ReportBriefSummary();
  circuit.ReportHPWL();

  GPSimPL gb_placer;
  gb_placer.SetInputCircuit(&circuit);
  gb_placer.SetBoundaryDef();
  gb_placer.SetFillingRate(1);
  gb_placer.ReportBoundaries();
  //gb_placer.is_dump = true;
#if !TEST_LG
  gb_placer.StartPlacement();
  gb_placer.SaveDEFFile("adaptec1_pl.def", adaptec1_def);
  circuit.SaveBookshelfPl("adaptec1bs.pl");
#endif
  gb_placer.GenMATLABTable("gb_result.txt");

  /*
  LGTetrisEx legalizer;
  legalizer.TakeOver(&gb_placer);
  legalizer.SetRowHeight(12);
  legalizer.StartPlacement();
  //legalizer.GenAvailSpace("as_result.txt");
  legalizer.GenMATLABTable("lg_result.txt");
  //legalizer->SaveDEFFile("circuit.def", def_file);
   */

#if TEST_WLG
  circuit.LoadImaginaryCellFile();
  //circuit.ReportWellShape();
  auto *well_legalizer = new StdClusterWellLegalizer;
  well_legalizer->TakeOver(&gb_placer);
  well_legalizer->SetRowHeight(1);
  well_legalizer->StartPlacement();
  well_legalizer->GenMATLABTable("sc_result.txt");
  well_legalizer->GenMATLABWellTable("scw");
  well_legalizer->EmitDEFWellFile("circuit", adaptec1_def);
  delete well_legalizer;
#endif

  Time = clock() - Time;
  std::cout << "Execution time " << double(Time) / CLOCKS_PER_SEC << "s.\n";

  return 0;
}
