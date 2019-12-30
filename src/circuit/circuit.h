//
// Created by Yihang Yang on 2019-03-26.
//

#ifndef DALI_CIRCUIT_HPP
#define DALI_CIRCUIT_HPP

#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include "status.h"
#include "layer.h"
#include "tech.h"
#include "design.h"
#include "blocktype.h"
#include "blocktypewell.h"
#include "block.h"
#include "iopin.h"
#include "net.h"
#include "../opendb.h"

class Circuit {
  friend class Placer;
 private:
  // statistical data of the circuit
  unsigned long int tot_width_;
  unsigned long int tot_height_;
  unsigned long int tot_blk_area_;
  unsigned long int tot_mov_width_;
  unsigned long int tot_mov_height_;
  unsigned long int tot_mov_block_area_;
  int tot_mov_blk_num_;
  int blk_min_width_;
  int blk_max_width_;
  int blk_min_height_;
  int blk_max_height_;

  // Manufacturing Grid
  bool grid_set_;
  double grid_value_x_;
  double grid_value_y_;

  Tech * tech_param_;
  Design * design_;
  WellInfo well_info_;
#ifdef USE_OPENDB
  odb::dbDatabase * db_;
#endif
 public:
  Circuit();
  ~Circuit();

  /****API to initialize circuit
   * 1. from openDB
   * 2. from LEF/DEF directly
   * ****/
#ifdef USE_OPENDB
  explicit Circuit(odb::dbDatabase* db);
  void InitializeFromDB(odb::dbDatabase* db);
#else
  void ReadLefFile(std::string const &name_of_file);
  void ReadDefFile(std::string const &name_of_file);
#endif

  /****API to set grid value****/
  void SetGridValue(double grid_value_x, double grid_value_y);
  double GetGridValueX() const {return grid_value_x_;} // unit in micro
  double GetGridValueY() const {return grid_value_y_;}
  void SetGridUsingMetalPitch();

  /****API to set metal layers: deprecated
   * now the metal layer information are all stored in openDB data structure
   * ****/
  std::vector<MetalLayer> metal_list;
  std::unordered_map<std::string, int> metal_name_map;
  bool IsMetalLayerExist(std::string &metal_name);
  int MetalLayerIndex(std::string &metal_name);
  MetalLayer *GetMetalLayer(std::string &metal_name);
  MetalLayer *AddMetalLayer(std::string &metal_name, double width, double spacing);
  MetalLayer *AddMetalLayer(std::string &metal_name);
  void ReportMetalLayers();

  /****API for BlockType
   * These are MACRO section in LEF
   * ****/
  std::unordered_map<std::string, BlockType*> block_type_map;
  bool IsBlockTypeExist(std::string &block_type_name);
  BlockType *GetBlockType(std::string &block_type_name);
  BlockType *AddBlockType(std::string &block_type_name, unsigned int width, unsigned int height);
  void ReportBlockType();
  void CopyBlockType(Circuit &circuit);

  /****API for DIE AREA
   * These are DIEAREA section in DEF
   * ****/
  int def_left = 0, def_right = 0, def_bottom = 0, def_top = 0;
  void SetBoundary(int left, int right, int bottom, int top); // unit in grid value
  void SetDieArea(int lower_x, int upper_x, int lower_y, int upper_y); // unit in um
  int Left() {return def_left;}
  int Right() {return def_right;}
  int Bottom() {return def_bottom;}
  int Top() {return def_top;}

  /****API for Block
   * These are COMPONENTS section in DEF
   * ****/
  std::vector<Block> block_list;
  std::map<std::string, int> block_name_map;
  bool IsBlockExist(std::string &block_name);
  int BlockIndex(std::string &block_name);
  Block *GetBlock(std::string &block_name);
  void AddBlock(std::string &block_name, BlockType *block_type, int llx = 0, int lly = 0, bool movable = true, BlockOrient orient= N);
  void AddBlock(std::string &block_name, std::string &block_type_name, int llx = 0, int lly = 0, bool movable = true, BlockOrient orient= N);
  void AddBlock(std::string &block_name, BlockType *block_type, int llx = 0, int lly = 0, PlaceStatus place_status = UNPLACED, BlockOrient orient= N);
  void AddBlock(std::string &block_name, std::string &block_type_name, int llx = 0, int lly = 0, PlaceStatus place_status = UNPLACED, BlockOrient orient= N);
  void ReportBlockList();
  void ReportBlockMap();

