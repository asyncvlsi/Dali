//
// Created by Yihang Yang on 2019-03-26.
//

#ifndef HPCC_CIRCUIT_HPP
#define HPCC_CIRCUIT_HPP

#include <vector>
#include <map>
#include "circuitblock.hpp"
#include "circuitpin.hpp"
#include "circuitnet.hpp"

class circuit_t {
public:
  circuit_t();
  /* essential data entries */
  std::vector< block_t > block_list;
  std::vector< net_t > net_list;
  // node_list and net_list contains all the information of a circuit graph

  /* the following entries are derived data */
  size_t tot_movable_num, tot_unmovable_num;
  // the total number of movable blocks, and the total number of unmovable blocks. These two variables might be removed later
  int HPWL;
  // HPWL of this circuit
  double ave_width;
  double ave_height;
  double ave_cell_area;
  // average cell width, height, and area
  std::map<std::string, size_t> block_name_map;
  // string to size_t map to find the index of a block in the block_list
  std::map<std::string, size_t> net_name_map;
  // string to size_t map to find the index of a net in the net_list

  bool add_to_block_list(block_t &block);
  bool add_to_net_list(net_t &net);
  void parse_line(std::string &line, std::vector<std::string> &field_list);
  bool read_nodes_file(std::string const &NameOfFile);
  void report_block_list();
  void report_block_map();
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
