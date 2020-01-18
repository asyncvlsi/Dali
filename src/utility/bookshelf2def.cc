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
  if (argc != 9) {
    ReportUsage();
    return 1;
  }
  std::string lef_file_name;
  std::string def_file_name;
  std::string pl_file_name;
  std::string out_def_name;

  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-lef" && i < argc) {
      lef_file_name = std::string(argv[i++]);
    } else if (arg == "-def" && i < argc) {
      def_file_name = std::string(argv[i++]);
    } else if (arg == "-pl" && i < argc) {
      pl_file_name = std::string(argv[i++]);
    } else if (arg == "-o" && i < argc) {
      out_def_name = std::string(argv[i++]) + ".def";
    } else {
      std::cout << "Unknown command line option: " << argv[i] << "\n";
      return 1;
    }
  }

  Circuit circuit;
#ifdef USE_OPENDB
  odb::dbDatabase *db = odb::dbDatabase::create();
  std::vector<std::string> defFileVec;
  defFileVec.push_back(def_file_name);
  odb_read_lef(db, lef_file_name.c_str());
  odb_read_def(db, defFileVec);
  circuit.InitializeFromDB(db);
#else
  circuit.ReadLefFile(lef_file_name);
  circuit.ReadDefFile(def_file_name);
#endif
  // might need to print out some circuit info here
  circuit.LoadBookshelfPl(pl_file_name);
  circuit.SaveDefFile(out_def_name, def_file_name);

  return 0;
}

void ReportUsage() {
  std::cout << "\033[0;36m"
            << "Usage: bookshelf2def\n"
            << " -lef <file.lef>\n"
            << " -def <file.def>\n"
            << " -pl  <file.pl>\n"
            << " -o   <out_name>.def\n"
            << "(order does not matter)"
            << "\033[0m\n";
}