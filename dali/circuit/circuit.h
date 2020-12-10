//
// Created by Yihang Yang on 2019-03-26.
//

#ifndef DALI_CIRCUIT_HPP
#define DALI_CIRCUIT_HPP

#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include <boost/functional/hash.hpp>

#include "blkpairnets.h"
#include "block.h"
#include "blocktype.h"
#include "blocktypewell.h"
#include "dali/common/logging.h"
#include "design.h"
#include "iopin.h"
#include "layer.h"
#include "net.h"
#include "status.h"
#include "tech.h"

namespace dali {

}
/****
 * The class Circuit is an abstract of a circuit graph.
 * It contains two main parts:
 *  1. technology, this part contains information in LEF and CELL
 *  2. design, this part contains information in DEF
 *
 * To create a Circuit instance, one can simply do:
 * a).
 *  Circuit circuit;
 *  circuit.InitializeFromDB(opendb_ptr);
 * b).
 *  Circuit circuit(opendb_ptr);
 *
 * To build a Circuit instance using API, one need to follow these major steps in sequence:
 *  1. set lef database microns
 *  2. set manufacturing grid and create metals
 *  3. set grid value in x and y direction
 *  4. create all macros
 *  5. create all INSTANCEs
 *  6. create all IOPINs
 *  7. create all NETs
 *  8. create well rect for macros
 * To know more detail about how to do this, take a look at comments in member function void InitializeFromDB().
 * ****/

namespace dali {

class Circuit {
  friend class Placer;
 private:
  Tech tech_; // information in LEF and CELL
  Design design_; // information in DEF

#ifdef USE_OPENDB
  odb::dbDatabase *db_ptr_; // pointer to openDB database
#endif

  // create fake N/P-well info for cells
  void LoadImaginaryCellFile();

  // custom residual function, return: x - round(x/y) * y
  static double Residual(double x, double y) {
    return x - std::round(x / y) * y;
  }

  // add a BlockType with name, with, and height. The return value is a pointer to this new BlockType for adding pins. Unit in grid value.
  BlockType *AddBlockTypeWithGridUnit(std::string &block_type_name, int width, int height);

  // add a BlockType with name, with, and height. The return value is a pointer to this new BlockType for adding pins. Unit in grid value.
  BlockType *AddWellTapBlockTypeWithGridUnit(std::string &block_type_name, int width, int height);

  // add a metal layer with name and size, unit in micron.
  MetalLayer *AddMetalLayer(std::string &metal_name, double width, double spacing);

  // add a metal layer with name;
  MetalLayer *AddMetalLayer(std::string &metal_name) { return AddMetalLayer(metal_name, 0, 0); }

  // set the boundary of the placement region, unit is in corresponding grid value.
  void SetBoundary(int left, int bottom, int right, int top) {
    DaliExpects(right > left, "Right boundary is not larger than Left boundary?");
    DaliExpects(top > bottom, "Top boundary is not larger than Bottom boundary?");
    design_.region_left_ = left;
    design_.region_right_ = right;
    design_.region_bottom_ = bottom;
    design_.region_top_ = top;
    design_.die_area_set_ = true;
  }

  // create a block instance using a pointer to its type
  void AddBlock(std::string &block_name,
                BlockType *block_type_ptr,
                int llx = 0,
                int lly = 0,
                PlaceStatus place_status = UNPLACED_,
                BlockOrient orient = N_,
                bool is_real_cel = true);

  // create a dummy BlockType for IOPins.
  void AddDummyIOPinBlockType();

  // add an unplaced IOPin.
  IOPin *AddUnplacedIOPin(std::string &iopin_name);

  // add a placed IOPin.
  IOPin *AddPlacedIOPin(std::string &iopin_name, int lx, int ly);

  // create well information container for a given BlockType.
  BlockTypeWell *AddBlockTypeWell(BlockType *blk_type);

  /****static functions****/
  // splits a line into many words
  static void StrSplit(std::string &line, std::vector<std::string> &res);

  // finds the first number in a string.
  static int FindFirstNumber(std::string &str);

 public:

