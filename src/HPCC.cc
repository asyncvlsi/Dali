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

  std::string lefFileName = "out_1K.lef";
  std::string defFileName = "out_1K.def";
  circuit.ReadLefFile(lefFileName);
  circuit.ReadDefFile(defFileName);

  //circuit.ReportBlockTypeList();
  //circuit.ReportBlockTypeMap();
  //circuit.ReportBlockList();
  //circuit.ReportBlockMap();
  //circuit.ReportNetList();
  //circuit.ReportNetMap();

  Placer *gb_placer = new GPSimPL;
  gb_placer->SetInputCircuit(&circuit);

  //gb_placer->SetFillingRate(1/1.4);
  //gb_placer->SetAspectRatio(1);
  //gb_placer->SetBoundaryAuto();

  gb_placer->SetBoundaryDef();
  gb_placer->ReportBoundaries();
  gb_placer->StartPlacement();
  gb_placer->GenMATLABScript("gb_placement_result.m");

  Placer *legalizer = new TetrisLegalizer;
  legalizer->TakeOver(gb_placer);
  legalizer->StartPlacement();
  legalizer->GenMATLABScript("legalizer_result.m");


  Time = clock() - Time;
  std::cout << "Execution time " << (float)Time/CLOCKS_PER_SEC << "s.\n";

  return 0;
}