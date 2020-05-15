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
  odb::dbDatabase *db_;
#endif

  Circuit();
  /****API to initialize circuit
   * 1. from openDB
   * 2. from LEF/DEF directly
   * ****/
#ifdef USE_OPENDB
  explicit Circuit(odb::dbDatabase *db);
  void InitializeFromDB(odb::dbDatabase *db);
#endif
  void ReadLefFile(std::string const &name_of_file);
  void ReadDefFile(std::string const &name_of_file);

  void ReadCellFile(std::string const &name_of_file);
  void LoadImaginaryCellFile();

  /****API to set grid value****/
  double GetGridValueX() const; // unit in micro
  double GetGridValueY() const;
  void SetGridValue(double grid_value_x, double grid_value_y);
  void SetGridUsingMetalPitch();
  void SetRowHeight(double row_height);
  double GetDBRowHeight() const;
  int GetIntRowHeight() const;

  /****API to set metal layers: deprecated
   * now the metal layer information are all stored in openDB data structure
   * ****/
  std::vector<MetalLayer> *MetalList();
  std::unordered_map<std::string, int> *MetalNameMap();
  bool IsMetalLayerExist(std::string &metal_name);
  int MetalLayerIndex(std::string &metal_name);
  MetalLayer *GetMetalLayer(std::string &metal_name);
  MetalLayer *AddMetalLayer(std::string &metal_name, double width, double spacing);
  MetalLayer *AddMetalLayer(std::string &metal_name);
  void ReportMetalLayers();

  /****API for BlockType
   * These are MACRO section in LEF
   * ****/
  std::unordered_map<std::string, BlockType *> *BlockTypeMap();
  bool IsBlockTypeExist(std::string &block_type_name);
  BlockType *GetBlockType(std::string &block_type_name);
  BlockType *AddBlockType(std::string &block_type_name, int width, int height);
  void ReportBlockType();
  void CopyBlockType(Circuit &circuit);

  /****API for DIE AREA
   * These are DIEAREA section in DEF
   * ****/
  int RegionLLX() const;
  int RegionURX() const;
  int RegionLLY() const;
  int RegionURY() const;
  int RegionWidth() const;
  int RegionHeight() const;
  void SetBoundary(int left, int right, int bottom, int top); // unit in grid value
  void SetDieArea(int lower_x, int upper_x, int lower_y, int upper_y); // unit in manufacturing grid

  /****API for Block
   * These are COMPONENTS section in DEF
   * ****/
  std::vector<Block> *GetBlockList();
  bool IsBlockExist(std::string &block_name);
  int BlockIndex(std::string &block_name);
  Block *GetBlock(std::string &block_name);
  void AddBlock(std::string &block_name,
                BlockType *block_type,
                int llx = 0,
                int lly = 0,
                bool movable = true,
                BlockOrient orient = N_);
  void AddBlock(std::string &block_name,
                std::string &block_type_name,
                int llx = 0,
                int lly = 0,
                bool movable = true,
                BlockOrient orient = N_);
  void AddBlock(std::string &block_name,
                BlockType *block_type,
                int llx = 0,
                int lly = 0,
                PlaceStatus place_status = UNPLACED_,
                BlockOrient orient = N_);
  void AddBlock(std::string &block_name,
                std::string &block_type_name,
                int llx = 0,
                int lly = 0,
                PlaceStatus place_status = UNPLACED_,
                BlockOrient orient = N_);
  void ReportBlockList();
  void ReportBlockMap();

  /****API for IOPIN
   * These are PINS section in DEF
   * ****/
  std::vector<IOPin> *GetIOPinList();
  void AddDummyIOPinType();
  bool IsIOPinExist(std::string &iopin_name);
  int IOPinIndex(std::string &iopin_name);
  IOPin *GetIOPin(std::string &iopin_name);
  IOPin *AddIOPin(std::string &iopin_name);
  IOPin *AddIOPin(std::string &iopin_name, int lx, int ly);
  void ReportIOPin();

  /****API for Nets
   * These are NETS section in DEF
   * ****/
  std::vector<Net> *GetNetList();
  bool IsNetExist(std::string &net_name);
  int NetIndex(std::string &net_name);
  Net *GetNet(std::string &net_name);
  void AddToNetMap(std::string &net_name);
  Net *AddNet(std::string &net_name, int capacity, double weight);
  void ReportNetList();
  void ReportNetMap();
  void InitNetFanoutHisto(std::vector<int> *histo_x = nullptr);
  void UpdateNetHPWLHisto();
  void ReportNetFanoutHisto();

  /****Utility functions related to netlist management****/
  void NetListPopBack();

  void ReportBriefSummary() const;

  /****API to add N/P-well technology information
   * These are for CELL file
   * ****/
  BlockTypeCluster *AddBlockTypeCluster();
  BlockTypeWell *AddBlockTypeWell(BlockTypeCluster *cluster, BlockType *blk_type, bool is_plug);
  BlockTypeWell *AddBlockTypeWell(BlockTypeCluster *cluster, std::string &blk_type_name, bool is_plug);
  void SetNWellParams(double width, double spacing, double op_spacing, double max_plug_dist, double overhang);
  void SetPWellParams(double width, double spacing, double op_spacing, double max_plug_dist, double overhang);
  void SetLegalizerSpacing(double same_spacing, double any_spacing);
  Tech *GetTech();
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
  int MinBlkWidth() const;
  int MaxBlkWidth() const;
  int MinBlkHeight() const;
  int MaxBlkHeight() const;
  long int TotBlkArea() const;
  int TotBlkNum() const;
  int TotMovableBlockNum() const;
  int TotFixedBlkCnt() const;
  double AveBlkWidth() const;
  double AveBlkHeight() const;
  double AveBlkArea() const;
  double AveMovBlkWidth() const;
  double AveMovBlkHeight() const;
  double AveMovBlkArea() const;
  double WhiteSpaceUsage() const;

  /****Utility member functions****/
  void NetSortBlkPin();
  // calculating HPWL from precise pin locations
  double HPWLX();
  double HPWLY();
  double HPWL();
  void ReportHPWL();
  void ReportHPWLHistogramLinear(int bin_num = 10);
  void ReportHPWLHistogramLogarithm(int bin_num = 10);
  // calculating HPWL from the center of cells
  double HPWLCtoCX();
  double HPWLCtoCY();
  double HPWLCtoC();
  void ReportHPWLCtoC();

  /****dump placement results to various file formats****/
  void WriteDefFileDebug(std::string const &name_of_file = "circuit.def");
  void GenMATLABScript(std::string const &name_of_file = "block_net_list.m");
  void GenMATLABTable(std::string const &name_of_file = "block.txt", bool only_well_tap = false);
  void GenMATLABWellTable(std::string const &name_of_file = "res", bool only_well_tap = false);
  void SaveDefFile(std::string const &name_of_file, std::string const &def_file_name, bool is_complete_version = true);
  void SaveDefFile(std::string const &base_name, std::string const &name_padding, std::string const &def_file_name, int save_floorplan, int save_cell, int save_iopin, int save_net);
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

