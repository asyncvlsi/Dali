//
// Created by Yihang Yang on 2019-05-14.
//

#include <iostream>
#include <vector>
#include <ctime>
#include "circuit/circuit.h"
#include "placer/placer.h"
#include "placer/globalPlacer/GPSimPL.h"

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

  Placer *placer = new GPSimPL;
  placer->SetInputCircuit(&circuit);
  placer->SetAspectRatio(1);
  placer->SetBoundaryAuto();
  placer->StartPlacement();
  placer->GenMATLABScript();

  Time = clock() - Time;
  std::cout << "Execution time " << (float)Time/CLOCKS_PER_SEC << "s.\n";

  return 0;
}