  Circuit();
  std::vector<BlkPairNets> blk_pair_net_list_; // lower triangle of the driver-load pair
  std::unordered_map<std::pair<int, int>, int, boost::hash<std::pair<int, int>>> blk_pair_map_;

  /****API to initialize circuit
   * 1. from openDB
   * 2. from LEF/DEF directly using a naive parser
   * ****/
#ifdef USE_OPENDB
  // constructor using openDB database
  explicit Circuit(odb::dbDatabase *db_ptr);

  // initialize a blank circuit from openDB database
  void InitializeFromDB(odb::dbDatabase *db_ptr);
#endif

  // simple LEF parser, do not recommend to use
  void ReadLefFile(std::string const &name_of_file);

  // simple DEF parser, do not recommend to use
  void ReadDefFile(std::string const &name_of_file);

  // simple CELL parser
  void ReadCellFile(std::string const &name_of_file);

  /****API to retrieve technology and design****/
  // get technology info
  Tech *getTech() { return &tech_; }
  Tech &getTechRef() { return tech_; }

  // get design info
  Design *getDesign() { return &design_; }
  Design &getDesignRef() { return design_; }

  /************************************************
   * The following APIs are for in LEF
   * ************************************************/
  /****API to set and get database unit****/
  // set database microns
  void setDatabaseMicron(int database_micron) {
    DaliExpects(database_micron > 0, "Cannot set negative database microns: Circuit::setDatabaseMicron()");
    tech_.database_microns_ = database_micron;
  }

  int DatabaseMicron() const { return tech_.database_microns_; }

  // set manufacturing grid
  void setManufacturingGrid(double manufacture_grid) {
    DaliExpects(manufacture_grid > 0, "Cannot set negative manufacturing grid: Circuit::setManufacturingGrid()");
    tech_.manufacturing_grid_ = manufacture_grid;
  }

  // get manufacturing grid
  double ManufacturingGrid() const { return tech_.manufacturing_grid_; }

  /****API to set grid value****/
  // set grid values in x and y direction, unit in micron
  void setGridValue(double grid_value_x, double grid_value_y);

  // set grid values using the first horizontal and vertical metal layer pitches
  void setGridUsingMetalPitch();

  // get the grid value in x direction, unit is usually in micro
  double GridValueX() const {
    DaliExpects(tech_.grid_set_, "Need to set grid value before use");
    return tech_.grid_value_x_;
  }

  // get the grid value in y direction, unit is usually in micro
  double GridValueY() const {
    DaliExpects(tech_.grid_set_, "Need to set grid value before use");
    return tech_.grid_value_y_;
  }

  // set the row height, unit in micron
  void setRowHeight(double row_height) {
    DaliExpects(row_height > 0, "Setting row height to a negative value? Circuit::setRowHeight()");
    double residual = Residual(row_height, tech_.grid_value_y_);
    DaliExpects(std::fabs(residual) < 1e-6, "Site height is not integer multiple of grid value in Y");
    tech_.row_height_set_ = true;
    tech_.row_height_ = row_height;
  }

  // get the row height in micro
  double getDBRowHeight() const { return tech_.row_height_; }

  // get the row height in grid value y
  int getINTRowHeight() const {
    DaliExpects(tech_.row_height_set_, "Row height not set, cannot retrieve its value: Circuit::getINTRowHeight()\n");
    return (int) std::round(tech_.row_height_ / tech_.grid_value_y_);
  }

  /****API to set metal layers: deprecated
   * now the metal layer information are all stored in openDB data structure
   * ****/
  // get the pointer to the list of metal layers
  std::vector<MetalLayer> *MetalListPtr() { return &tech_.metal_list_; }

  // get the pointer to the name map of metal layers
  std::unordered_map<std::string, int> *MetalNameMap() { return &tech_.metal_name_map_; }

  // check if a metal layer with given name exists or not
  bool IsMetalLayerExist(std::string &metal_name) {
    return tech_.metal_name_map_.find(metal_name) != tech_.metal_name_map_.end();
  }

