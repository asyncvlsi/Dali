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
#include "circuit.h"
#include "common/global.h"

VerboseLevel globalVerboseLevel = LOG_INFO;

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
    } else {
      std::cout << "Unknown option for readArgs\n";
    }
  }

  if ((lef_file_name.empty())||(def_file_name.empty())) {
    std::cout << "Invalid input!\n";
    ReportUsage();
    return 1;
  }

  Circuit circuit;
  circuit.ReadLefFile(lef_file_name);
  circuit.ReadDefFile(def_file_name);
  // might need to print out some circuit info here
  std::cout << "Pin-to-Pin HPWL\n";
  std::cout << "  HPWL in the x direction: " << circuit.HPWLX() << std::endl;
  std::cout << "  HPWL in the y direction: " << circuit.HPWLY() << std::endl;
  std::cout << "  HPWL total:              " << circuit.HPWL()  << std::endl;
  std::cout << "Center-to-Center HPWL\n";
  std::cout << "  HPWL in the x direction: " << circuit.HPWLCtoCX() << std::endl;
  std::cout << "  HPWL in the y direction: " << circuit.HPWLCtoCY() << std::endl;
  std::cout << "  HPWL total:              " << circuit.HPWLCtoC()  << std::endl;
  return 0;
}

void ReportUsage() {
  std::cout << "Usage: hpwl\n"
               " -lef <file.lef>\n"
               " -def <file.def>\n"
               "(order does not matter)"
            << std::endl;
}