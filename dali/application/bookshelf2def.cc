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
 * This can extract location of cells from a bookshelf .pl file, and generate a new DEF file with these locations.
 * ****/
#include <iostream>

#include "dali/circuit/circuit.h"
#include "dali/common/logging.h"

void ReportUsage();

using namespace dali;

int main(int argc, char *argv[]) {
  /*if (argc != 9) {
    ReportUsage();
    return 1;
  }*/
  std::string lef_file_name;
  std::string def_file_name;
  std::string pl_file_name;
  std::string out_def_name;
  std::string str_x_grid;
  std::string str_y_grid;

  double x_grid = 0;
  double y_grid = 0;

  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-lef" && i < argc) {
      lef_file_name = std::string(argv[i++]);
    } else if (arg == "-def" && i < argc) {
      def_file_name = std::string(argv[i++]);
    } else if (arg == "-pl" && i < argc) {
      pl_file_name = std::string(argv[i++]);
    } else if ((arg == "-g" || arg == "-grid") && i < argc) {
      str_x_grid = std::string(argv[i++]);
      str_y_grid = std::string(argv[i++]);
      try {
        x_grid = std::stod(str_x_grid);
        y_grid = std::stod(str_y_grid);
      } catch (...) {
        BOOST_LOG_TRIVIAL(info) << "Invalid input files!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-o" && i < argc) {
      out_def_name = std::string(argv[i++]) + ".def";
    } else {
      BOOST_LOG_TRIVIAL(info)
        << "Unknown command line option: " << argv[i] << "\n";
      return 1;
    }
  }

  Circuit circuit;
  circuit.SetGridValue(x_grid, y_grid);
  DaliExpects(false, "Fix read LEF/DEF");
  //circuit.ReadLefFile(lef_file_name);
  //circuit.ReadDefFile(def_file_name);
  circuit.LoadBookshelfPl(pl_file_name);
  circuit.SaveDefFile(out_def_name, "", def_file_name, 1, 1, 1, 1);

  return 0;
}

void ReportUsage() {
  BOOST_LOG_TRIVIAL(info)
    << "\033[0;36m"
    << "Usage: bookshelf2def\n"
    << " -lef <file.lef>\n"
    << " -def <file.def>\n"
    << " -pl  <file.pl>\n"
    << " -g/-grid grid_value_x grid_value_y\n"
    << " -o   <out_name>.def\n"
    << "(order does not matter)"
    << "\033[0m\n";
}