  // get the index of a metal layer
  int MetalLayerIndex(std::string &metal_name) {
    DaliExpects(IsMetalLayerExist(metal_name), "MetalLayer does not exist, cannot find it: " + metal_name);
    return tech_.metal_name_map_.find(metal_name)->second;
  }

  // get a pointer to the metal layer with a given name
  MetalLayer *getMetalLayerPtr(std::string &metal_name) {
    DaliExpects(IsMetalLayerExist(metal_name), "MetalLayer does not exist, cannot find it: " + metal_name);
    return &tech_.metal_list_[MetalLayerIndex(metal_name)];
  }

  // add a metal layer, unit in micron.
  void AddMetalLayer(std::string &metal_name,
                     double width,
                     double spacing,
                     double min_area,
                     double pitch_x,
                     double pitch_y,
                     MetalDirection metal_direction);

  // report metal layer information for debugging purposes
  void ReportMetalLayers();

  /****API for BlockType****/

  // get the pointer to the unordered BlockType map.
  std::unordered_map<std::string, BlockType *> *BlockTypeMap() { return &tech_.block_type_map_; }

  // check if a BlockType with a given name exists or not
  bool IsBlockTypeExist(std::string &block_type_name) {
    return tech_.block_type_map_.find(block_type_name) != tech_.block_type_map_.end();
  }

  // get the pointer to the BlockType with a given name, if not exist, return nullptr.
  BlockType *getBlockType(std::string &block_type_name) {
    auto res = tech_.block_type_map_.find(block_type_name);
    return res != tech_.block_type_map_.end() ? res->second : nullptr;
  }

  // add a BlockType with name, with, and height. The return value is a pointer to this new BlockType for adding pins. Unit in micron.
  BlockType *AddBlockType(std::string &block_type_name, double width, double height);

  void SetBlockTypeSize(BlockType *blk_type_ptr, double width, double height);

  // add a BlockType with name, with, and height. The return value is a pointer to this new BlockType for adding pins. Unit in micron.
  BlockType *AddWellTapBlockType(std::string &block_type_name, double width, double height);

  // add a cell pin with a given name to a BlockType, this method is not the optimal one, but it is very safe to use.
  Pin *AddBlkTypePin(std::string &block_type_name, std::string &pin_name, bool is_input) {
    BlockType *blk_type_ptr = getBlockType(block_type_name);
    DaliExpects(blk_type_ptr != nullptr,
                "Cannot add BlockType pins because there is no such a BlockType: " + block_type_name);
    return blk_type_ptr->AddPin(pin_name, is_input);
  }

  // add a cell pin with a given name to a BlockType, users must guarantee the pointer @param is valid.
  Pin *AddBlkTypePin(BlockType *blk_type_ptr, std::string &pin_name, bool is_input) {
    return blk_type_ptr->AddPin(pin_name, is_input);
  }

  // add a rectangle to a block pin, this method is not the optimal one, but it is very safe to use. Unit in grid value.
  void AddBlkTypePinRect(std::string &block_type_name,
                         std::string &pin_name,
                         double llx,
                         double lly,
                         double urx,
                         double ury) {
    BlockType *blk_type_ptr = getBlockType(block_type_name);
    DaliExpects(blk_type_ptr != nullptr,
                "Cannot add BlockType pins because there is no such a BlockType: " + block_type_name);
    Pin *pin_ptr = blk_type_ptr->getPinPtr(pin_name);
    DaliExpects(pin_ptr != nullptr,
                "Cannot add BlockType pins because there is no such a pin: " + block_type_name + "::" + pin_name);
    pin_ptr->AddRect(llx, lly, urx, ury);
  }

  // add a rectangle to a block pin, users must guarantee the pointer @param is valid. Unit in grid value.
  void AddBlkTypePinRect(Pin *pin_ptr, double llx, double lly, double urx, double ury) {
    pin_ptr->AddRect(llx, lly, urx, ury);
  }

  // report the whole BlockType list for debugging purposes.
  void ReportBlockType();

  // create BlockTypes by copying from another Circuit instance.
  void CopyBlockType(Circuit &circuit);

  /************************************************
   * End of APIs for LEF
   * ************************************************/

  /************************************************
   * The following APIs are for DEF
   * ************************************************/

