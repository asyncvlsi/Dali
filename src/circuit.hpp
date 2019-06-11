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
protected:
  double _ave_width;
  double _ave_height;
  int _tot_block_area;
  int _tot_movable_num;
  // these data entries are all cached data
public:
  circuit_t();
  /* essential data entries */
  std::vector< block_t > block_list;
  std::vector< net_t > net_list;
  // node_list and net_list contains all the information of a circuit graph

  int HPWL;
  // HPWL of this circuit
  std::map<std::string, size_t> block_name_map;
  // string to size_t map to find the index of a block in the block_list
  std::map<std::string, size_t> net_name_map;
  // string to size_t map to find the index of a net in the net_list

  bool add_new_block(std::string &blockName, int w, int h, int llx = 0, int lly = 0, bool movable = true);
  bool create_blank_net(std::string &netName, double weight = 1);
  bool add_pin_to_net(std::string &netName, std::string &blockName, int xOffset, int yOffset);
  // the above three member functions should be called to add elements to block_list or net_list

  void parse_line(std::string &line, std::vector<std::string> &field_list);
  bool read_nodes_file(std::string const &NameOfFile);
  void report_block_list();
  void report_block_map();
  bool read_nets_file(std::string const &NameOfFile);
  void report_net_list();
  void report_net_map();
  bool read_pl_file(std::string const &NameOfFile);
  // the above member functions should be called when input comes from files

  //-----------------------------------------------------------------------------------------------
  /* the following member function calculate corresponding values in real time, the running time is O(n) */
  double ave_width_real_time();
  double ave_height_real_time();
  double ave_block_area_real_time();
  int tot_block_area_real_time();
  int tot_movable_num_real_time();
  int tot_unmovable_num_real_time();

  /* these following member functions just return cached data entries O(1)
   * or if the cached data entries have not been initialized, the corresponding real_time function will be called O(n) */
  double ave_width();
  double ave_height();
  double ave_block_area();
  int tot_block_area();
  int tot_movable_num();
  int tot_unmovable_num();

};

#endif //HPCC_CIRCUIT_HPP
