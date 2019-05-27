//
// Created by Yihang Yang on 2019-05-14.
//

#include <iostream>
#include <vector>
#include <ctime>
#include "circuit.hpp"
#include "placer.hpp"

int main() {
  time_t Time = clock();
  circuit_t circuit;
  if (!circuit.read_nodes_file("../test/layout.nodes")) {
    return 1;
  }
  //circuit.report_block_list();
  //circuit.report_block_map();
  if (!circuit.read_nets_file("../test/layout.nets")) {
    return 1;
  }
  //circuit.report_net_list();
  //circuit.report_net_map();
  if (!circuit.read_pl_file("../test/layout.pl")) {
    return 1;
  }
  //circuit.report_block_list();

  placer_t placer;
  placer.set_space_block_ratio(2);
  placer.set_aspect_ratio(2);
  std::cout << placer.space_block_ratio() << " " << placer.filling_rate() << " " << placer.aspect_ratio() << "\n";

  placer.set_input_circuit(circuit);
  placer.auto_set_boundaries();
  placer.report_boundaries();

  //circuit.write_pl_solution("./test/sample1_solution.pl");
  //circuit.write_node_terminal("terminal.txt", "nodes.txt");
  //circuit.write_pl_anchor_solution("./test/sample1_solution.pl");
  //circuit.write_anchor_terminal("terminal.txt", "nodes.txt");

  Time = clock() - Time;
  std::cout << "Execution time " << (float)Time/CLOCKS_PER_SEC << "s.\n";

  return 0;
}