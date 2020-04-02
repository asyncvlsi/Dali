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
  int Left() const;
  int Right() const;
  int Bottom() const;
  int Top() const;
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
                BlockOrient orient = N);
  void AddBlock(std::string &block_name,
                std::string &block_type_name,
                int llx = 0,
                int lly = 0,
                bool movable = true,
                BlockOrient orient = N);
  void AddBlock(std::string &block_name,
                BlockType *block_type,
                int llx = 0,
                int lly = 0,
                PlaceStatus place_status = UNPLACED,
                BlockOrient orient = N);
  void AddBlock(std::string &block_name,
                std::string &block_type_name,
                int llx = 0,
                int lly = 0,
                PlaceStatus place_status = UNPLACED,
                BlockOrient orient = N);
  void ReportBlockList();
  void ReportBlockMap();

  /****API for IOPIN
   * These are PINS section in DEF
   * ****/
  std::vector<IOPin> *GetIOPinList();
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
  std::vector<Net> *GetNetList();
  bool IsNetExist(std::string &net_name);
  int NetIndex(std::string &net_name);
  Net *GetNet(std::string &net_name);
  void AddToNetMap(std::string &net_name);
  Net *AddNet(std::string &net_name, double weight = 1);
  void ReportNetList();
  void ReportNetMap();

  /****Utility functions related to netlist management****/
  void NetListPopBack();

  void ReportBriefSummary();

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
  int MinWidth() const;
  int MaxWidth() const;
  int MinHeight() const;
  int MaxHeight() const;
  long int TotArea() const;
  int TotBlockNum() const;
  int TotMovableBlockNum() const;
  int TotFixedBlkCnt() const;
  double AveWidth() const;
  double AveHeight() const;
  double AveArea() const;
  double AveMovWidth() const;
  double AveMovHeight() const;
  double AveMovArea() const;
  double WhiteSpaceUsage() const;

  /****Utility member functions****/
  void NetSortBlkPin();
  // calculating HPWL from precise pin locations
  double HPWLX();
  double HPWLY();
  double HPWL();
  void ReportHPWL();
  // calculating HPWL from the center of cells
  double HPWLCtoCX();
  double HPWLCtoCY();
  double HPWLCtoC();
  void ReportHPWLCtoC();

  /****dump placement results to various file formats****/
  void WriteDefFileDebug(std::string const &name_of_file = "circuit.def");
  void GenMATLABScript(std::string const &name_of_file = "block_net_list.m");
  void GenMATLABTable(std::string const &name_of_file = "block.txt");
  void GenMATLABWellTable(std::string const &name_of_file = "res");
  void SaveDefFile(std::string const &name_of_file, std::string const &def_file_name);

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
  tech_.row_height_ = row_height;
}

inline double Circuit::GetDBRowHeight() const {
  return tech_.row_height_;
}

inline int Circuit::GetIntRowHeight() const {
  return (int) std::round(tech_.row_height_ / tech_.grid_value_y_);
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

inline int Circuit::Left() const {
  return design_.def_left;
}

inline int Circuit::Right() const {
  return design_.def_right;
}

inline int Circuit::Bottom() const {
  return design_.def_bottom;
}

inline int Circuit::Top() const {
  return design_.def_top;
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

inline int Circuit::MinWidth() const {
  return design_.blk_min_width_;
}

inline int Circuit::MaxWidth() const {
  return design_.blk_max_width_;
}

inline int Circuit::MinHeight() const {
  return design_.blk_min_height_;
}

inline int Circuit::MaxHeight() const {
  return design_.blk_max_height_;
}

inline long int Circuit::TotArea() const {
  return design_.tot_blk_area_;
}

inline int Circuit::TotBlockNum() const {
  return design_.block_list.size();
}

inline int Circuit::TotMovableBlockNum() const {
  return design_.tot_mov_blk_num_;
}

inline int Circuit::TotFixedBlkCnt() const {
  return (int) design_.block_list.size() - design_.tot_mov_blk_num_;
}

inline double Circuit::AveWidth() const {
  return double(design_.tot_width_) / double(TotBlockNum());
}

inline double Circuit::AveHeight() const {
  return double(design_.tot_height_) / double(TotBlockNum());
}

inline double Circuit::AveArea() const {
  return double(design_.tot_blk_area_) / double(TotBlockNum());
}

inline double Circuit::AveMovWidth() const {
  return double(design_.tot_mov_width_) / design_.tot_mov_blk_num_;
}

inline double Circuit::AveMovHeight() const {
  return double(design_.tot_mov_height_) / design_.tot_mov_blk_num_;
}

inline double Circuit::AveMovArea() const {
  return double(design_.tot_mov_block_area_) / design_.tot_mov_blk_num_;
}

inline double Circuit::WhiteSpaceUsage() const {
  return double(TotArea()) / (Right() - Left()) / (Top() - Bottom());
}

inline Tech *Circuit::GetTech() {
  return &tech_;
}

#endif //DALI_CIRCUIT_HPP
