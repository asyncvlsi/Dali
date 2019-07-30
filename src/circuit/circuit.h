//
// Created by Yihang Yang on 2019-03-26.
//

#ifndef HPCC_CIRCUIT_HPP
#define HPCC_CIRCUIT_HPP

#include <vector>
#include <map>
#include "block.h"
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

  double reset_signal_weight = 1e-6;
  double normal_signal_weight = 1;
  int lef_database_microns = 0;
  double m2_pitch = 0;
  int def_distance_microns = 0;
  int def_left = 0, def_right = 0, def_bottom = 0, def_top = 0;

  void SetBoundary(int left, int right, int bottom, int top);

  // API to add new BlockType
  bool IsBlockTypeExist(std::string &block_type_name);
  int BlockTypeIndex(std::string &block_type_name);
  BlockType *GetBlockType(std::string &block_type_name);
  void AddToBlockTypeMap(std::string &block_type_name);
  BlockType *AddBlockType(std::string &block_type_name, int width, int height);

  // API to add new Block Instance
  bool IsBlockExist(std::string &block_name);
  int BlockIndex(std::string &block_name);
  Block *GetBlock(std::string &block_name);
  void AddToBlockMap(std::string &block_name);
  void AddBlock(std::string &block_name, std::string &block_type_name, int llx = 0, int lly = 0, bool movable = true, BlockOrient orient= N);

  // API to add new Net
  bool IsNetExist(std::string &net_name);
  int NetIndex(std::string &net_name);
  Net *GetIndex(std::string &net_name);
  void AddToNetMap(std::string &net_name);
  Net *AddNet(std::string &net_name, double weight = 1);

  // old API
  void add_block_type(std::string &block_type_name, int width, int height);
  void add_pin_to_block(std::string &block_type_name, std::string &pin_name, int x_offset, int y_offset);
  void add_new_block(std::string &block_name, std::string &block_type_name, int llx = 0, int lly = 0, bool movable = true, BlockOrient orient = N);
  void create_blank_net(std::string &net_name, double weight);
  void add_pin_to_net(std::string &net_name, std::string &block_name, std::string &pin_name);

  // read lef/def file using above member functions
  void ParseLine(std::string &line, std::vector<std::string> &field_list);
  void ReadLefFile(std::string const &NameOfFile);
  void ReadDefFile(std::string const &NameOfFile);
  void ReportBlockTypeList();
  void ReportBlockTypeMap();
  void ReportBlockList();
  void ReportBlockMap();
  void ReportNetList();
  void ReportNetMap();

  // dump circuit to LEF/DEF file, readable by the the above ReadDefFile()
  void WriteDefFileDebug(std::string const &NameOfFile= "circuit.nodes");
  void GenMATLABScript(std::string const &filename= "block_net_list.m");
  void SaveDefFile(std::string const &NameOfFile, std::string const &defFileName);
};

#endif //HPCC_CIRCUIT_HPP
