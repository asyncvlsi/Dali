//
// Created by Yihang Yang on 2019-03-26.
//

#ifndef HPCC_CIRCUIT_H
#define HPCC_CIRCUIT_H

#include <vector>
#include <map>
#include "circuitblocktype.h"
#include "circuitblock.h"
#include "circuitpin.h"
#include "circuitnet.h"

class circuit_t {
private:
  int _dummy_space_x = 0;
  int _dummy_space_y = 0;
  double _global_signal_weight = 1e-6;
  double _normal_signal_weight = 1;
protected:
  double _ave_width;
  double _ave_height;
  int _tot_block_area;
  int _tot_movable_num;
  // these data entries are all cached data
public:
  circuit_t();
  /* essential data entries */
  std::vector< block_t > blockList;
  std::map<std::string, size_t> blockNameMap;
  // string to size_t map to find the index of a block in the blockList
  std::vector< net_t > netList;
  std::map<std::string, size_t> netNameMap;
  // string to size_t map to find the index of a net in the netList
  // node_list and netList contains all the information of a circuit graph

  void set_dummy_space(int init_ds_x, int init_ds_y);
  int dummy_space_x();
  int dummy_space_y();

  bool add_new_block(std::string &blockName, int w, int h, int llx = 0, int lly = 0, bool movable = true, std::string typeName="");
  bool create_blank_net(std::string &netName, double weight = 1);
  bool add_pin_to_net(const std::string &netName, const std::string &blockName, int xOffset, int yOffset, std::string pinName="");
  // the above three member functions should be called to add elements to blockList or netList

  void parse_line(std::string &line, std::vector<std::string> &field_list);
  bool read_nodes_file(std::string const &NameOfFile);
  void report_block_list();
  void report_block_map();
  bool read_nets_file(std::string const &NameOfFile);
  void report_net_list();
  void report_net_map();
  bool read_pl_file(std::string const &NameOfFile);
  // the above member functions should be called when input comes from files

  int lef_database_microns = 0;
  double m2_pitch = 0;
  int def_distance_microns = 0;
  int def_left = 0, def_right = 0, def_bottom = 0, def_top = 0;
  std::vector< block_type_t > blockTypeList;
  std::map<std::string, size_t> blockTypeNameMap;
  bool add_block_type(std::string &blockTypeName, int width, int height);
  bool add_pin_to_block(std::string &blockTypeName, std::string &pinName, int xOffset, int yOffset);
  bool add_new_block(std::string &blockName, std::string &blockTypeName, int llx = 0, int lly = 0, bool movable = true);
  bool add_pin_to_net(std::string &netName, std::string &blockName, std::string &pinName);
  bool read_lef_file(std::string const &NameOfFile);
  void report_blockType_list();
  void report_blockType_map();
  bool read_def_file(std::string const &NameOfFile);

  std::vector< net_t > pseudoNetList;
  std::map<std::string, size_t> pseudoNetNameMap;
  bool create_pseudo_net(std::string &driveBlockName, std::string &drivePinName, std::string &loadBlockName, std::string &loadPinName, double weight = 1);
  bool remove_pseudo_net(std::string &driveBlockName, std::string &drivePinName, std::string &loadBlockName, std::string &loadPinName);
  void remove_all_pseudo_nets();

  //-----------------------------------------------------------------------------------------------
  /* the following member function calculate corresponding values in real time, the running time is O(n) */
  double ave_width_real_time();
  double ave_height_real_time();
  double ave_block_area_real_time();
  int tot_block_area_real_time();
  int tot_movable_num_real_time();
  int tot_unmovable_num_real_time();
  int reportHPWL();

  /* these following member functions just return cached data entries O(1)
   * or if the cached data entries have not been initialized, the corresponding real_time function will be called O(n) */
  double ave_width();
  double ave_height();
  double ave_block_area();
  int tot_block_area();
  int tot_movable_num();
  int tot_unmovable_num();

  bool write_nodes_file(std::string const &NameOfFile="circuit.nodes");
  bool write_nets_file(std::string const &NameOfFile="circuit.nets");
  bool gen_matlab_disp_file(std::string const &filename="block_net_list.m");
  bool save_DEF(std::string const &NameOfFile, std::string const &defFileName);
};

#endif //HPCC_CIRCUIT_H
