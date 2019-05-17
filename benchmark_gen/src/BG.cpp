//
// Created by Yihang Yang on 2019-05-14.
//

#include <iostream>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <vector>
#include "circuit.cpp"

int main() {
  clock_t T=clock();
  circuit_t circuit1;
  circuit1.random_gen_node_list(20, 80, 20, 40, 100);
  circuit1.random_gen_net_list();

  circuit1.write_node_file("sample1.nodes");
  circuit1.write_net_file("sample1.nets");

  T = clock() - T;
  std::cout << "Process complete. Execution time : " << (float)T/CLOCKS_PER_SEC << " s\n";
  return 0;
}