//
// Created by Yihang Yang on 2019-03-26.
//

#ifndef HPCC_CIRCUIT_HPP
#define HPCC_CIRCUIT_HPP

#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include "status.h"
#include "layer.h"
#include "block.h"
#include "pin.h"
#include "net.h"

class Circuit {
 private:
  // statistical data of the circuit
  int tot_width_;
  int tot_height_;
  long int tot_block_area_;
  int tot_mov_width_;
  int tot_mov_height_;
  long int tot_mov_block_area_;
  int tot_movable_blk_num_;
  int min_width_;
  int max_width_;
  int min_height_;
  int max_height_;

  // Manufacturing Grid
  bool grid_set_;
  double grid_value_x_;
  double grid_value_y_;

 public:
  Circuit();
  ~Circuit();

  std::vector<MetalLayer> metal_list;
  std::unordered_map<std::string, int> metal_name_map;
  bool IsMetalLayerExist(std::string &metal_name);
  int MetalLayerIndex(std::string &metal_name);
  MetalLayer *GetMetalLayer(std::string &metal_name);
  MetalLayer *AddMetalLayer(std::string &metal_name, double width, double spacing);
  MetalLayer *AddMetalLayer(std::string &metal_name);
  void ReportMetalLayers();

  std::unordered_map<std::string, BlockType*> block_type_name_map;

  std::vector<Block> block_list;
  std::map<std::string, int> block_name_map;

  std::vector<Pin> pin_list;
  std::map<std::string, int> pin_name_map;

  std::vector<Net> net_list;
  std::map<std::string, int> net_name_map;

  double reset_signal_weight = 1;
  double normal_signal_weight = 1;
  double manufacturing_grid = 0;
  int lef_database_microns = 0;
  int def_distance_microns = 0;
  int def_left = 0, def_right = 0, def_bottom = 0, def_top = 0;
  bool def_boundary_set = false;
  void SetBoundaryFromDef(int left, int right, int bottom, int top);

  // API to add new BlockType
  bool IsBlockTypeExist(std::string &block_type_name);
  BlockType *GetBlockType(std::string &block_type_name);
  BlockType *AddBlockType(std::string &block_type_name, int width, int height);

  // API to add new Block Instance
  bool IsBlockExist(std::string &block_name);
  int BlockIndex(std::string &block_name);
  Block *GetBlock(std::string &block_name);
  void AddBlock(std::string &block_name, BlockType *block_type, int llx = 0, int lly = 0, bool movable = true, BlockOrient orient= N);
  void AddBlock(std::string &block_name, std::string &block_type_name, int llx = 0, int lly = 0, bool movable = true, BlockOrient orient= N);
  void AddBlock(std::string &block_name, BlockType *block_type, int llx = 0, int lly = 0, PlaceStatus place_status = UNPLACED, BlockOrient orient= N);
  void AddBlock(std::string &block_name, std::string &block_type_name, int llx = 0, int lly = 0, PlaceStatus place_status = UNPLACED, BlockOrient orient= N);

  // API to add new IOPin
  bool IsIOPinExist(std::string &iopin_name);
  int PinIndex();

  // API to add new Net
  bool IsNetExist(std::string &net_name);
  int NetIndex(std::string &net_name);
  Net *GetNet(std::string &net_name);
  void AddToNetMap(std::string &net_name);
  Net *AddNet(std::string &net_name, double weight = 1);

  // API to set grid value
  void SetGridValue(double grid_value_x, double grid_value_y);
  void SetGridUsingMetalPitch();

  // API to add well information
  void AddBlkWell(std::string &block_name, bool is_pluged, int llx, int lly, int urx, int ury);
  void AddBlkWell(Block *blk, bool is_pluged, int llx, int lly, int urx, int ury);
  void SetWellWidth(double well_width);
  void SetWellSpace(double well_space);
  void SetWellMaxLengthUnplug(double max_length_unplug);
  double WellWidth() const;
  double WellSpace() const;
  double WellMaxLengthUnplug() const;

  /*
  std::vector< Net > pseudo_net_list;
  std::map<std::string, size_t> pseudo_net_name_map;
  bool CreatePseudoNet(std::string &drive_blk, std::string &drive_pin, std::string &load_blk, std::string &load_pin, double weight = 1);
  bool CreatePseudoNet(Block *drive_blk, int drive_pin, Block *load_blk, int load_pin, double weight = 1);
  bool RemovePseudoNet(std::string &drive_blk, std::string &drive_pin, std::string &load_blk, std::string &load_pin);
  bool RemovePseudoNet(Block *drive_blk, int drive_pin, Block *load_blk, int load_pin);
  void RemoveAllPseudoNets();
   */
  // repulsive force can be created using an attractive force, a spring whose rest length in the current distance or even longer than the current distance

  // read lef/def file using above member functions
  double GridValueX(); // unit in micro
  double GridValueY();
  void ReadLefFile(std::string const &name_of_file);
  void ReadDefFile(std::string const &name_of_file);
  void ReportBlockTypeList();
  void ReportBlockTypeMap();
  void ReportBlockList();
  void ReportBlockMap();
  void ReportNetList();
  void ReportNetMap();
  void ReportBriefSummary();

  int MinWidth() const;
  int MaxWidth() const;
  int MinHeight() const;
  int MaxHeight() const;
  long int TotArea() const;
  int TotBlockNum() const;
  int TotMovableBlockNum() const;
  unsigned int TotFixedBlkCnt() const {return block_list.size() - tot_movable_blk_num_;}
  double AveWidth() const;
  double AveHeight() const;
  double AveArea() const;
  double AveMovWidth() const;
  double AveMovHeight() const;
  double AveMovArea() const;

  void NetSortBlkPin();
  double HPWLX();
  double HPWLY();
  double HPWL();
  void ReportHPWL();
  double HPWLCtoCX();
  double HPWLCtoCY();
  double HPWLCtoC();
  void ReportHPWLCtoC();

  // dump circuit to LEF/DEF file, readable by the the above ReadDefFile()
  void WriteDefFileDebug(std::string const &name_of_file= "circuit.def");
  void GenMATLABScript(std::string const &name_of_file= "block_net_list.m");
  void SaveDefFile(std::string const &name_of_file, std::string const &def_file_name);
  void SaveISPD(std::string const &name_of_file);
};

#endif //HPCC_CIRCUIT_HPP
