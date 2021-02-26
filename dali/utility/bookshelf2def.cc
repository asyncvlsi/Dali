//
// Created by Yihang Yang on 1/18/20.
//

/****
 * This can extract location of cells from a bookshelf .pl file, and generate a new DEF file with these locations.
 * ****/

#include <iostream>

#include "circuit.h"
#include "common/global.h"

VerboseLevel globalVerboseLevel = LOG_INFO;

void ReportUsage();

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
  bool use_db = false;

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
        BOOST_LOG_TRIVIAL(info)   << "Invalid input files!\n";
        ReportUsage();
        return 1;
      }
    } else if (arg == "-o" && i < argc) {
      out_def_name = std::string(argv[i++]) + ".def";
    } else if (arg == "-db" && i < argc) {
      use_db = true;
    } else {
      BOOST_LOG_TRIVIAL(info)   << "Unknown command line option: " << argv[i] << "\n";
      return 1;
    }
  }

  Circuit circuit;
  if (use_db) {
    odb::dbDatabase *db = odb::dbDatabase::create();
    std::vector<std::string> defFileVec;
    defFileVec.push_back(def_file_name);
    odb_read_lef(db, lef_file_name.c_str());
    odb_read_def(db, defFileVec);
    circuit.InitializeFromDB(db);
  } else {
    circuit.SetGridValue(x_grid, y_grid);
    circuit.ReadLefFile(lef_file_name);
    circuit.ReadDefFile(def_file_name);
  }
  circuit.LoadBookshelfPl(pl_file_name);
  circuit.SaveDefFile(out_def_name, def_file_name);

  return 0;
}

void ReportUsage() {
  BOOST_LOG_TRIVIAL(info)   << "\033[0;36m"
            << "Usage: bookshelf2def\n"
            << " -lef <file.lef>\n"
            << " -def <file.def>\n"
            << " -pl  <file.pl>\n"
            << " -g/-grid grid_value_x grid_value_y\n"
            << " -o   <out_name>.def\n"
            << " -db (optional, use Naive parser by default)\n"
            << "(order does not matter)"
            << "\033[0m\n";
}