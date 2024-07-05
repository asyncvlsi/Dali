/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#ifndef DALI_CIRCUIT_CIRCUIT_H_
#define DALI_CIRCUIT_CIRCUIT_H_

#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/functional/hash.hpp>
#include <phydb/phydb.h>

#include "block.h"
#include "block_type.h"
#include "dali/common/helper.h"
#include "dali/common/logging.h"
#include "design.h"
#include "enums.h"
#include "iopin.h"
#include "layer.h"
#include "net.h"
#include "tech.h"

namespace dali {

/****
 * A collections of constant numbers used in class Circuit.
 */
struct CircuitConstants {
  double normal_net_weight = 1.0;
  double epsilon = 1e-6;
};

/****
 * The class Circuit is an abstract of a circuit graph.
 * It contains two main parts:
 *  1. technology, this part contains information in LEF and CELL
 *  2. design, this part contains information in DEF
 *
 * To create a Circuit instance, one can simply do:
 *  Circuit circuit;
 *  circuit.InitializeFromPhyDB(phydb_ptr);
 *
 * To build a Circuit instance using API, one need to follow these major steps in sequence:
 *  - set lef database microns
 *  - set manufacturing grid and create metals
 *  - set grid value in x and y direction
 *  - create all macros
 *  - specify the number of COMPONENTs, IOPINs, and NETS
 *  - create all COMPONENTs
 *  - create all IOPINs
 *  - create all NETs
 *  - create well rect for macros
 * To know more detail about how to do this, take a look at comments in member function void InitializeFromPhyDB().
 * ****/
class Circuit {
  friend class Placer;
  friend class GlobalPlacer;
 public:
  Circuit();

  /**** API talking to PhyDB ****/
  // initialize data structure from a PhyDB, which should live longer than Circuit!
  void InitializeFromPhyDB(phydb::PhyDB *phy_db_ptr);

  // convert length from um to database unit
  int Micron2DatabaseUnit(double x) const;

  // convert length from database unit to um
  double DatabaseUnit2Micron(int x) const;

  // Dali to PhyDB distance scale factor x
  int DistanceScaleFactorX() const;

  // Dali to PhyDB distance scale factor y
  int DistanceScaleFactorY() const;

  // convert location in a design from Dali to PhyDB
  int LocDali2PhydbX(double loc) const;

  // convert location in a design from Dali to PhyDB
  int LocDali2PhydbY(double loc) const;

  // convert location in a design from PhyDB to Dali
  double LocPhydb2DaliX(int loc) const;

  // convert location in a design from PhyDB to Dali
  double LocPhydb2DaliY(int loc) const;

  // convert length in um to grid unit
  double LengthPhydb2DaliX(double length) const;

  // convert length in um to grid unit
  double LengthPhydb2DaliY(double length) const;

  /**** API to retrieve technology and design, this can be easily extended to support multiple techs and designs ****/
  // get the tech
  Tech &tech();

  // get the design
  Design &design();

  /************************************************
   * The following APIs are for information in LEF
   * ************************************************/
  /**** API to set and get database unit ****/
  // set database microns
  void SetDatabaseMicrons(int database_micron);

  // get database microns
  int DatabaseMicrons() const;

  // set manufacturing grid
  void SetManufacturingGrid(double manufacture_grid);

  // get manufacturing grid
  double ManufacturingGrid() const;

  /**** API to set grid value ****/
  // set grid values in x and y direction, unit in micron
  void SetGridValue(double grid_value_x, double grid_value_y);

  // set grid values using the first horizontal and vertical metal layer pitches
  void SetGridFromMetalPitch();

  // get the grid value in x direction, unit is um
  double GridValueX() const;

  // get the grid value in y direction, unit is um
  double GridValueY() const;

  // set the row height, unit is um
  void SetRowHeight(double row_height);

  // get the row height in um
  double RowHeightMicronUnit() const;

  // get the row height in grid value y
  int RowHeightGridUnit() const;

  /**** API to set metal layers ****/
  // get all metal layers
  std::vector<MetalLayer> &Metals();