  /****API for DIE AREA
   * These are DIEAREA section in DEF
   * ****/

  // set DEF UNITS DISTANCE MICRONS
  void setUnitsDistanceMicrons(int distance_microns) {
    DaliExpects(distance_microns > 0, "Negative distance micron?");
    design_.def_distance_microns = distance_microns;
  }

  // return DEF UNITS DISTANCE MICRONS
  int DistanceMicrons() const { return design_.def_distance_microns; }

  // set die area, unit in manufacturing grid
  void setDieArea(int lower_x, int lower_y, int upper_x, int upper_y) { // unit in manufacturing grid
    DaliExpects(tech_.grid_value_x_ > 0 && tech_.grid_value_y_ > 0,
                "Need to set positive grid values before setting placement boundary");
    DaliExpects(design_.def_distance_microns > 0,
                "Need to set def_distance_microns before setting placement boundary using Circuit::SetDieArea()");
    double factor_x = tech_.grid_value_x_ * design_.def_distance_microns;
    double factor_y = tech_.grid_value_y_ * design_.def_distance_microns;
    SetBoundary((int) std::round(lower_x / factor_x),
                (int) std::round(lower_y / factor_y),
                (int) std::round(upper_x / factor_x),
                (int) std::round(upper_y / factor_y));
  }

  // return lower x of the placement region, unit is grid value in x.
  int RegionLLX() const { return design_.region_left_; }

  // return upper x of the placement region, unit is grid value in x.
  int RegionURX() const { return design_.region_right_; }

  // return lower y of the placement region, unit is grid value in y.
  int RegionLLY() const { return design_.region_bottom_; }

  // return upper y of the placement region, unit is grid value in y.
  int RegionURY() const { return design_.region_top_; }

  // return width of the placement region, unit is grid value in x.
  int RegionWidth() const { return design_.region_right_ - design_.region_left_; }

  // return height of the placement region, unit is grid value in y.
  int RegionHeight() const { return design_.region_top_ - design_.region_bottom_; }

  // Before adding COMPONENTs, IOPINs, and NETs, user need to specify how many instances there are.
  void setListCapacity(int components_count, int pins_count, int nets_count);

  /****API for Block
   * These are COMPONENTS section in DEF
   * ****/

  // get the pointer to the block list.
  std::vector<Block> *getBlockList() { return &(design_.block_list); }
  std::vector<Block> &BlockListRef() { return design_.block_list; }

  // check if a block with the given name exists or not.
  bool IsBlockExist(std::string &block_name) {
    return !(design_.block_name_map.find(block_name) == design_.block_name_map.end());
  }

  // returns the index of a block with a given name. Users must guarantee the given name is valid.
  int BlockIndex(std::string &block_name) {
    return design_.block_name_map.find(block_name)->second;
  }

  // returns a pointer to the block with a given name. Users must guarantee the given name is valid.
  Block *getBlockPtr(std::string &block_name) {
    return &design_.block_list[BlockIndex(block_name)];
  }

  // create a block instance using the name of its type
  void AddBlock(std::string &block_name,
                std::string &block_type_name,
                int llx = 0,
                int lly = 0,
                PlaceStatus place_status = UNPLACED_,
                BlockOrient orient = N_,
                bool is_real_cel = true);

  // report the whole Block list for debugging purposes.
  void ReportBlockList();

  // report the whole Block map for debugging purposes.
  void ReportBlockMap();

  /****API for IOPIN
   * These are PINS section in DEF
   * ****/

  // get the pointer to the IOPin list.
  std::vector<IOPin> *getIOPinList() { return &(design_.iopin_list); }

  // check if an IOPin with a given name exists or not.
  bool IsIOPinExist(std::string &iopin_name) {
    return !(design_.iopin_name_map.find(iopin_name) == design_.iopin_name_map.end());
  }

  // returns the index of the IOPin with a given name. Users must guarantee the given name is valid.
  int IOPinIndex(std::string &iopin_name) {
    return design_.iopin_name_map.find(iopin_name)->second;
  }

