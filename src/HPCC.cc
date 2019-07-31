//
// Created by Yihang Yang on 2019-05-14.
//

#include <iostream>
#include <vector>
#include <ctime>
#include "circuit/circuit.h"

int main() {
  time_t Time = clock();
  Circuit circuit;

  std::string lefFileName = "out_1K.lef";
  std::string defFileName = "out_1K.def";
  circuit.ReadLefFile(lefFileName);
  //circuit.ReadDefFile(defFileName);

  circuit.ReportBlockTypeList();
  //circuit.ReportBlockTypeMap();
  //circuit.ReportBlockList();
  //circuit.ReportBlockMap();
  //circuit.ReportNetList();
  //circuit.ReportNetMap();

  Time = clock() - Time;
  std::cout << "Execution time " << (float)Time/CLOCKS_PER_SEC << "s.\n";

  return 0;
}