  // get the metal layer name to internal index map
  std::unordered_map<std::string, int> &MetalNameMap();

  // check if a metal layer with given name exists or not
  bool IsMetalLayerExisting(std::string const &metal_name);

  // get the index of a metal layer
  int GetMetalLayerId(std::string const &metal_name);

  // get a pointer to the metal layer with a given name
  MetalLayer *GetMetalLayerPtr(std::string const &metal_name);

  // add a metal layer, unit is um
  MetalLayer *AddMetalLayer(
      std::string const &metal_name,
      double width,
      double spacing,
      double min_area,
      double pitch_x,
      double pitch_y,
      MetalDirection metal_direction
  );

  // print metal layer information
  void ReportMetalLayers();

  /****API for BlockType****/
  std::vector<BlockType> &BlockTypes();

  // check if a BlockType with a given name exists or not
  bool IsBlockTypeExisting(std::string const &block_type_name);

  // get the pointer to the BlockType with a given name, if not exist, return a nullptr
  BlockType *GetBlockTypePtr(std::string const &block_type_name);

  // add a BlockType, width and height are in um
  BlockType *AddBlockType(
      std::string const &block_type_name,
      double width,
      double height
  );

  int GetRoundOrCeilGriddedWidth(double width, std::string const &block_type_name) const;
  int GetRoundOrCeilGriddedHeight(double height, std::string const &block_type_name) const;
  // add a BlockType for well tap cell
  int AddWellTapBlockType(
      std::string const &block_type_name,
      double width,
      double height
  );

  // add a BlockType for filler cell
  BlockType *AddFillerBlockType(
      std::string const &block_type_name,
      double width,
      double height
  );

  // add a cell pin with a given name to a BlockType
  Pin *AddBlkTypePin(
      BlockType *blk_type_ptr,
      std::string const &pin_name,
      bool is_input
  );

  // print all BlockTypes
  void ReportBlockType();

  // create BlockTypes by copying from another Circuit instance
  void CopyBlockType(Circuit &circuit);

  /************************************************
   * The following APIs are for DEF
   * ************************************************/

  /**** APIs for DIE AREA ****/
  // set DEF UNITS DISTANCE MICRONS
  void SetUnitsDistanceMicrons(int distance_microns);

  // get DEF UNITS DISTANCE MICRONS
  int DistanceMicrons() const;

  // enable shrink off grid die area
  void SetEnableShrinkOffGridDieArea(bool enable_shrink_off_grid_die_area);

  // set die area, unit in manufacturing grid
  void SetDieArea(int lower_x, int lower_y, int upper_x, int upper_y);

  // set rectilinear die area, unit in manufacturing grid;
  void SetRectilinearDieArea(std::vector<int2d> &rectilinear_die_area);

  // return lower x of the placement region, unit is grid value in x
  int RegionLLX() const;

  // return upper x of the placement region, unit is grid value in x
  int RegionURX() const;

  // return lower y of the placement region, unit is grid value in y
  int RegionLLY() const;

  // return upper y of the placement region, unit is grid value in y
  int RegionURY() const;

  // return width of the placement region, unit is grid value in x
  int RegionWidth() const;

  // return height of the placement region, unit is grid value in y
  int RegionHeight() const;

  // if placement boundary in DEF is not an integer multiple of grid value, this is the residual
  int DieAreaOffsetX() const;

  // if placement boundary in DEF is not an integer multiple of grid value, this is the residual
  int DieAreaOffsetY() const;

  // set the capacity of COMPONENTs, PINs, and NETs, it is not allowed to add more items than their corresponding capacity
  void ReserveSpaceForDesignImp(size_t components_count, size_t pins_count, size_t nets_count);

  /**** APIs for Block (COMPONENTS in DEF) ****/
  // get all blocks
  std::vector<Block> &Blocks();

  // check if a block with the given name exists or not
  bool IsBlockExisting(std::string const &block_name);

  // returns the internal index of a block with a given name
  int GetBlockId(std::string const &block_name);

  // returns a pointer to the block with a given name
  Block *GetBlockPtr(std::string const &block_name);

