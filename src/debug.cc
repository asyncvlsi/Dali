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
  std::string lefFileName = "Pbenchmark_1K.lef";
  std::string defFileName = "Pbenchmark_1K.def";
  using namespace odb;
  extern bool lefin_parse(lefin* , const char* );
  dbDatabase* db = dbDatabase::create();
  bool ignore_non_routing_layers = false;
  std::vector<std::string> defFileVec;
  defFileVec.push_back(defFileName);
  //odb_read_lef(db, lefFileName.c_str());
  //odb_read_def(db, defFileVec);

  return 0;

  Circuit circuit;

  std::string adaptec1_lef = "../test/adaptec1/adaptec1.lef";
  std::string adaptec1_def = "../test/adaptec1/adaptec1.def";

  std::string lef_file, def_file;
  //lef_file = "ispd19_sample3.input.lef";
  //def_file = "ispd19_sample3.input.def";
  lef_file = "Pbenchmark_1K.lef";
  def_file = "Pbenchmark_1K.def";
  //lef_file = adaptec1_lef;
  //def_file = adaptec1_def;
  //circuit.SetGridValue(0.01,0.01);
  time_t Time = clock();
  circuit.ReadLefFile(lef_file);
  circuit.ReadDefFile(def_file);
  //circuit.ReportBlockType();

  std::cout << "File loading complete, time: " << double(Time)/CLOCKS_PER_SEC << "s\n";

  circuit.ReportBriefSummary();

  circuit.ReportHPWL();

  Time = clock();
  Placer *gb_placer = new GPSimPL;
  gb_placer->SetInputCircuit(&circuit);

  gb_placer->SetBoundaryDef();
  gb_placer->SetFillingRate(1);
  gb_placer->ReportBoundaries();
  gb_placer->StartPlacement();
  //gb_placer->GenMATLABScript("gb_result.txt");
  //gb_placer->SaveNodeTerminal();
  //gb_placer->SaveDEFFile("circuit.def", def_file);
  //circuit.SaveISPD("circuit.pl");

  /*Placer *d_placer = new MDPlacer;
  d_placer->TakeOver(gb_placer);
  d_placer->StartPlacement();
  d_placer->GenMATLABScript("dp_result.txt");*/

  //Placer *legalizer = new TetrisLegalizer;
  //legalizer->TakeOver(gb_placer);
  //legalizer->StartPlacement();
  //legalizer->GenMATLABScript("legalizer_result.txt");
  //legalizer->SaveDEFFile("circuit.def", def_file);

  delete gb_placer;
  //delete d_placer;
  //delete legalizer;

  Time = clock() - Time;
  std::cout << "Execution time " << double(Time)/CLOCKS_PER_SEC << "s.\n";

  return 0;
}
