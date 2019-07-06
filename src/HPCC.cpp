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
  /*
  if (!circuit.read_lef_file("out_1K.lef")) {
    return 1;
  }
  //circuit.report_blockType_list();
  //circuit.report_blockType_map();
  if (!circuit.read_def_file("out_1K.def")) {
    return 1;
  }
  */
  //circuit.report_block_list();
  //circuit.report_block_map();
  //circuit.report_net_list();
  //circuit.report_net_map();
  /****debug case****/

  if (!circuit.read_nodes_file("nnnodes0")) {
    //circuit.report_block_list();
    //circuit.report_block_map();
    return 1;
  }
  if (!circuit.read_nets_file("nnnets0")) {
    //circuit.report_net_list();
    //circuit.report_net_map();
    return 1;
  }


  std::cout << circuit.tot_movable_num_real_time() << " movable cells\n";
  std::cout << circuit.block_list.size() << " total cells\n";

  placer_al_t placer;
  placer.set_space_block_ratio(1.6);
  placer.set_aspect_ratio(1);
  std::cout << placer.space_block_ratio() << " " << placer.filling_rate() << " " << placer.aspect_ratio() << "\n";
  std::cout << "average width and height: " << circuit.ave_width() << " " << circuit.ave_height() << " " << circuit.ave_width() + circuit.ave_height() << "\n";
  placer.set_input_circuit(&circuit);
  placer.set_boundary(0,360,0,3000); // debug case
  //placer.auto_set_boundaries(); // set boundary for layout
  //placer.set_boundary(459,11151,459,11139); // set boundary for adaptec1
  //placer.set_boundary(circuit.def_left,circuit.def_right,circuit.def_bottom,circuit.def_top); // set boundary for lef/def
  placer.report_boundaries();
  placer.start_placement();
  placer.report_placement_result();
  placer.gen_matlab_disp_file(); // generate matlab file for layout
  //placer.write_node_terminal(); // generate a data file for adaptec1
  //circuit.save_DEF();


  Time = clock() - Time;
  std::cout << "Execution time " << (float)Time/CLOCKS_PER_SEC << "s.\n";

  return 0;
}