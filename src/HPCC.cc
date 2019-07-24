//
// Created by Yihang Yang on 2019-05-14.
//

#include <iostream>
#include <vector>
#include <ctime>
#include "circuit/circuit.h"
#include "placer.h"
#include "DLA/placerdla.h"
#include "AL/placeral.h"

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

  //std::string lefFileName = "../test/out_1K/3m/out_1K.lef";
  //std::string defFileName = "../test/out_1K/3m/out_1K.def";
  std::string lefFileName = "out_1K.lef";
  std::string defFileName = "out_1K.def";
  if (!circuit.read_lef_file(lefFileName)) {
    return 1;
  }
  //circuit.report_blockType_list();
  //circuit.report_blockType_map();
  if (!circuit.read_def_file(defFileName)) {
    return 1;
  }

  //circuit.report_block_list();
  //circuit.report_block_map();
  //circuit.report_net_list();
  //circuit.report_net_map();
  /****debug case****/
  /*
  if (!circuit.read_nodes_file("nnnodes245")) {
    //circuit.report_block_list();
    //circuit.report_block_map();
    return 1;
  }
  if (!circuit.read_nets_file("nnnets245")) {
    //circuit.report_net_list();
    //circuit.report_net_map();
    return 1;
  }
   */

  std::cout << circuit.tot_movable_num_real_time() << " movable cells\n";
  std::cout << circuit.blockList.size() << " total cells\n";

  placer_t *placer = new placer_al_t;
  placer->set_space_block_ratio(1.4);
  placer->set_aspect_ratio(1);
  std::cout << placer->space_block_ratio() << " " << placer->filling_rate() << " " << placer->aspect_ratio() << "\n";
  std::cout << "average width and height: " << circuit.ave_width() << " " << circuit.ave_height() << " " << circuit.ave_width() + circuit.ave_height() << "\n";
  placer->set_input_circuit(&circuit);
  //placer->set_boundary(1149,1718,2664,2667); // debug case
  //placer->auto_set_boundaries(); // set boundary for layout
  //placer->set_boundary(459,11151,459,11139); // set boundary for adaptec1
  placer->set_boundary(circuit.def_left,circuit.def_right,circuit.def_bottom,circuit.def_top); // set boundary for lef/def
  placer->report_boundaries();
  placer->start_placement();
  placer->report_placement_result();
  placer->gen_matlab_disp_file("al_result.txt"); // generate matlab file for layout
  //placer->write_node_terminal(); // generate a data file for adaptec1
  delete placer;
  //circuit.save_DEF("circuit_dla.def", defFileName);

  Time = clock() - Time;
  std::cout << "Execution time " << (float)Time/CLOCKS_PER_SEC << "s.\n";

  return 0;
}