  // create a block instance using the name of its type
  void AddBlock(
      std::string const &block_name,
      std::string const &block_type_name,
      double llx = 0,
      double lly = 0,
      PlaceStatus place_status = UNPLACED,
      BlockOrient orient = N,
      bool is_real_cel = true
  );

  void UpdateTotalBlkArea();

  // report the whole Block list for debugging purposes
  void ReportBlockList();

  // report the whole Block map for debugging purposes
  void ReportBlockMap();

  /**** API for I/O pins (PINS in DEF) ****/
  // get I/O pins
  std::vector<IoPin> &IoPins();

  // check if an IOPin with a given name exists or not
  bool IsIoPinExisting(std::string const &iopin_name);

  // returns the index of the IOPin with a given name
  int GetIoPinId(std::string const &iopin_name);

  // returns a pointer to the IOPin with a given name
  IoPin *GetIoPinPtr(std::string const &iopin_name);

  // add an I/O pin
  IoPin *AddIoPin(
      std::string const &iopin_name,
      PlaceStatus place_status,
      SignalUse signal_use,
      SignalDirection signal_direction,
      double lx = 0,
      double ly = 0
  );

  // add an I/O pin from a PhyDB I/O pin
  void AddIoPinFromPhyDB(phydb::IOPin &iopin);

  // add a placement blockage from a PhyDB blockage
  void AddPlacementBlockageFromPhyDB(phydb::Blockage &blockage);

  // print all I/O pins
  void ReportIOPin();

  /**** API for Nets (NETS in DEF) ****/
  // get all nets
  std::vector<Net> &Nets();

  // check if a Net with a given name exists or not
  bool IsNetExisting(std::string const &net_name);

  // returns the index of the Net with a given name
  int GetNetId(std::string const &net_name);

  // returns a pointer to the Net with a given name
  Net *GetNetPtr(std::string const &net_name);

  // add a net with given name and capacity (number of cell pins)
  Net *AddNet(
      std::string const &net_name,
      size_t capacity,
      double weight = -1
  );

  // add a IoPin to a net
  void AddIoPinToNet(
      std::string const &iopin_name,
      std::string const &net_name
  );

  // add a block pin to a net
  void AddBlkPinToNet(
      std::string const &blk_name,
      std::string const &pin_name,
      std::string const &net_name
  );

  // print all nets
  void ReportNetList();

  // report the net map for debugging purposes
  void ReportNetMap();

  // initialize data structure for net fanout histogram
  void InitNetFanoutHistogram(std::vector<size_t> *histo_x = nullptr);

  // update HPWL values for the net fanout histogram
  void UpdateNetHPWLHistogram();

  // report the net fanout histogram
  void ReportNetFanoutHistogram();

  // report brief summary of this circuit
  void ReportBriefSummary();

  /************************************************
   * The following APIs are for in CELL
   * ************************************************/
  // set N-well layer parameters
  void SetNwellParams(
      double width,
      double spacing,
      double op_spacing,
      double max_plug_dist,
      double overhang
  );

  // set P-well layer parameters
  void SetPwellParams(
      double width,
      double spacing,
      double op_spacing,
      double max_plug_dist,
      double overhang
  );

  // set same_spacing (NN and PP) and any_spacing (NP)
  void SetLegalizerSpacing(double same_spacing, double any_spacing);

  // set the N/P-well shape of a given BlockType, unit in micron
  void SetWellRect(
      std::string const &blk_type_name,
      bool is_n,
      double lx,
      double ly,
      double ux,
      double uy
  );

  // create end cap cell type for gridded rows
  int CreateEndCapCellType(
      std::string const &end_cap_cell_type_name,
      int width,
      int n_well_height_in_grid_unit,
      int p_well_height_in_grid_unit
  );

  // report the well shape for each BlockType for debugging purposes
  void ReportWellShape();

  // read cell file for multiwell gridded cells
  void ReadMultiWellCell(std::string const &name_of_file);

  /**** Functions to get useful values ****/
  // returns the minimum width of blocks
  int MinBlkWidth() const;

  // returns the maximum width of blocks
  int MaxBlkWidth() const;

