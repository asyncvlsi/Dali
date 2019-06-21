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
  if (!circuit.read_pl_file("../test/adaptec1/adaptec1.pl")) {
    //circuit.report_block_list();
    return 1;
  }

  std::cout << circuit.tot_movable_num_real_time() << " movable cells\n";
  std::cout << circuit.block_list.size() << " total cells\n";

  placer_t *placer = new placer_al_t;
  placer->set_space_block_ratio(1.5);
  placer->set_aspect_ratio(1);
  std::cout << placer->space_block_ratio() << " " << placer->filling_rate() << " " << placer->aspect_ratio() << "\n";

  placer->set_input_circuit(&circuit);
  //placer->auto_set_boundaries();
  placer->set_boundary(459,11151,459,11139);
  placer->report_boundaries();
  placer->start_placement();
  placer->report_placement_result();
  placer->write_node_terminal();


  Time = clock() - Time;
  std::cout << "Execution time " << (float)Time/CLOCKS_PER_SEC << "s.\n";

  return 0;
}