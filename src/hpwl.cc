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
  if (argc != 5) {
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
      std::cout << "Unknown command line option: " << argv[i] << "\n";
      return 1;
    }
  }

  Circuit circuit;
  circuit.ReadLefFile(lef_file_name);
  circuit.ReadDefFile(def_file_name);
  // might need to print out some circuit info here
  double hpwl_x = circuit.HPWLX();
  double hpwl_y = circuit.HPWLY();
  std::cout << "Pin-to-Pin HPWL\n"
            << "  HPWL in the x direction: " << hpwl_x << "\n"
            << "  HPWL in the y direction: " << hpwl_y << "\n"
            << "  HPWL total:              " << hpwl_x + hpwl_y
            << "\n";
  hpwl_x = circuit.HPWLCtoCX();
  hpwl_y = circuit.HPWLCtoCY();
  std::cout << "Center-to-Center HPWL\n"
            << "  HPWL in the x direction: " << hpwl_x << "\n"
            << "  HPWL in the y direction: " << hpwl_y << "\n"
            << "  HPWL total:              " << hpwl_x + hpwl_y
            << "\n";
  return 0;
}

void ReportUsage() {
  std::cout << "Usage: hpwl\n"
            << " -lef <file.lef>\n"
            << " -def <file.def>\n"
            << "(order does not matter)"
            << "\n";
}