inline double Circuit::GetGridValueX() const {
  return tech_.grid_value_x_;
} // unit in micro

inline double Circuit::GetGridValueY() const {
  return tech_.grid_value_y_;
}

inline void Circuit::SetRowHeight(double row_height) {
  Assert(row_height > 0, "Setting row height to a negative value?");
  tech_.row_height_set_ = true;
  tech_.row_height_ = row_height;
}

inline double Circuit::GetDBRowHeight() const {
  return tech_.row_height_;
}

inline int Circuit::GetIntRowHeight() const {
  if (tech_.row_height_set_) {
    return (int) std::round(tech_.row_height_ / tech_.grid_value_y_);
  } else {
    std::cout << "Row height not set, cannot retrieve its value\n";
    exit(1);
  }
}

inline std::vector<MetalLayer> *Circuit::MetalList() {
  return &tech_.metal_list;
}

inline std::unordered_map<std::string, int> *Circuit::MetalNameMap() {
  return &tech_.metal_name_map;
}

inline std::unordered_map<std::string, BlockType *> *Circuit::BlockTypeMap() {
  return &tech_.block_type_map;
}

inline int Circuit::RegionLLX() const {
  return design_.region_left_;
}

inline int Circuit::RegionURX() const {
  return design_.region_right_;
}

inline int Circuit::RegionLLY() const {
  return design_.region_bottom_;
}

inline int Circuit::RegionURY() const {
  return design_.region_top_;
}

inline int Circuit::RegionWidth() const {
  return design_.region_right_ - design_.region_left_;
}

inline int Circuit::RegionHeight() const {
  return design_.region_top_ - design_.region_bottom_;
}

inline std::vector<Block> *Circuit::GetBlockList() {
  return &(design_.block_list);
}

inline std::vector<IOPin> *Circuit::GetIOPinList() {
  return &(design_.iopin_list);
}

inline std::vector<Net> *Circuit::GetNetList() {
  return &(design_.net_list);
}

inline void Circuit::InitNetFanoutHisto(std::vector<int> *histo_x) {
  design_.InitNetFanoutHisto(histo_x);
  design_.net_histogram_.hpwl_unit_ = tech_.grid_value_x_;
}

inline void Circuit::ReportNetFanoutHisto() {
  UpdateNetHPWLHisto();
  design_.ReportNetFanoutHisto();
}

inline int Circuit::MinBlkWidth() const {
  return design_.blk_min_width_;
}

inline int Circuit::MaxBlkWidth() const {
  return design_.blk_max_width_;
}

inline int Circuit::MinBlkHeight() const {
  return design_.blk_min_height_;
}

inline int Circuit::MaxBlkHeight() const {
  return design_.blk_max_height_;
}

inline long int Circuit::TotBlkArea() const {
  return design_.tot_blk_area_;
}

inline int Circuit::TotBlkNum() const {
  return design_.block_list.size();
}

inline int Circuit::TotMovableBlockNum() const {
  return design_.tot_mov_blk_num_;
}

inline int Circuit::TotFixedBlkCnt() const {
  return (int) design_.block_list.size() - design_.tot_mov_blk_num_;
}

inline double Circuit::AveBlkWidth() const {
  return double(design_.tot_width_) / double(TotBlkNum());
}

inline double Circuit::AveBlkHeight() const {
  return double(design_.tot_height_) / double(TotBlkNum());
}

inline double Circuit::AveBlkArea() const {
  return double(design_.tot_blk_area_) / double(TotBlkNum());
}

inline double Circuit::AveMovBlkWidth() const {
  return double(design_.tot_mov_width_) / design_.tot_mov_blk_num_;
}

inline double Circuit::AveMovBlkHeight() const {
  return double(design_.tot_mov_height_) / design_.tot_mov_blk_num_;
}

inline double Circuit::AveMovBlkArea() const {
  return double(design_.tot_mov_block_area_) / design_.tot_mov_blk_num_;
}

inline double Circuit::WhiteSpaceUsage() const {
  return double(TotBlkArea()) / (RegionURX() - RegionLLX()) / (RegionURY() - RegionLLY());
}

inline Tech *Circuit::GetTech() {
  return &tech_;
}

inline double Circuit::HPWL() {
  return HPWLX() + HPWLY();
}

inline void Circuit::ReportHPWL() {
  printf("  Current HPWL: %e um\n", HPWL());
}

#endif //DALI_CIRCUIT_HPP
