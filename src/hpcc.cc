//
// Created by Yihang Yang on 9/3/19.
//

#include <iostream>
#include "common/global.h"

VerboseLevel globalVerboseLevel = LOG_CRITICAL;

void ReportUsage();

int main(int argc, char *argv[]) {
  ReportUsage();
  return 0;
}

void ReportUsage() {
  std::cout << "Usage: hpcc\n"
               " -lef <file.lef>\n"
               " -def <file.def>\n"
               " -grid grid_value_x grid_value_y (optional? default TBD)\n"
               " -v verbosity_level (optional, 0-5, default 0)\n"
               "(order does not matter)"
               << std::endl;
}