//
// Created by Yihang Yang on 1/17/20.
//

/****
 * This is a converter, which can convert a LEFDEF file to bookshelf files.
 * ****/

#include <iostream>

#include "circuit.h"
#include "common/global.h"
//TODO: fix this
VerboseLevel globalVerboseLevel = LOG_INFO;

void ReportUsage();

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
      BOOST_LOG_TRIVIAL(info)   << "Unknown command line option: " << argv[i] << "\n";
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
  circuit.SaveBookshelfNode(book_shelf_out + ".nodes");
  circuit.SaveBookshelfNet(book_shelf_out + ".nets");
  circuit.SaveBookshelfPl(book_shelf_out + ".pl");
  circuit.SaveBookshelfScl(book_shelf_out + ".scl");
  circuit.SaveBookshelfWts(book_shelf_out + ".wts");
  circuit.SaveBookshelfAux(book_shelf_out);

  return 0;
}

void ReportUsage() {
  BOOST_LOG_TRIVIAL(info)   << "\033[0;36m"
            << "Usage: lefdef2bookshelf\n"
            << " -lef <file.lef>\n"
            << " -def <file.def>\n"
            << " -bs/-bookshelf <output> (.aux .nets .nodes .pl .scl .wts file will be created)\n"
            << "(order does not matter)"
            << "\033[0m\n";
}