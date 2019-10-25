//
// Created by Yihang Yang on 2019-05-14.
//

#include <iostream>
#include <vector>
#include <ctime>
#include "circuit.h"
#include "placer.h"

VerboseLevel globalVerboseLevel = LOG_INFO;

int main() {
  Circuit circuit;

  std::string adaptec1_lef = "../test/adaptec1/adaptec1.lef";
  std::string adaptec1_def = "../test/adaptec1/adaptec1.def";

  std::string lef_file, def_file;
  lef_file = "../test/out_1K.lef";
  def_file = "../test/out_1K.def";
  //lef_file = adaptec1_lef;
  //def_file = adaptec1_def;
  time_t Time = clock();
  circuit.ReadLefFile(lef_file);
  circuit.ReadDefFile(def_file);
  std::cout << "File loading complete, time: " << (float) Time / CLOCKS_PER_SEC << std::endl;

  //circuit.ReportBlockTypeList();
  //circuit.ReportBlockTypeMap();
  //circuit.ReportBlockList();
  //circuit.ReportBlockMap();
  //circuit.ReportNetList();
  //circuit.ReportNetMap();
  circuit.ReportBriefSummary();

  circuit.ReportHPWL();

  Time = clock();
  Placer *gb_placer = new GPSimPL;
  gb_placer->SetInputCircuit(&circuit);

  //gb_placer->SetFillingRate(1/1.4);
  //gb_placer->SetAspectRatio(1);
  //gb_placer->SetBoundaryAuto();

  gb_placer->SetBoundaryDef();
  gb_placer->SetFillingRate(1);
  gb_placer->ReportBoundaries();
  gb_placer->StartPlacement();
  gb_placer->GenMATLABScript("gb_result.txt");
  //gb_placer->SaveNodeTerminal();

  Placer *d_placer = new MDPlacer;
  d_placer->TakeOver(gb_placer);
  d_placer->StartPlacement();
  d_placer->GenMATLABScript("dp_result.txt");

  Placer *legalizer = new TetrisLegalizer;
  legalizer->TakeOver(gb_placer);
  legalizer->StartPlacement();
  legalizer->GenMATLABScript("legalizer_result.txt");
  //legalizer->SaveDEFFile("circuit.def", def_file);

  delete gb_placer;
  delete d_placer;
  delete legalizer;

  Time = clock() - Time;
  std::cout << "Execution time " << (float)Time/CLOCKS_PER_SEC << "s.\n";

  return 0;
}