  // returns the minimum height of blocks
  int MinBlkHeight() const;

  // returns the maximum height of blocks
  int MaxBlkHeight() const;

  // returns the total block area
  unsigned long long TotBlkArea() const;

  // returns the total number of blocks
  int TotBlkCnt() const;

  // returns the total number of movable blocks
  int TotMovBlkCnt() const;

  // returns the total number of fixed blocks
  int TotFixedBlkCnt() const;

  // returns the average width of blocks
  double AveBlkWidth() const;

  // returns the average height of blocks
  double AveBlkHeight() const;

  // returns the average area of blocks
  double AveBlkArea() const;

  // returns the average width of movable blocks
  double AveMovBlkWidth() const;

  // returns the average height of movable blocks
  double AveMovBlkHeight() const;

  // returns the average area of movable blocks
  double AveMovBlkArea() const;

  // returns the white space usage ratio
  double WhiteSpaceUsage() const;

  /**** Utility member functions ****/
  // sort block pais in nets
  void NetSortBlkPin();

  // returns HPWL in the x direction, considering cell pin offsets, unit in micron
  double WeightedHPWLX();

  // returns HPWL in the y direction, considering cell pin offsets, unit in micron
  double WeightedHPWLY();

  // returns total HPWL, considering cell pin offsets, unit in micron
  double WeightedHPWL();

  // simple function to report HPWL
  void ReportHPWL();

  // returns bounding box in the x direction, unit in micron
  double WeightedBoundingBoxX();

  // returns bounding box in the y direction, unit in micron
  double WeightedBoundingBoxY();

  // returns total bounding box, unit in micron
  double WeightedBoundingBox();

  // simple function to report total bounding box
  void ReportBoundingBox();

  // report the histogram of HPWL using linear bins
  void ReportHPWLHistogramLinear(int bin_num = 10);

  // report the histogram of HPWL using logarithmic bins
  void ReportHPWLHistogramLogarithm(int bin_num = 10);

  // returns HPWL in the x direction, assuming cell pins are in the cell, unit in micron
  double HPWLCtoCX();

  // returns HPWL in the y direction, assuming cell pins are in the cell, unit in micron
  double HPWLCtoCY();

  // returns total HPWL, assuming cell pins are in the cell, unit in micron
  double HPWLCtoC();

  // simple function to report HPWL
  void ReportHPWLCtoC();

  // create a file to save distance to optimal region for each cell
  void SaveOptimalRegionDistance(std::string const &file_name =
  "optimal_region_distance.txt");

  /**** Save placement results to various file formats ****/
  // save placement result as a Matlab table
  void GenMATLABTable(
      std::string const &name_of_file = "block.txt",
      bool only_well_tap = false
  );

  // save placement with well fillings as a Matlab tale
  void GenMATLABWellTable(
      std::string const &name_of_file = "res",
      bool only_well_tap = false
  );

  // save long nets as a Matlab table
  void GenLongNetTable(std::string const &name_of_file);

  // save placement to a DEF file with many options
  void SaveDefFile(
      std::string const &base_name,
      std::string const &name_padding,
      std::string const &def_file_name,
      int save_floorplan,
      int save_cell,
      int save_iopin,
      int save_net
  );

  // save all components to a DEF file
  void SaveDefFileComponent(
      std::string const &name_of_file,
      std::string const &def_file_name
  );

  /**** Save results in Bookshelf formats ****/
  void SaveBookshelfNode(std::string const &name_of_file);

  void SaveBookshelfNet(std::string const &name_of_file);

  void SaveBookshelfPl(std::string const &name_of_file);

  void SaveBookshelfScl(std::string const &name_of_file);

  void SaveBookshelfWts(std::string const &name_of_file);

  void SaveBookshelfAux(std::string const &name_of_file);

  void LoadBookshelfPl(std::string const &name_of_file);

  /**** for standard cells ****/
  void CreateFakeWellForStandardCell();

 private:
  Tech tech_; // information in LEF and CELL
  Design design_; // information in DEF
  phydb::PhyDB *phy_db_ptr_ = nullptr;
  CircuitConstants constants_;
  bool enable_shrink_off_grid_die_area_ = false;

