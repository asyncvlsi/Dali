//
// Created by Yihang Yang on 2019-03-26.
//

#ifndef DALI_CIRCUIT_HPP
#define DALI_CIRCUIT_HPP

#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include "block.h"
#include "blocktype.h"
#include "blocktypewell.h"
#include "common/opendb.h"
#include "common/si2lefdef.h"
#include "design.h"
#include "iopin.h"
#include "layer.h"
#include "net.h"
#include "status.h"
#include "tech.h"

class Circuit {
  friend class Placer;
 public:
  Tech tech_; // information in LEF and CELL
  Design design_; // information in DEF
  WellInfo well_info_; // maybe redundant

#ifdef USE_OPENDB
  odb::dbDatabase *db_ptr_;
#endif

  Circuit();
  /****API to initialize circuit
   * 1. from openDB
   * 2. from LEF/DEF directly
   * ****/
#ifdef USE_OPENDB
  explicit Circuit(odb::dbDatabase *db_ptr);
  void InitializeFromDB(odb::dbDatabase *db_ptr);
#endif
  void ReadLefFile(std::string const &name_of_file);
  void ReadDefFile(std::string const &name_of_file);

  void ReadCellFile(std::string const &name_of_file);
  void LoadImaginaryCellFile();

  /****API to set grid value****/
  void SetGridValue(double grid_value_x, double grid_value_y);
  void SetGridUsingMetalPitch();
  double GetGridValueX() const { return tech_.grid_value_x_; } // unit in micro
  double GetGridValueY() const { return tech_.grid_value_y_; }
  void SetRowHeight(double row_height) {
    Assert(row_height > 0, "Setting row height to a negative value?");
    tech_.row_height_set_ = true;
    tech_.row_height_ = row_height;
  }
  double GetDBRowHeight() const {
    return tech_.row_height_;
  }
  int GetIntRowHeight() const {
    Assert(tech_.row_height_set_, "Row height not set, cannot retrieve its value: Circuit::GetIntRowHeight()\n");
    return (int) std::round(tech_.row_height_ / tech_.grid_value_y_);
  }

  /****API to set metal layers: deprecated
   * now the metal layer information are all stored in openDB data structure
   * ****/
  std::vector<MetalLayer> *MetalList() { return &tech_.metal_list; }
  std::unordered_map<std::string, int> *MetalNameMap() { return &tech_.metal_name_map; }
  bool IsMetalLayerExist(std::string &metal_name) {
    return tech_.metal_name_map.find(metal_name) != tech_.metal_name_map.end();
  }
  int MetalLayerIndex(std::string &metal_name) {
    Assert(IsMetalLayerExist(metal_name), "MetalLayer does not exist, cannot find it: " + metal_name);
    return tech_.metal_name_map.find(metal_name)->second;
  }
  MetalLayer *GetMetalLayer(std::string &metal_name) {
    Assert(IsMetalLayerExist(metal_name), "MetalLayer does not exist, cannot find it: " + metal_name);
    return &tech_.metal_list[MetalLayerIndex(metal_name)];
  }
  MetalLayer *AddMetalLayer(std::string &metal_name, double width, double spacing);
  MetalLayer *AddMetalLayer(std::string &metal_name) {
    return AddMetalLayer(metal_name, 0, 0);
  }
  void ReportMetalLayers();

  /****API for BlockType
   * These are MACRO section in LEF
   * ****/
  std::unordered_map<std::string, BlockType *> *BlockTypeMap() { return &tech_.block_type_map; }
  bool IsBlockTypeExist(std::string &block_type_name) {
    return tech_.block_type_map.find(block_type_name) != tech_.block_type_map.end();
  }
  BlockType *GetBlockType(std::string &block_type_name) {
    Assert(IsBlockTypeExist(block_type_name), "BlockType not exist, cannot find it: " + block_type_name);
    return tech_.block_type_map.find(block_type_name)->second;
  }
  BlockType *AddBlockType(std::string &block_type_name, int width, int height);
  void ReportBlockType();
  void CopyBlockType(Circuit &circuit);

