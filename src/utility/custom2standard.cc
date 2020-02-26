//
// Created by Yihang Yang on 2/25/20.
//

#include <fstream>
#include <iostream>

#include "circuit.h"
#include "common/global.h"
#include "common/misc.h"

VerboseLevel globalVerboseLevel = LOG_INFO;

void ReportUsage();

int main(int argc, char *argv[]) {
  if (argc != 5) {
    ReportUsage();
    return 1;
  }
  std::string lef_file_name;
  std::string out_file_name;

  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-lef" && i < argc) {
      lef_file_name = std::string(argv[i++]);
    } else if (arg == "-o" && i < argc) {
      out_file_name = std::string(argv[i++]);
    } else {
      std::cout << "Unknown command line option: " << argv[i] << "\n";
      return 1;
    }
  }

  Circuit circuit;
#ifdef USE_OPENDB
  odb::dbDatabase *db = odb::dbDatabase::create();
  std::vector<std::string> defFileVec;
  odb_read_lef(db, lef_file_name.c_str());
  circuit.InitializeFromDB(db);
#else
  circuit.ReadLefFile(lef_file_name);
#endif

  double max_height = 0;
  for (auto &pair: circuit.block_type_map) {
    if (pair.second->Height() > max_height) {
      max_height = pair.second->Height();
    }
  }
  max_height *= circuit.GetGridValueY();

  std::ifstream ist;
  ist.open(lef_file_name.c_str());
  Assert(ist.is_open(), "Cannot open file " + lef_file_name);

  std::ofstream ost;
  ost.open((out_file_name + ".lef").c_str());
  Assert(ost.is_open(), "Cannot open file " + out_file_name);

  std::string line;
  std::vector<std::string> line_field;

  bool is_core_site_found = false;
  std::string core_site_name;

  while (!ist.eof()) {
    getline(ist, line);
    if (!is_core_site_found && line.find("SITE") != std::string::npos) {
      ost << line << "\n";
      Circuit::StrSplit(line, line_field);
      core_site_name = line_field[1];
      do {
        getline(ist, line);
        if (line.find("SIZE") != std::string::npos) {
          Circuit::StrSplit(line, line_field);
          ost << "    " << line_field[0] << " " << line_field[1] << " " << line_field[2] << " " << max_height << " ;\n";
        } else {
          ost << line << "\n";
        }
      } while (!ist.eof() && line.find("END") == std::string::npos);
      is_core_site_found = true;
      continue;
    }

    if (is_core_site_found && line.find("SIZE") != std::string::npos) {
      Circuit::StrSplit(line, line_field);
      ost << "    " << line_field[0] << " " << line_field[1] << " " << line_field[2] << " " << max_height << " ;\n";
      continue;
    }

    ost << line << "\n";
  }

  return 0;
}

void ReportUsage() {
  std::cout << "\033[0;36m"
            << "Usage: custom2standard\n"
            << " -lef <file.lef>\n"
            << " -o   <file>.lef\n"
            << "(order does not matter)"
            << "\033[0m\n";
}