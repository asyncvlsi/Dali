//
// Created by Yihang Yang on 2019-05-14.
//

#include <iostream>
#include <vector>
#include <ctime>
#include "circuit.hpp"

int main() {
  time_t Time = clock();
  circuit_t circuit;
  circuit.read_nodes_file("../test/layout.nodes");
  circuit.read_nets_file("../test/layout.nets");

  //circuit.set_filling_rate();
  //circuit.set_boundary();

  //diffusion_limited_aggregation_placer placer0(circuit);
  //placer0.place();
  //placer0.report_result();
  //placer0.clear();

  //circuit.write_pl_solution("./test/sample1_solution.pl");
  //circuit.write_node_terminal("terminal.txt", "nodes.txt");
  //circuit.write_pl_anchor_solution("./test/sample1_solution.pl");
  //circuit.write_anchor_terminal("terminal.txt", "nodes.txt");

  Time = clock() - Time;
  std::cout << "Execution time " << (float)Time/CLOCKS_PER_SEC << "s.\n";

  return 0;
}