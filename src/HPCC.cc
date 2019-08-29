//
// Created by Yihang Yang on 2019-05-14.
//

#include <iostream>
#include <vector>
#include <ctime>
#include "circuit/circuit.h"
#include "placer/placer.h"
#include "placer/globalPlacer/GPSimPL.h"
#include "placer/legalizer/LGTetris.h"

int main() {
  time_t Time = clock();
  Circuit circuit;

  std::string adaptec1_lef = "../test/adaptec1/adaptec1.lef";
  std::string adaptec1_def = "../test/adaptec1/adaptec1.def";

  std::string lef_file, def_file;
  lef_file = "../test/out_1K.lef";
  def_file = "../test/out_1K.def";
  //lef_file = adaptec1_lef;
  //def_file = adaptec1_def;
  circuit.ReadLefFile(lef_file);
  circuit.ReadDefFile(def_file);

  //circuit.ReportBlockTypeList();
  //circuit.ReportBlockTypeMap();
  //circuit.ReportBlockList();
  //circuit.ReportBlockMap();
  //circuit.ReportNetList();
  //circuit.ReportNetMap();
  circuit.ReportBriefSummary();

  Placer *gb_placer = new GPSimPL;
  gb_placer->SetInputCircuit(&circuit);
  //gb_placer->SaveNodeTerminal();

  //gb_placer->SetFillingRate(1/1.4);
  //gb_placer->SetAspectRatio(1);
  //gb_placer->SetBoundaryAuto();


  gb_placer->SetBoundaryDef();
  gb_placer->SetFillingRate(1);
  gb_placer->ReportBoundaries();
  gb_placer->StartPlacement();
  //gb_placer->GenMATLABScript("gb_placement_result.txt");
  //gb_placer->SaveNodeTerminal();

  /*
  Placer *legalizer = new TetrisLegalizer;
  legalizer->TakeOver(gb_placer);
  legalizer->StartPlacement();
  legalizer->GenMATLABScript("legalizer_result.txt");
  //legalizer->SaveDEFFile("circuit.def", def_file);
   */



  Time = clock() - Time;
  std::cout << "Execution time " << (float)Time/CLOCKS_PER_SEC << "s.\n";

  return 0;
}