  /****API for IOPIN
   * These are PINS section in DEF
   * ****/
  std::vector<IOPin> pin_list;
  std::map<std::string, int> pin_name_map;
  void AddAbsIOPinType();
  bool IsIOPinExist(std::string &iopin_name);
  int IOPinIndex(std::string &iopin_name);
  IOPin *GetIOPin(std::string &iopin_name);
  IOPin *AddIOPin(std::string &iopin_name);
  IOPin *AddIOPin(std::string &iopin_name, int lx, int ly);
  void ReportIOPin();

  /****API for Nets
   * These are NETS section in DEF
   * ****/
  std::vector<Net> net_list;
  std::map<std::string, int> net_name_map;
  bool IsNetExist(std::string &net_name);
  int NetIndex(std::string &net_name);
  Net *GetNet(std::string &net_name);
  void AddToNetMap(std::string &net_name);
  Net *AddNet(std::string &net_name, double weight = 1);
  void ReportNetList();
  void ReportNetMap();

  void ReportBriefSummary();

  double reset_signal_weight = 1;
  double normal_signal_weight = 1;
  double manufacturing_grid = 0;
  int lef_database_microns = 0;
  int def_distance_microns = 0;

  /****API to add N/P-well technology information
   * These are for CELL file
   * ****/
  BlockTypeCluster *AddBlockTypeCluster();
  BlockTypeWell *AddBlockTypeWell(BlockTypeCluster *cluster, BlockType *blk_type, bool is_plug);
  BlockTypeWell *AddBlockTypeWell(BlockTypeCluster *cluster, std::string &blk_type_name, bool is_plug);
  void SetNWellParams(double width, double spacing, double op_spacing, double max_plug_dist);
  void SetPWellParams(double width, double spacing, double op_spacing, double max_plug_dist);
  Tech *GetTech() const {return tech_param_;}
  void ReadWellFile(std::string const &name_of_file);
  void ReportWellShape();

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

  /****Other member functions****/
  int MinWidth() const {return blk_min_width_;}
  int MaxWidth() const {return  blk_max_width_;}
  int MinHeight() const {return blk_min_height_;}
  int MaxHeight() const {return  blk_max_height_;}
  unsigned long int TotArea() const {return tot_blk_area_;}
  int TotBlockNum() const {return block_list.size();}
  int TotMovableBlockNum() const {return tot_mov_blk_num_;}
  unsigned int TotFixedBlkCnt() const {return block_list.size() - tot_mov_blk_num_;}
  double AveWidth() const {return double(tot_width_)/double(TotBlockNum());}
  double AveHeight() const {return double(tot_height_)/double(TotBlockNum());}
  double AveArea() const {return double(tot_blk_area_)/(double)TotBlockNum();}
  double AveMovWidth() const {return double(tot_mov_width_)/TotMovableBlockNum();}
  double AveMovHeight() const {return double(tot_mov_height_)/TotMovableBlockNum();}
  double AveMovArea() const {return double(tot_mov_block_area_)/TotMovableBlockNum();}
  double WhiteSpaceUsage() const {return double(TotArea())/(def_right-def_left)/(def_top-def_bottom);}

  void NetSortBlkPin();
  double HPWLX();
  double HPWLY();
  double HPWL();
  void ReportHPWL();
  double HPWLCtoCX();
  double HPWLCtoCY();
  double HPWLCtoC();
  void ReportHPWLCtoC();

  /****dump placement results to various file formats****/
  void WriteDefFileDebug(std::string const &name_of_file= "circuit.def");
  void GenMATLABScript(std::string const &name_of_file= "block_net_list.m");
  void GenMATLABTable(std::string const &name_of_file = "block.txt");
  void GenMATLABWellTable(std::string const &name_of_file = "res");
  void SaveDefFile(std::string const &name_of_file, std::string const &def_file_name);
  void SaveISPD(std::string const &name_of_file);
};

#endif //DALI_CIRCUIT_HPP
