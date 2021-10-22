//
// Created by Yihang Yang on 3/23/2019.
//

#ifndef DALI_DALI_CIRCUIT_CIRCUIT_H_
#define DALI_DALI_CIRCUIT_CIRCUIT_H_

#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include <boost/functional/hash.hpp>
#include <phydb/phydb.h>

#include "blkpairnets.h"
#include "block.h"
#include "blocktype.h"
#include "dali/common/logging.h"
#include "design.h"
#include "iopin.h"
#include "layer.h"
#include "net.h"
#include "status.h"
#include "tech.h"

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
  public:
    Circuit();

    // lower triangle of the driver-load pair
    std::vector<BlkPairNets> blk_pair_net_list_;
    std::unordered_map<
        std::pair<int, int>,
        int,
        boost::hash<std::pair<int, int>>
    > blk_pair_map_;

    /**** API talking to PhyDB ****/
    void InitializeFromPhyDB(phydb::PhyDB *phy_db_ptr);

    void AddIoPinFromPhyDB(phydb::IOPin &iopin);

    int Micron2DatabaseUnit(double x);

    double DatabaseUnit2Micron(int x);

    int LocDali2PhydbX(double loc);

    int LocDali2PhydbY(double loc);

    double LocPhydb2DaliX(int loc);

    double LocPhydb2DaliY(int loc);

    /**** API to retrieve technology and design, this can be easily extended to support multiple techs and designs ****/
    Tech &tech();

    Design &design();

    /************************************************
     * The following APIs are for information in LEF
     * ************************************************/
    /****API to set and get database unit****/
    void SetDatabaseMicron(int database_micron);

    int DatabaseMicron() const;

    void SetManufacturingGrid(double manufacture_grid);

    double ManufacturingGrid() const;

    /****API to set grid value****/
    void SetGridValue(double grid_value_x, double grid_value_y);

    void SetGridFromMetalPitch();

    double GridValueX() const;

    double GridValueY() const;

    void SetRowHeight(double row_height);

    double RowHeightMicron() const;

    int RowHeightGrid() const;

    /****API to set metal layers: deprecated
     * now the metal layer information are all stored in openDB data structure
     * ****/
    std::vector<MetalLayer> &Metals();

    std::unordered_map<std::string, int> &MetalNameMap();

    bool IsMetalLayerExisting(std::string const &metal_name);

    int GetMetalLayerId(std::string const &metal_name);

    MetalLayer *GetMetalLayerPtr(std::string const &metal_name);

    void AddMetalLayer(
        std::string const &metal_name,
        double width,
        double spacing,
        double min_area,
        double pitch_x,
        double pitch_y,
        MetalDirection metal_direction
    );

    void ReportMetalLayers();

    /****API for BlockType****/
    std::unordered_map<std::string, BlockType *> &BlockTypeMap();

    bool IsBlockTypeExisting(std::string const &block_type_name);

    BlockType *GetBlockTypePtr(std::string const &block_type_name);

    void BlockTypeSizeMicrometerToGridValue(
        std::string const &block_type_name,
        double width,
        double height,
        int &gridded_width,
        int &gridded_height
    );

    BlockType *AddBlockType(
        std::string const &block_type_name,
        double width,
        double height
    );

    void SetBlockTypeSize(BlockType *blk_type_ptr, double width, double height);

    BlockType *AddWellTapBlockType(
        std::string const &block_type_name,
        double width,
        double height
    );

    Pin *AddBlkTypePin(
        std::string const &block_type_name,
        std::string const &pin_name,
        bool is_input
    );

    Pin *AddBlkTypePin(
        BlockType *blk_type_ptr,
        std::string const &pin_name,
        bool is_input
    );

    void AddBlkTypePinRect(
        std::string const &block_type_name,
        std::string const &pin_name,
        double llx,
        double lly,
        double urx,
        double ury
    );

    void AddBlkTypePinRect(
        Pin *pin_ptr,
        double llx,
        double lly,
        double urx,
        double ury
    );

    void ReportBlockType();

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
    void SetUnitsDistanceMicrons(int distance_microns);

    int DistanceMicrons() const;

    void SetDieArea(int lower_x, int lower_y, int upper_x, int upper_y);

    int RegionLLX() const;

    int RegionURX() const;

    int RegionLLY() const;

    int RegionURY() const;

    int RegionWidth() const;

    int RegionHeight() const;

    void SetListCapacity(int components_count, int pins_count, int nets_count);

    /****API for Block
     * These are COMPONENTS section in DEF
     * ****/
    std::vector<Block> &Blocks();

    bool IsBlockExisting(std::string const &block_name);

    int GetBlockId(std::string const &block_name);

    Block *GetBlockPtr(std::string const &block_name);

    void AddBlock(
        std::string const &block_name,
        std::string const &block_type_name,
        double llx = 0,
        double lly = 0,
        PlaceStatus place_status = UNPLACED,
        BlockOrient orient = N,
        bool is_real_cel = true
    );

    void ReportBlockList();

    void ReportBlockMap();

    /****API for IOPIN
     * These are PINS section in DEF
     * ****/
    std::vector<IoPin> &IoPins();

    bool IsIoPinExisting(std::string const &iopin_name);

    int GetIoPinId(std::string const &iopin_name);

    IoPin *GetIoPinPtr(std::string const &iopin_name);

    IoPin *AddIoPin(
        std::string const &iopin_name,
        PlaceStatus place_status,
        SignalUse signal_use,
        SignalDirection signal_direction,
        double lx = 0,
        double ly = 0
    );

    void ReportIOPin();

    /****API for Nets
     * These are NETS section in DEF
     * ****/
    std::vector<Net> *getNetList();

    std::vector<Net> &NetListRef();

    bool IsNetExist(std::string &net_name);

    int NetIndex(std::string &net_name);

    Net *getNetPtr(std::string &net_name);

    Net *AddNet(std::string &net_name, int capacity, double weight = 1);

    void AddIOPinToNet(std::string &iopin_name, std::string &net_name);

    void AddBlkPinToNet(
        std::string &blk_name,
        std::string &pin_name,
        std::string &net_name
    );

    void ReportNetList();

    void ReportNetMap();

    void InitNetFanoutHistogram(std::vector<int> *histo_x = nullptr);

    void UpdateNetHPWLHistogram();

    void ReportNetFanoutHistogram();

    /************************************************
     * End of API for DEF
     * ************************************************/
    void ReportBriefSummary() const;

    /************************************************
     * The following APIs are for in CELL
     * ************************************************/
    void SetNWellParams(
        double width,
        double spacing,
        double op_spacing,
        double max_plug_dist,
        double overhang
    );

    void SetPWellParams(
        double width,
        double spacing,
        double op_spacing,
        double max_plug_dist,
        double overhang
    );

    void SetLegalizerSpacing(double same_spacing, double any_spacing);

    BlockTypeWell *AddBlockTypeWell(std::string &blk_type_name);

    void setWellRect(
        std::string &blk_type_name,
        bool is_N,
        double lx,
        double ly,
        double ux,
        double uy
    );

    void ReportWellShape();

    /****member functions to obtain some useful values****/
    int MinBlkWidth() const;

    int MaxBlkWidth() const;

    int MinBlkHeight() const;

    int MaxBlkHeight() const;

    long int TotBlkArea() const;

    int TotBlkCount() const;

    int TotMovableBlockCount() const;

    int TotFixedBlkCnt() const;

    double AveBlkWidth() const;

    double AveBlkHeight() const;

    double AveBlkArea() const;

    double AveMovBlkWidth() const;

    double AveMovBlkHeight() const;

    double AveMovBlkArea() const;

    double WhiteSpaceUsage() const;

    /****Utility member functions****/
    void BuildBlkPairNets();

    void NetSortBlkPin();

    double WeightedHPWLX();

    double WeightedHPWLY();

    double WeightedHPWL();

    void ReportHPWL();

    void ReportHPWLHistogramLinear(int bin_num = 10);

    void ReportHPWLHistogramLogarithm(int bin_num = 10);

    void SaveOptimalRegionDistance(
        std::string file_name = "optimal_region_distance.txt");

    double HPWLCtoCX();

    double HPWLCtoCY();

    double HPWLCtoC();

    void ReportHPWLCtoC();

    /****save placement results to various file formats****/
    void GenMATLABTable(
        std::string const &name_of_file = "block.txt",
        bool only_well_tap = false
    );

    void GenMATLABWellTable(
        std::string const &name_of_file = "res",
        bool only_well_tap = false
    );

    void GenLongNetTable(std::string const &name_of_file);

    void SaveDefFile(
        std::string const &name_of_file,
        std::string const &def_file_name,
        bool is_complete_version = true
    );

    void SaveDefFile(
        std::string const &base_name,
        std::string const &name_padding,
        std::string const &def_file_name,
        int save_floorplan,
        int save_cell,
        int save_iopin,
        int save_net
    );

    void SaveIODefFile(
        std::string const &name_of_file,
        std::string const &def_file_name
    );

    void SaveDefWell(
        std::string const &name_of_file,
        std::string const &def_file_name,
        bool is_no_normal_cell = true
    );

    void SaveDefPPNPWell(
        std::string const &name_of_file,
        std::string const &def_file_name
    );

    void SaveInstanceDefFile(
        std::string const &name_of_file,
        std::string const &def_file_name
    );

    /****some Bookshelf IO APIs****/
    void SaveBookshelfNode(std::string const &name_of_file);

    void SaveBookshelfNet(std::string const &name_of_file);

    void SaveBookshelfPl(std::string const &name_of_file);

    void SaveBookshelfScl(std::string const &name_of_file);

    void SaveBookshelfWts(std::string const &name_of_file);

    void SaveBookshelfAux(std::string const &name_of_file);

    void LoadBookshelfPl(std::string const &name_of_file);

  private:
    Tech tech_; // information in LEF and CELL
    Design design_; // information in DEF
    phydb::PhyDB *phy_db_ptr_;

    void LoadImaginaryCellFile();

    static double Residual(double x, double y);

    BlockType *AddBlockTypeWithGridUnit(
        std::string const &block_type_name,
        int width,
        int height
    );

    BlockType *AddWellTapBlockTypeWithGridUnit(
        std::string const &block_type_name,
        int width,
        int height
    );

    MetalLayer *AddMetalLayer(
        std::string const &metal_name,
        double width,
        double spacing
    );

    MetalLayer *AddMetalLayer(std::string &metal_name);

    void SetBoundary(int left, int bottom, int right, int top);

    void AddBlock(
        std::string const &block_name,
        BlockType *block_type_ptr,
        double llx = 0,
        double lly = 0,
        PlaceStatus place_status = UNPLACED,
        BlockOrient orient = N,
        bool is_real_cel = true
    );

    void AddDummyIOPinBlockType();

    IoPin *AddUnplacedIOPin(std::string const &iopin_name);

    IoPin *AddPlacedIOPin(std::string const &iopin_name, double lx, double ly);

    BlockTypeWell *AddBlockTypeWell(BlockType *blk_type);

    void LoadTech(phydb::PhyDB *phy_db_ptr); // LEF

    void LoadDesign(phydb::PhyDB *phy_db_ptr); // DEF

    void LoadWell(phydb::PhyDB *phy_db_ptr); // CELL
};

}

#endif //DALI_DALI_CIRCUIT_CIRCUIT_H_
