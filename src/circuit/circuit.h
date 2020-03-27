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
#include "design.h"
#include "iopin.h"
#include "layer.h"
#include "net.h"
#include "opendb.h"
#include "status.h"
#include "tech.h"

class Circuit {
  friend class Placer;
 private:
  // statistical data of the circuit
  long int tot_width_;
  long int tot_height_;
  long int tot_blk_area_;
  long int tot_mov_width_;
  long int tot_mov_height_;
  long int tot_mov_block_area_;
  int tot_mov_blk_num_;
  int blk_min_width_;
  int blk_max_width_;
  int blk_min_height_;
  int blk_max_height_;

  // Manufacturing Grid
  bool grid_set_;
  double grid_value_x_;
  double grid_value_y_;
  double row_height_;

  Tech tech_param_;
  Design design_;
  WellInfo well_info_;
#ifdef USE_OPENDB
  odb::dbDatabase *db_;
#endif

 public:
  Circuit();
  ~Circuit();

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

  /****API to set grid value****/
  void SetGridValue(double grid_value_x, double grid_value_y);
  double GetGridValueX() const; // unit in micro
  double GetGridValueY() const;
  void SetGridUsingMetalPitch();
  void SetRowHeight(double row_height);
  double GetDBRowHeight() const;
  int GetIntRowHeight() const;

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
  std::unordered_map<std::string, BlockType *> block_type_map;
  bool IsBlockTypeExist(std::string &block_type_name);
  BlockType *GetBlockType(std::string &block_type_name);
  BlockType *AddBlockType(std::string &block_type_name, int width, int height);
  void ReportBlockType();
  void CopyBlockType(Circuit &circuit);

  /****API for DIE AREA
   * These are DIEAREA section in DEF
   * ****/
  void SetBoundary(int left, int right, int bottom, int top); // unit in grid value
  void SetDieArea(int lower_x, int upper_x, int lower_y, int upper_y); // unit in um
  int Left() const;
  int Right() const;
  int Bottom() const;
  int Top() const;

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
  void SetNWellParams(double width, double spacing, double op_spacing, double max_plug_dist, double overhang);
  void SetPWellParams(double width, double spacing, double op_spacing, double max_plug_dist, double overhang);
  void SetLegalizerSpacing(double same_spacing, double any_spacing);
  Tech *GetTech();
  void ReadCellFile(std::string const &name_of_file);
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
  return grid_value_x_;
} // unit in micro

inline double Circuit::GetGridValueY() const {
  return grid_value_y_;
}

inline void Circuit::SetRowHeight(double row_height) {
  row_height_ = row_height;
}

inline double Circuit::GetDBRowHeight() const {
  return row_height_;
}

inline int Circuit::GetIntRowHeight() const {
  return (int) std::round(row_height_ / grid_value_y_);
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
  return blk_min_width_;
}

inline int Circuit::MaxWidth() const {
  return blk_max_width_;
}

inline int Circuit::MinHeight() const {
  return blk_min_height_;
}

inline int Circuit::MaxHeight() const {
  return blk_max_height_;
}

inline long int Circuit::TotArea() const {
  return tot_blk_area_;
}

inline int Circuit::TotBlockNum() const {
  return design_.block_list.size();
}

inline int Circuit::TotMovableBlockNum() const {
  return tot_mov_blk_num_;
}

inline int Circuit::TotFixedBlkCnt() const {
  return (int) design_.block_list.size() - tot_mov_blk_num_;
}

inline double Circuit::AveWidth() const {
  return double(tot_width_) / double(TotBlockNum());
}

inline double Circuit::AveHeight() const {
  return double(tot_height_) / double(TotBlockNum());
}

inline double Circuit::AveArea() const {
  return double(tot_blk_area_) / double(TotBlockNum());
}

inline double Circuit::AveMovWidth() const {
  return double(tot_mov_width_) / tot_mov_blk_num_;
}

inline double Circuit::AveMovHeight() const {
  return double(tot_mov_height_) / tot_mov_blk_num_;
}

inline double Circuit::AveMovArea() const {
  return double(tot_mov_block_area_) / tot_mov_blk_num_;
}

inline double Circuit::WhiteSpaceUsage() const {
  return double(TotArea()) / (Right() - Left()) / (Top() - Bottom());
}

inline Tech *Circuit::GetTech() {
  return &tech_param_;
}

#endif //DALI_CIRCUIT_HPP