  void LoadImaginaryCellFile();

  void SetPhyDB(phydb::PhyDB *phy_db_ptr);

  // add a BlockType with name, with, and height. The return value is a pointer to this new BlockType for adding pins. Unit in grid value
  BlockType *AddBlockTypeWithGridUnit(
      std::string const &block_type_name,
      int width,
      int height
  );

  // add a BlockType with name, with, and height. The return value is a pointer to this new BlockType for adding pins. Unit in grid value
  int AddWellTapBlockTypeWithGridUnit(
      std::string const &block_type_name,
      int width,
      int height
  );

  BlockType *AddFillerBlockTypeWithGridUnit(
      std::string const &block_type_name,
      int width,
      int height
  );

  // set the boundary of the placement region, unit is in corresponding grid value
  void SetBoundary(int left, int bottom, int right, int top);

  void BlockTypeSizeMicrometerToGridValue(
      std::string const &block_type_name,
      double width,
      double height,
      int &gridded_width,
      int &gridded_height
  );

  // create a block instance using a pointer to its type
  void AddBlock(
      std::string const &block_name,
      BlockType *block_type_ptr,
      double llx = 0,
      double lly = 0,
      PlaceStatus place_status = UNPLACED,
      BlockOrient orient = N,
      bool is_real_cel = true
  );

  // create a dummy BlockType for I/O pins
  void AddDummyIOPinBlockType();

  // add an unplaced IOPin
  IoPin *AddUnplacedIoPin(std::string const &iopin_name);

  // add a placed IOPin
  IoPin *AddPlacedIOPin(std::string const &iopin_name, double lx, double ly);

  // shrink off grid die area
  RectI ShrinkOffGridDieArea(int lower_x, int lower_y, int upper_x, int upper_y);

  // shift off grid die area
  RectI ShiftOffGridDieArea(int lower_x, int lower_y, int upper_x, int upper_y);

  // load information in LEF
  void LoadTech(phydb::PhyDB *phy_db_ptr);

  // load information in DEF
  void ReserveSpaceForDesign();
  void LoadUnits();
  void LoadDieArea();
  void LoadComponents();
  void LoadIoPins();
  void LoadPlacementBlockages();
  void LoadNets();
  void LoadDesign();

  // load information in CELL
  void LoadCell(phydb::PhyDB *phy_db_ptr);

  // export cells/components to an output stream
  void SaveCell(std::ofstream &ost, Block &blk) const;
  void SaveNormalCells(
      std::ofstream &ost,
      std::unordered_set<PlaceStatus> *filter_out = nullptr
  );
  void SaveWellTapCells(std::ofstream &ost);
  void SaveCircuitWellCoverCell(
      std::ofstream &ost,
      std::string const &base_name
  ) const;
  void SaveCircuitPpnpCoverCell(
      std::ofstream &ost,
      std::string const &base_name
  ) const;
  void ExportNormalCells(std::ofstream &ost);
  void ExportWellTapCells(std::ofstream &ost);
  void ExportNormalAndWellTapCells(
      std::ofstream &ost,
      std::string const &base_name
  );
  void ExportNormalWellTapAndCoverCells(
      std::ofstream &ost,
      std::string const &base_name
  );
  void ExportCellsExcept(
      std::ofstream &ost,
      std::unordered_set<PlaceStatus> *filter_out = nullptr
  );
  void ExportCells(
      std::ofstream &ost,
      std::string const &base_name,
      int mode
  );
  void SaveIoPin(std::ofstream &ost, IoPin &iopin, bool after_io_place) const;
  void ExportIoPinsInfoAfterIoPlacement(std::ofstream &ost);
  void ExportIoPinsInfoBeforeIoPlacement(std::ofstream &ost);
  void ExportIoPins(std::ofstream &ost, int mode);
  void ExportAllNets(std::ofstream &ost);
  void ExportPowerNetsForWellTapCells(std::ofstream &ost);
  void ExportNets(std::ofstream &ost, int mode);
};

}

#endif //DALI_CIRCUIT_CIRCUIT_H_