  /****API for DIE AREA
   * These are DIEAREA section in DEF
   * ****/
  int RegionLLX() const { return design_.region_left_; }
  int RegionURX() const { return design_.region_right_; }
  int RegionLLY() const { return design_.region_bottom_; }
  int RegionURY() const { return design_.region_top_; }
  int RegionWidth() const { return design_.region_right_ - design_.region_left_; }
  int RegionHeight() const { return design_.region_top_ - design_.region_bottom_; }
  void SetBoundary(int left, int right, int bottom, int top) { // unit in grid value
    Assert(right > left, "Right boundary is not larger than Left boundary?");
    Assert(top > bottom, "Top boundary is not larger than Bottom boundary?");
    design_.region_left_ = left;
    design_.region_right_ = right;
    design_.region_bottom_ = bottom;
    design_.region_top_ = top;
  }
  void SetDieArea(int lower_x, int upper_x, int lower_y, int upper_y) { // unit in manufacturing grid
    Assert(tech_.grid_value_x_ > 0 && tech_.grid_value_y_ > 0,
           "Need to set positive grid values before setting placement boundary");
    Assert(design_.def_distance_microns > 0,
           "Need to set def_distance_microns before setting placement boundary using Circuit::SetDieArea()");
    double factor_x = tech_.grid_value_x_ * design_.def_distance_microns;
    double factor_y = tech_.grid_value_y_ * design_.def_distance_microns;
    SetBoundary((int) std::round(lower_x / factor_x),
                (int) std::round(upper_x / factor_x),
                (int) std::round(lower_y / factor_y),
                (int) std::round(upper_y / factor_y));
  }

  /****API for Block
   * These are COMPONENTS section in DEF
   * ****/
  std::vector<Block> *GetBlockList() { return &(design_.block_list); }
  bool IsBlockExist(std::string &block_name);
  int BlockIndex(std::string &block_name);
  Block *GetBlock(std::string &block_name);
  void AddBlock(std::string &block_name,
                BlockType *block_type_ptr,
                int llx = 0,
                int lly = 0,
                bool movable = true,
                BlockOrient orient = N_,
                bool is_real_cel = true);
  void AddBlock(std::string &block_name,
                std::string &block_type_name,
                int llx = 0,
                int lly = 0,
                bool movable = true,
                BlockOrient orient = N_,
                bool is_real_cel = true);
  void AddBlock(std::string &block_name,
                BlockType *block_type_ptr,
                int llx = 0,
                int lly = 0,
                PlaceStatus place_status = UNPLACED_,
                BlockOrient orient = N_,
                bool is_real_cel = true);
  void AddBlock(std::string &block_name,
                std::string &block_type_name,
                int llx = 0,
                int lly = 0,
                PlaceStatus place_status = UNPLACED_,
                BlockOrient orient = N_,
                bool is_real_cel = true);
  void ReportBlockList();
  void ReportBlockMap();

  /****API for IOPIN
   * These are PINS section in DEF
   * ****/
  std::vector<IOPin> *GetIOPinList() { return &(design_.iopin_list); }
  void AddDummyIOPinType();
  bool IsIOPinExist(std::string &iopin_name);
  int IOPinIndex(std::string &iopin_name);
  IOPin *GetIOPin(std::string &iopin_name);
  IOPin *AddUnplacedIOPin(std::string &iopin_name);
  IOPin *AddPlacedIOPin(std::string &iopin_name, int lx, int ly);
  IOPin *AddIOPin(std::string &iopin_name, PlaceStatus place_status, int lx, int ly);
  void ReportIOPin();

  /****API for Nets
   * These are NETS section in DEF
   * ****/
  std::vector<Net> *GetNetList() { return &(design_.net_list); }
  bool IsNetExist(std::string &net_name);
  int NetIndex(std::string &net_name);
  Net *GetNet(std::string &net_name);
  void AddToNetMap(std::string &net_name);
  Net *AddNet(std::string &net_name, int capacity, double weight);
  void ReportNetList();
  void ReportNetMap();
  void InitNetFanoutHisto(std::vector<int> *histo_x = nullptr) {
    design_.InitNetFanoutHisto(histo_x);
    design_.net_histogram_.hpwl_unit_ = tech_.grid_value_x_;
  }
  void UpdateNetHPWLHisto();
  void ReportNetFanoutHisto() {
    UpdateNetHPWLHisto();
    design_.ReportNetFanoutHisto();
  }

  /****Utility functions related to netlist management****/
  void NetListPopBack();

  void ReportBriefSummary() const;

  /****API to add N/P-well technology information
   * These are for CELL file
   * ****/
  BlockTypeCluster *AddBlockTypeCluster() {
    well_info_.cluster_list_.emplace_back(nullptr, nullptr);
    return &(well_info_.cluster_list_.back());
  }
  BlockTypeWell *AddBlockTypeWell(BlockTypeCluster *cluster, BlockType *blk_type, bool is_plug);
  BlockTypeWell *AddBlockTypeWell(BlockTypeCluster *cluster, std::string &blk_type_name, bool is_plug) {
    BlockType *blk_type_ptr = GetBlockType(blk_type_name);
    AddBlockTypeWell(cluster, blk_type_ptr, is_plug);
    return blk_type_ptr->WellPtr();
  }
  void SetNWellParams(double width, double spacing, double op_spacing, double max_plug_dist, double overhang) {
    tech_.SetNLayer(width, spacing, op_spacing, max_plug_dist, overhang);
  }
  void SetPWellParams(double width, double spacing, double op_spacing, double max_plug_dist, double overhang) {
    tech_.SetPLayer(width, spacing, op_spacing, max_plug_dist, overhang);
  }
  void SetLegalizerSpacing(double same_spacing, double any_spacing) {
    tech_.SetDiffSpacing(same_spacing, any_spacing);
  }
  Tech *GetTech() { return &tech_; }
  void ReportWellShape();

