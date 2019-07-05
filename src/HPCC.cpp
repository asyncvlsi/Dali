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
  /****adaptec1****/
  /*
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
  */

  /****layout****/
  /*
  if (!circuit.read_nodes_file("../test/layout.nodes")) {
    //circuit.report_block_list();
    //circuit.report_block_map();
    return 1;
  }
  if (!circuit.read_nets_file("../test/layout.nets")) {
    //circuit.report_net_list();
    //circuit.report_net_map();
    return 1;
  }
  if (!circuit.read_pl_file("../test/layout.pl")) {
    //circuit.report_block_list();
    return 1;
  }
  */

  /****LEF/DEF****/
  if (!circuit.read_lef_file("out.lef")) {
    //circuit.report_blockType_list();
    //circuit.report_blockType_map();
    return 1;
  }
  /*if (!circuit.read_def_file("out.def")) {
    //circuit.report_net_list();
    //circuit.report_net_map();
    return 1;
  }*/
  /****debug case****/
  /*
  if (!circuit.read_nodes_file("nodes.txt")) {
    //circuit.report_block_list();
    //circuit.report_block_map();
    return 1;
  }
  if (!circuit.read_nets_file("nets.txt")) {
    //circuit.report_net_list();
    //circuit.report_net_map();
    return 1;
  }
  */

  /*
  std::cout << circuit.tot_movable_num_real_time() << " movable cells\n";
  std::cout << circuit.block_list.size() << " total cells\n";

  placer_t *ptr_placer = new placer_al_t;
  ptr_placer->set_space_block_ratio(1.5);
  ptr_placer->set_aspect_ratio(1);
  std::cout << ptr_placer->space_block_ratio() << " " << ptr_placer->filling_rate() << " " << ptr_placer->aspect_ratio() << "\n";

  ptr_placer->set_input_circuit(&circuit);
  ptr_placer->auto_set_boundaries(); // set boundary for layout
  //ptr_placer->set_boundary(459,11151,459,11139); // set boundary for adaptec1
  ptr_placer->report_boundaries();
  ptr_placer->start_placement();
  ptr_placer->report_placement_result();
  ptr_placer->gen_matlab_disp_file(); // generate matlab file for layout
  //ptr_placer->write_node_terminal(); // generate a data file for adaptec1
  //ptr_placer->save_DEF();

  delete ptr_placer;
   */

  Time = clock() - Time;
  std::cout << "Execution time " << (float)Time/CLOCKS_PER_SEC << "s.\n";

  return 0;
}