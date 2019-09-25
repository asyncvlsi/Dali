//
// Created by Yihang Yang on 9/24/19.
//

/****
 * This is a stand-alone hpwl calculator, it will report:
 * 1. HPWL
 *    pin-to-pin
 *    center-to-center
 * 2. Weighted HPWL, a weight file needs to be provided
 *    pin-to-pin
 *    center-to-center
 * ****/

#include <iostream>
#include "common/global.h"
#include "circuit.h"

VerboseLevel globalVerboseLevel = LOG_CRITICAL;

void ReportUsage();

int main(int argc, char *argv[]) {
  if (argc < 4) {
    ReportUsage();
    return 1;
  }
  std::string lef_file_name;
  std::string def_file_name;
  std::string str_grid_value_x, str_grid_value_y;
  std::string str_verbose_level;

  for( int i = 1; i < argc; ) {
    std::string arg(argv[i++]);
    if (arg == "-lef" && i < argc) {
      lef_file_name = std::string(argv[i++]);
    } else if (arg == "-def" && i < argc) {
      def_file_name = std::string(argv[i++]);
    } else if (arg == "-grid" && i < argc) {
      str_grid_value_x = std::string(argv[i++]);
      str_grid_value_y = std::string(argv[i++]);
    } else if (arg == "-v" && i < argc) {
      str_verbose_level = std::string(argv[i++]);
    } else {
      std::cout << "Unknown option for readArgs\n";
    }
  }

  Circuit circuit;
  //circuit.SetGridValue();
  circuit.ReadLefFile(lef_file_name);
  circuit.ReadDefFile(def_file_name);
  return 0;
}

void ReportUsage() {
  std::cout << "Usage: hpwl\n"
               " -lef <file.lef>\n"
               " -def <file.def>\n"
               " -grid grid_value_x grid_value_y (optional? default TBD)\n"
               " -v verbosity_level (optional, 0-5, default 0)\n"
               "(order does not matter)"
            << std::endl;
}