  /****API to add virtual nets for timing driven placement****/
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

  /****member functions to obtain statical values****/
  int MinBlkWidth() const { return design_.blk_min_width_; }
  int MaxBlkWidth() const { return design_.blk_max_width_; }
  int MinBlkHeight() const { return design_.blk_min_height_; }
  int MaxBlkHeight() const { return design_.blk_max_height_; }
  long int TotBlkArea() const { return design_.tot_blk_area_; }
  int TotBlkNum() const { return design_.blk_count_; }
  int TotMovableBlockNum() const { return design_.tot_mov_blk_num_; }
  int TotFixedBlkCnt() const { return (int) design_.block_list.size() - design_.tot_mov_blk_num_; }
  double AveBlkWidth() const { return double(design_.tot_width_) / double(TotBlkNum()); }
  double AveBlkHeight() const { return double(design_.tot_height_) / double(TotBlkNum()); }
  double AveBlkArea() const { return double(design_.tot_blk_area_) / double(TotBlkNum()); }
  double AveMovBlkWidth() const { return double(design_.tot_mov_width_) / design_.tot_mov_blk_num_; }
  double AveMovBlkHeight() const { return double(design_.tot_mov_height_) / design_.tot_mov_blk_num_; }
  double AveMovBlkArea() const { return double(design_.tot_mov_block_area_) / design_.tot_mov_blk_num_; }
  double WhiteSpaceUsage() const {
    return double(TotBlkArea()) / (RegionURX() - RegionLLX()) / (RegionURY() - RegionLLY());
  }

  /****Utility member functions****/
  void NetSortBlkPin();
  // calculating HPWL from precise pin locations
  double HPWLX();
  double HPWLY();
  double HPWL() { return HPWLX() + HPWLY(); }
  void ReportHPWL() { printf("  Current HPWL: %e um\n", HPWL()); }
  void ReportHPWLHistogramLinear(int bin_num = 10);
  void ReportHPWLHistogramLogarithm(int bin_num = 10);
  // calculating HPWL from the center of cells
  double HPWLCtoCX();
  double HPWLCtoCY();
  double HPWLCtoC() {return HPWLCtoCX() + HPWLCtoCY();}
  void ReportHPWLCtoC() {printf("  Current HPWL: %e um\n", HPWLCtoC());}

  /****dump placement results to various file formats****/
  void WriteDefFileDebug(std::string const &name_of_file = "circuit.def");
  void GenMATLABScript(std::string const &name_of_file = "block_net_list.m");
  void GenMATLABTable(std::string const &name_of_file = "block.txt", bool only_well_tap = false);
  void GenMATLABWellTable(std::string const &name_of_file = "res", bool only_well_tap = false);
  void SaveDefFile(std::string const &name_of_file, std::string const &def_file_name, bool is_complete_version = true);
  void SaveDefFile(std::string const &base_name,
                   std::string const &name_padding,
                   std::string const &def_file_name,
                   int save_floorplan,
                   int save_cell,
                   int save_iopin,
                   int save_net);
  void SaveIODefFile(std::string const &name_of_file, std::string const &def_file_name);
  void SaveDefWell(std::string const &name_of_file, std::string const &def_file_name, bool is_no_normal_cell = true);
  void SaveDefPPNPWell(std::string const &name_of_file, std::string const &def_file_name);
  void SaveInstanceDefFile(std::string const &name_of_file, std::string const &def_file_name);

  /****some Bookshelf IO APIs****/
  void SaveBookshelfNode(std::string const &name_of_file);
  void SaveBookshelfNet(std::string const &name_of_file);
  void SaveBookshelfPl(std::string const &name_of_file);
  void SaveBookshelfScl(std::string const &name_of_file);
  void SaveBookshelfWts(std::string const &name_of_file);
  void SaveBookshelfAux(std::string const &name_of_file);
  void LoadBookshelfPl(std::string const &name_of_file);

  /****static functions****/
  static void StrSplit(std::string &line, std::vector<std::string> &res);
  static int FindFirstDigit(std::string &str);
};

#endif //DALI_CIRCUIT_HPP
