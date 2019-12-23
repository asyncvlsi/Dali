//
// Created by Yihang Yang on 2019-05-14.
//

#include <iostream>
#include <vector>
#include <ctime>
#include "opendb.h"
#include "circuit.h"
#include "placer.h"

VerboseLevel globalVerboseLevel = LOG_DEBUG;

int main() {
  Circuit circuit;

  time_t Time = clock();

  std::string lef_file_name = "Pbenchmark_1K.lef";
  std::string def_file_name = "Pbenchmark_1K.def";

  odb::dbDatabase* db = odb::dbDatabase::create();
  std::vector<std::string> defFileVec;
  defFileVec.push_back(def_file_name);
  odb_read_lef(db, lef_file_name.c_str());
  odb_read_def(db, defFileVec);
  circuit.InitializeFromDB(db);

  //circuit.ReadLefFile(lef_file_name);
  //circuit.ReadDefFile(def_file_name);
  std::cout << "File loading complete, time: " << double(Time)/CLOCKS_PER_SEC << "s\n";

  circuit.ReportBriefSummary();
  //circuit.ReportBlockType();
  circuit.ReportHPWL();

  Placer *gb_placer = new GPSimPL;
  gb_placer->SetInputCircuit(&circuit);

  gb_placer->SetBoundaryDef();
  gb_placer->SetFillingRate(1);
  gb_placer->ReportBoundaries();
  gb_placer->StartPlacement();
  //gb_placer->GenMATLABScript("gb_result.txt");
  //gb_placer->SaveNodeTerminal();
  //gb_placer->SaveDEFFile("circuit.def", def_file);

  /*Placer *d_placer = new MDPlacer;
  d_placer->TakeOver(gb_placer);
  d_placer->StartPlacement();
  d_placer->GenMATLABScript("dp_result.txt");*/

  Placer *legalizer = new TetrisLegalizer;
  legalizer->TakeOver(gb_placer);
  legalizer->StartPlacement();
  //legalizer->GenMATLABScript("legalizer_result.txt");
  //legalizer->SaveDEFFile("circuit.def", def_file);

  if (1) {
    std::string well_file_name("Pbenchmark_1K.cell");
    circuit.ReadWellFile(well_file_name);
    Placer *well_legalizer = new WellLegalizer;
    well_legalizer->TakeOver(gb_placer);

    delete well_legalizer;
  }

  delete gb_placer;
  //delete d_placer;
  delete legalizer;

  Time = clock() - Time;
  std::cout << "Execution time " << double(Time)/CLOCKS_PER_SEC << "s.\n";

  return 0;
}

void Test(Circuit &circuit) {
  std::string adaptec1_lef = "../test/adaptec1/adaptec1.lef";
  std::string adaptec1_def = "../test/adaptec1/adaptec1.def";

  std::string lef_file, def_file;
  circuit.SetGridValue(0.01,0.01);
  circuit.ReadLefFile(adaptec1_lef);
  circuit.ReadDefFile(adaptec1_def);
}
