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
 * This is a converter, which can convert a LEFDEF file to bookshelf files.
 * ****/

#include <iostream>

#include "dali/circuit/circuit.h"
#include "dali/common/logging.h"

void ReportUsage();

using namespace dali;

int main(int argc, char *argv[]) {
  if (argc != 7) {
    ReportUsage();
    return 1;
  }
  std::string lef_file_name;
  std::string def_file_name;
  std::string book_shelf_out;

  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-lef" && i < argc) {
      lef_file_name = std::string(argv[i++]);
    } else if (arg == "-def" && i < argc) {
      def_file_name = std::string(argv[i++]);
    } else if ((arg == "-bs" || arg == "-bookshelf") && i < argc) {
      book_shelf_out = std::string(argv[i++]);
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
  circuit.SaveBookshelfNode(book_shelf_out + ".nodes");
  circuit.SaveBookshelfNet(book_shelf_out + ".nets");
  circuit.SaveBookshelfPl(book_shelf_out + ".pl");
  circuit.SaveBookshelfScl(book_shelf_out + ".scl");
  circuit.SaveBookshelfWts(book_shelf_out + ".wts");
  circuit.SaveBookshelfAux(book_shelf_out);

  return 0;
}

void ReportUsage() {
  BOOST_LOG_TRIVIAL(info)
    << "\033[0;36m"
    << "Usage: lefdef2bookshelf\n"
    << " -lef <file.lef>\n"
    << " -def <file.def>\n"
    << " -bs/-bookshelf <output> (.aux .nets .nodes .pl .scl .wts file will be created)\n"
    << "(order does not matter)"
    << "\033[0m\n";
}