//
// Created by Yihang Yang on 3/5/20.
//

#include <ctime>

#include <iostream>
#include <vector>

//#include <galois/Galois.h>

#include "circuit.h"
#include "common/logging.h"
#include "common/si2lefdef.h"
#include "placer.h"

using namespace dali;

int main(int argc, char *argv[]) {
  init_logging(boost::log::trivial::info);
  int num_of_thread = 1;
  omp_set_num_threads(num_of_thread);
  
  //galois::SharedMemSys G;
  //galois::preAlloc(num_of_thread * 2);
  //galois::setActiveThreads(num_of_thread);

  dali::Circuit circuit;

  time_t Time = clock();

  std::string lef_file_name = "processor1000.lef";
  std::string def_file_name = "processor1000.def";
  std::string cel_file_name = "processor1000.cell";
  std::string out_file_name = "circuit";

  // load LEF/DEF
  readLef(lef_file_name, circuit);
  readDef(def_file_name, circuit);

  circuit.ReadCellFile(cel_file_name);

  BOOST_LOG_TRIVIAL(info)   << "File loading complete, time: " << double(clock() - Time) / CLOCKS_PER_SEC << " s\n";
  BOOST_LOG_TRIVIAL(info)  <<"  Average white space utility: " << circuit.WhiteSpaceUsage() << "\n";
  circuit.ReportBriefSummary();
  //circuit.ReportBlockType();
  circuit.ReportHPWL();

  dali::WellPlaceFlow well_place_flow;
  well_place_flow.SetInputCircuit(&circuit);
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