  // returns a pointer to the IOPin with a given name. Users must guarantee the given name is valid.
  IOPin *getIOPin(std::string &iopin_name) {
    return &design_.iopin_list[IOPinIndex(iopin_name)];
  }

  // add an INPin
  IOPin *AddIOPin(std::string &iopin_name,
                  PlaceStatus place_status,
                  SignalUse signal_use,
                  SignalDirection signal_direction,
                  int lx = 0,
                  int ly = 0);

  // report the whole IOPin list for debugging purposes.
  void ReportIOPin();

  /****API for Nets
   * These are NETS section in DEF
   * ****/

  // get the pointer to the net list.
  std::vector<Net> *getNetList() { return &(design_.net_list); }
  std::vector<Net> &NetListRef() { return design_.net_list; }

  // check if a Net with a given name exists or not.
  bool IsNetExist(std::string &net_name) {
    return !(design_.net_name_map.find(net_name) == design_.net_name_map.end());
  }

  // returns the index of the Net with a given name. Users must guarantee the given name is valid.
  int NetIndex(std::string &net_name) {
    return design_.net_name_map.find(net_name)->second;
  }

  // returns a pointer to the Net with a given name. Users must guarantee the given name is valid.
  Net *getNetPtr(std::string &net_name) {
    return &design_.net_list[NetIndex(net_name)];
  }

  // add a net with given name and capacity (number of cell pins), net weight is default 1.
  Net *AddNet(std::string &net_name, int capacity, double weight = 1);

  // add a IOPin to a net.
  void AddIOPinToNet(std::string &iopin_name, std::string &net_name);

  // add a block pin to a net.
  void AddBlkPinToNet(std::string &blk_name, std::string &pin_name, std::string &net_name);

  // report the net list for debugging purposes.
  void ReportNetList();

  // report the net map for debugging purposes.
  void ReportNetMap();

  // initialize data structure for net fanout histogram
  void InitNetFanoutHistogram(std::vector<int> *histo_x = nullptr) {
    design_.InitNetFanoutHisto(histo_x);
    design_.net_histogram_.hpwl_unit_ = tech_.grid_value_x_;
  }

  // update HPWL values for the net fanout histogram
  void UpdateNetHPWLHistogram();

  // report the net fanout histogram
  void ReportNetFanoutHistogram() {
    UpdateNetHPWLHistogram();
    design_.ReportNetFanoutHisto();
  }

  /************************************************
   * End of API for DEF
   * ************************************************/

  // report brief summary of this circuit.
  void ReportBriefSummary() const;

  /************************************************
   * The following APIs are for in LEF
   * ************************************************/

  // set N-well layer parameters
  void SetNWellParams(double width, double spacing, double op_spacing, double max_plug_dist, double overhang) {
    tech_.SetNLayer(width, spacing, op_spacing, max_plug_dist, overhang);
  }

  // set P-well layer parameters
  void SetPWellParams(double width, double spacing, double op_spacing, double max_plug_dist, double overhang) {
    tech_.SetPLayer(width, spacing, op_spacing, max_plug_dist, overhang);
  }

  // TODO: discuss with Rajit about the necessity of the parameter ANY_SPACING in CELL file.
  // set same_spacing (NN and PP) and any_spacing (NP). This member function will be depreciated
  void SetLegalizerSpacing(double same_spacing, double any_spacing) {
    tech_.SetDiffSpacing(same_spacing, any_spacing);
  }

  // create well information container for a given BlockType.
  BlockTypeWell *AddBlockTypeWell(std::string &blk_type_name) {
    BlockType *blk_type_ptr = getBlockType(blk_type_name);
    return AddBlockTypeWell(blk_type_ptr);
  }

  // TODO: discuss with Rajit about the necessity of having N/P-wells not fully covering the prBoundary of a given cell.
  // set the N/P-well shape of a given BlockType, unit in micron.
  void setWellRect(std::string &blk_type_name, bool is_N, double lx, double ly, double ux, double uy);

  // report the well shape for each BlockType for debugging purposes.
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

  /****member functions to obtain some useful values****/

  // returns the minimum width of blocks.
  int MinBlkWidth() const { return design_.blk_min_width_; }

