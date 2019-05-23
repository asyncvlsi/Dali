//
// Created by Yihang Yang on 2019-03-26.
//

#ifndef HPCC_CIRCUIT_HPP
#define HPCC_CIRCUIT_HPP

#include <vector>
#include <queue>
#include <set>
#include "circuit_node_net.hpp"

class circuit_t {
public:
  circuit_t();
  size_t CELL_NUM, TERMINAL_NUM;
  // total movable cells number and terminals number, calculated in ispd_read_write.hpp, used in global_placer.hpp, and conjugate_gradient.hpp
  int LEFT, RIGHT, BOTTOM, TOP;
  // Boundaries of the chip, calculated in global_placer.hpp, used in conjugate_gradient.hpp
  float TARGET_FILLING_RATE, WHITE_SPACE_NODE_RATE;
  // target density of cells, movable cells density in each bin cannot exceed this density constraint
  double HPWL;
  double AVE_WIDTH, AVE_HEIGHT, AVE_CELL_AREA;

  std::vector< node_t > Nodelist;
  std::vector< net_t > Netlist;
  bool read_nodes_file(std::string const &NameOfFile);
  bool read_pl_file(std::string const &NameOfFile);
  bool read_nets_file(std::string const &NameOfFile);
  bool write_pl_solution(std::string const &NameOfFile);
  bool write_pl_anchor_solution(std::string const &NameOfFile);
  bool write_node_terminal(std::string const &NameOfFile="terminal.txt", std::string const &NameOfFile1="nodes.txt");
  bool write_anchor_terminal(std::string const &NameOfFile="terminal.txt", std::string const &NameOfFile1="nodes.txt");
  bool set_filling_rate(float rate=2.0/3.0);
  bool set_boundary(int left=0, int right=0, int bottom=0, int top=0);
};

#endif //HPCC_CIRCUIT_HPP
