/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/

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

#include "dali/circuit/circuit.h"

void ReportUsage();

using namespace dali;

int main(int argc, char *argv[]) {
  if (argc != 5) {
    ReportUsage();
    return 1;
  }
  InitLogging();
  std::string lef_file_name;
  std::string def_file_name;

  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-lef" && i < argc) {
      lef_file_name = std::string(argv[i++]);
    } else if (arg == "-def" && i < argc) {
      def_file_name = std::string(argv[i++]);
    } else {
      BOOST_LOG_TRIVIAL(info) << "Unknown command line option: "
                              << argv[i] << "\n";
      return 1;
    }
  }

  phydb::PhyDB phy_db;
  phy_db.ReadLef(lef_file_name);
  phy_db.ReadDef(def_file_name);
  Circuit circuit;
  circuit.InitializeFromPhyDB(&phy_db);
  // might need to print out some circuit info here
  double hpwl_x = circuit.WeightedHPWLX();
  double hpwl_y = circuit.WeightedHPWLY();
  BOOST_LOG_TRIVIAL(info)
    << "Pin-to-Pin HPWL\n"
    << "  HPWL in the x direction: " << hpwl_x << "\n"
    << "  HPWL in the y direction: " << hpwl_y << "\n"
    << "  HPWL total:              " << hpwl_x + hpwl_y
    << "\n";
  hpwl_x = circuit.HPWLCtoCX();
  hpwl_y = circuit.HPWLCtoCY();
  BOOST_LOG_TRIVIAL(info)
    << "Center-to-Center HPWL\n"
    << "  HPWL in the x direction: " << hpwl_x << "\n"
    << "  HPWL in the y direction: " << hpwl_y << "\n"
    << "  HPWL total:              " << hpwl_x + hpwl_y
    << "\n";
  return 0;
}

void ReportUsage() {
  BOOST_LOG_TRIVIAL(info)
    << "\033[0;36m"
    << "Usage: hpwl\n"
    << " -lef <file.lef>\n"
    << " -def <file.def>\n"
    << "(order does not matter)"
    << "\033[0m\n";
}