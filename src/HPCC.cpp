//
// Created by Yihang Yang on 2019-05-14.
//

#include <iostream>
#include <vector>
#include <ctime>
#include "circuit/circuit.hpp"
#include "placer.hpp"
#include "DLA/placerdla.hpp"
#include "AL/placeral.hpp"

int main() {
  time_t Time = clock();
  circuit_t circuit;
  if (!circuit.read_nodes_file("../test/adaptec1/adaptec1.nodes")) {
    //circuit.report_block_list();
    //circuit.report_block_map();
    return 1;
  }
  if (!circuit.read_nets_file("../test/adaptec1/adaptec1.nets")) {
    //circuit.report_net_list();
    //circuit.report_net_map();
    return 1;
  }
  //if (!circuit.read_pl_file("../test/layout.pl")) {
    //circuit.report_block_list();
    //return 1;
  //}

  placer_t *placer = new placer_al_t;
  placer->set_space_block_ratio(1.5);
  placer->set_aspect_ratio(1);
  std::cout << placer->space_block_ratio() << " " << placer->filling_rate() << " " << placer->aspect_ratio() << "\n";

  placer->set_input_circuit(&circuit);
  placer->auto_set_boundaries();
  placer->report_boundaries();
  placer->start_placement();
  placer->report_placement_result();
  placer->gen_matlab_disp_file();

  //circuit.write_pl_solution("./test/sample1_solution.pl");

  Time = clock() - Time;
  std::cout << "Execution time " << (float)Time/CLOCKS_PER_SEC << "s.\n";

  return 0;
}