  // returns the maximum width of blocks, for checking if the placement region is too narrow, and so on.
  int MaxBlkWidth() const { return design_.blk_max_width_; }

  // returns the minimum height of blocks.
  int MinBlkHeight() const { return design_.blk_min_height_; }

  // returns the maximum height of blocks, for checking if the placement region is too short, and so on.
  int MaxBlkHeight() const { return design_.blk_max_height_; }

  // returns the total block area, for checking if the placement region is big enough.
  long int TotBlkArea() const { return design_.tot_blk_area_; }

  // returns the total number of blocks.
  int TotBlkCount() const { return design_.blk_count_; }

  // returns the total number of movable blocks.
  int TotMovableBlockCount() const { return design_.tot_mov_blk_num_; }

  // returns the total number of fixed blocks.
  int TotFixedBlkCnt() const { return (int) design_.block_list.size() - design_.tot_mov_blk_num_; }

  // returns the average width of blocks.
  double AveBlkWidth() const { return double(design_.tot_width_) / double(TotBlkCount()); }

  // returns the average height of blocks.
  double AveBlkHeight() const { return double(design_.tot_height_) / double(TotBlkCount()); }

  // returns the average area of blocks.
  double AveBlkArea() const { return double(design_.tot_blk_area_) / double(TotBlkCount()); }

  // returns the average width of movable blocks.
  double AveMovBlkWidth() const { return double(design_.tot_mov_width_) / TotMovableBlockCount(); }

  // returns the average height of movable blocks.
  double AveMovBlkHeight() const { return double(design_.tot_mov_height_) / TotMovableBlockCount(); }

  // returns the average area of movable blocks.
  double AveMovBlkArea() const { return double(design_.tot_mov_block_area_) / TotMovableBlockCount(); }

  // returns the white space usage ratio.
  double WhiteSpaceUsage() const {
    return double(TotBlkArea()) / double(RegionURX() - RegionLLX()) / double(RegionURY() - RegionLLY());
  }

  /****Utility member functions****/
  void BuildBlkPairNets();
  void NetSortBlkPin();

  // returns HPWL in the x direction, considering cell pin offsets, unit in micron.
  double WeightedHPWLX();

  // returns HPWL in the y direction, considering cell pin offsets, unit in micron.
  double WeightedHPWLY();

  // returns total HPWL, considering cell pin offsets, unit in micron.
  double WeightedHPWL() { return WeightedHPWLX() + WeightedHPWLY(); }

  // simple function to report HPWL.
  void ReportHPWL() {
    BOOST_LOG_TRIVIAL(info) << "  Current weighted HPWL: " << WeightedHPWL() << " um";
  }

  // report the histogram of HPWL using linear bins.
  void ReportHPWLHistogramLinear(int bin_num = 10);

  // report the histogram of HPWL using logarithmic bins.
  void ReportHPWLHistogramLogarithm(int bin_num = 10);

  // create a file to save distance to optimal region for each cell
  void SaveOptimalRegionDistance(std::string file_name = "optimal_region_distance.txt");

  // returns HPWL in the x direction, assuming cell pins are in the cell, unit in micron.
  double HPWLCtoCX();

  // returns HPWL in the y direction, assuming cell pins are in the cell, unit in micron.
  double HPWLCtoCY();

  // returns total HPWL, assuming cell pins are in the cell, unit in micron.
  double HPWLCtoC() { return HPWLCtoCX() + HPWLCtoCY(); }

  // simple function to report HPWL.
  void ReportHPWLCtoC() { BOOST_LOG_TRIVIAL(info)  <<"  Current HPWL: "<< HPWLCtoC() << " um"; }

  /****save placement results to various file formats****/
  void GenMATLABTable(std::string const &name_of_file = "block.txt", bool only_well_tap = false);
  void GenMATLABWellTable(std::string const &name_of_file = "res", bool only_well_tap = false);
  void GenLongNetTable(std::string const &name_of_file);
  void SaveDefFile(std::string const &name_of_file, std::string const &def_file_name, bool is_complete_version = true);

  // save def file for users.
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
};

}

#endif //DALI_CIRCUIT_HPP
