//
// Created by Yihang Yang on 3/5/20.
//

#include <ctime>

#include <iostream>
#include <vector>

#include <galois//Galois.h>

#include "circuit.h"
#include "placer.h"

#define USE_DB_PARSER 1

int main(int argc, char *argv[]) {
  int num_of_thread = 2;
  galois::SharedMemSys G;
  galois::preAlloc(num_of_thread * 2);
  galois::setActiveThreads(num_of_thread);

  Circuit circuit;

  time_t Time = clock();

  std::string lef_file_name = "out.lef";
  std::string def_file_name = "out.def";
  std::string cel_file_name = "out.cell";
  std::string out_file_name = "circuit";
  int gc = 30;
  int it_num = 200;

  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-gc" && i < argc) { // grid cap
      std::string str_gc = std::string(argv[i++]);
      try {
        gc = std::stoi(str_gc);
      } catch (...) {
        BOOST_LOG_TRIVIAL(info)   << "Invalid input!\n";
        return 1;
      }
    } else if (arg == "-itnum" && i < argc) { // iteration number
      std::string str_itnum = std::string(argv[i++]);
      try {
        it_num = std::stoi(str_itnum);
      } catch (...) {
        BOOST_LOG_TRIVIAL(info)   << "Invalid input!\n";
        return 1;
      }
    }
  }

#if USE_DB_PARSER
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

  circuit.ReadCellFile(cel_file_name);

  BOOST_LOG_TRIVIAL(info)   << "File loading complete, time: " << double(clock() - Time) / CLOCKS_PER_SEC << " s\n";
  BOOST_LOG_TRIVIAL(info)  <<"  Average white space utility: %.4f\n", circuit.WhiteSpaceUsage());
  circuit.ReportBriefSummary();
  //circuit.ReportBlockType();
  circuit.ReportHPWL();

  WellPlaceFlow well_place_flow;
  well_place_flow.SetInputCircuit(&circuit);
  well_place_flow.SetGridCapacity(gc);
  well_place_flow.SetIteration(it_num);
  well_place_flow.SetBoundaryDef();
  well_place_flow.SetFillingRate(0.67);
  well_place_flow.ReportBoundaries();
  well_place_flow.StartPlacement();
  well_place_flow.GenMATLABTable("gb_result.txt");

  well_place_flow.EmitDEFWellFile(out_file_name, 1);
  circuit.SaveDefFile(out_file_name, "", def_file_name, 1, 1, 2, 1);

  Time = clock() - Time;
  BOOST_LOG_TRIVIAL(info)   << "Execution time " << double(Time) / CLOCKS_PER_SEC << "s.\n";

  return 0;
}
