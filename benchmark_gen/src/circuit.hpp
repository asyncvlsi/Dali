//
// Created by Yihang Yang on 2019-03-26.
//

#ifndef SLDP_CIRCUIT_HPP
#define SLDP_CIRCUIT_HPP

#include <vector>
#include <queue>
#include <set>
#include "node_net.hpp"

typedef struct  {
  size_t pin;
  float weight;
} weight_tuple;
// weight tuple, include the pin number and the corresponding weight
// for more information about the sparse matrix format, see the documents

class circuit_t {
public:
  circuit_t();
  int LEFT, RIGHT, BOTTOM, TOP;
  float TARGET_FILLING_RATE;
  std::vector< node_t > Nodelist;
  std::vector< net_t > Netlist;

  void random_gen_node_list(int w_lower, int w_upper, int h_lower, int h_upper, int cell_num);
  void random_gen_net_list();

  bool write_net_file(std::string const &NameOfFile);
  bool write_node_file(std::string const &NameOfFile);
  bool write_pl_file(std::string const &NameOfFile);
  bool write_node_terminal(std::string const &NameOfFile="terminal.txt", std::string const &NameOfFile1="nodes.txt");
};

#endif //SLDP_CIRCUIT_HPP
