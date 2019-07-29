//
// Created by Yihang Yang on 2019-03-26.
//

#ifndef HPCC_CIRCUIT_HPP
#define HPCC_CIRCUIT_HPP

#include <vector>
#include <map>
#include "block.h"
#include "pin.h"
#include "net.h"

class Circuit {
private:
  int tot_width_;
  int tot_height_;
  int tot_block_area_;
  int tot_movable_num_;
  int min_width_;
  int max_width_;
  int min_height_;
  int max_height_;
public:
  Circuit();
  Circuit(int tot_block_type_num, int tot_block_num, int tot_net_num);
  std::vector<BlockType> block_type_list;
  std::map<std::string, int> block_type_name_map;
  std::vector<Block > block_list;
  std::map<std::string, int> block_name_map;
  std::vector<Net > net_list;
  std::map<std::string, int> net_name_map;

  int lef_database_microns = 0;
  double m2_pitch = 0;
  int def_distance_microns = 0;

  int def_left = 0, def_right = 0, def_bottom = 0, def_top = 0;
  void SetBoundary(int left, int right, int bottom, int top);

  // API to add new BlockType
  bool IsBlockTypeExist(std::string &block_type_name);
  int BlockTypeIndex(std::string &block_type_name);
  void AddToBlockTypeMap(std::string &block_type_name);
  BlockType *AddBlockType(std::string &block_type_name, int width, int height);

  // API to add new Block Instance
  bool IsBlockInstExist(std::string &block_name);
  int BlockInstIndex(std::string &block_name);
  void AddToBlockMap(std::string &block_name);
  void AddBlockInst(std::string &block_name, std::string &block_type_name, int llx = 0, int lly = 0, bool movable = true, BlockOrient orient= N);

  // API to add new Net
  bool IsNetExist(std::string &net_name);
  int NetIndex(std::string &net_name);
  void AddToNetMap(std::string &net_name);
  Net *AddNet(std::string &net_name, double weight = 1);

  // old API
  void add_block_type(std::string &blockTypeName, int width, int height);
  void add_pin_to_block(std::string &blockTypeName, std::string &pinName, int xOffset, int yOffset);
  void add_new_block(std::string &blockName, std::string &blockTypeName, int llx = 0, int lly = 0, bool movable = true);
  void add_pin_to_net(std::string &netName, std::string &blockName, std::string &pinName);

  // functional member functions
  void parse_line(std::string &line, std::vector<std::string> &field_list);
  bool read_nodes_file(std::string const &NameOfFile);
  void report_block_list();
  void report_block_map();
  bool read_nets_file(std::string const &NameOfFile);
  void report_net_list();
  void report_net_map();
  bool read_pl_file(std::string const &NameOfFile);

  // dump circuit to LEF/DEF file, readable by
  bool write_nodes_file(std::string const &NameOfFile="circuit.nodes");
  bool write_nets_file(std::string const &NameOfFile="circuit.nets");
};

#endif //HPCC_CIRCUIT_HPP
