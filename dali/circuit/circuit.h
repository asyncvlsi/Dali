//
// Created by Yihang Yang on 3/23/2019.
//

#ifndef DALI_CIRCUIT_HPP
#define DALI_CIRCUIT_HPP

#include <map>
#include <set>
#include <unordered_map>
#include <vector>

#include <boost/functional/hash.hpp>
#include <phydb/phydb.h>

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
    phydb::PhyDB *phy_db_ptr_;

    void LoadImaginaryCellFile();

    static double Residual(double x, double y);

    BlockType *AddBlockTypeWithGridUnit(
        std::string &block_type_name,
        int width,
        int height
    );

    BlockType *AddWellTapBlockTypeWithGridUnit(
        std::string &block_type_name,
        int width,
        int height
    );

    MetalLayer *AddMetalLayer(
        std::string &metal_name,
        double width,
        double spacing
    );

    MetalLayer *AddMetalLayer(std::string &metal_name);

    void SetBoundary(int left, int bottom, int right, int top);

    void AddBlock(
        std::string &block_name,
        BlockType *block_type_ptr,
        double llx = 0,
        double lly = 0,
        PlaceStatus place_status = UNPLACED,
        BlockOrient orient = N,
        bool is_real_cel = true
    );

    void AddDummyIOPinBlockType();

    IOPin *AddUnplacedIOPin(std::string &iopin_name);

    IOPin *AddPlacedIOPin(std::string &iopin_name, double lx, double ly);

    BlockTypeWell *AddBlockTypeWell(BlockType *blk_type);

public:

    Circuit();

    // lower triangle of the driver-load pair
    std::vector<BlkPairNets> blk_pair_net_list_;
    std::unordered_map<
        std::pair<int, int>,
        int,
        boost::hash<std::pair<int, int>>
    > blk_pair_map_;

    /****API to initialize circuit
     * 2. from LEF/DEF directly using a naive parser
     * ****/
    void LoadTech(phydb::PhyDB *phy_db_ptr); // LEF

    int Micron2DatabaseUnit(double x);

    double DatabaseUnit2Micron(int x);

    int DaliLoc2PhyDBLocX(double loc);

    int DaliLoc2PhyDBLocY(double loc);

    double PhyDBLoc2DaliLocX(int loc);

    double PhyDBLoc2DaliLocY(int loc);

    void AddIoPinFromPhyDB(phydb::IOPin &iopin);

    void LoadDesign(phydb::PhyDB *phy_db_ptr); // DEF

    void LoadWell(phydb::PhyDB *phy_db_ptr); // CELL

    void InitializeFromPhyDB(phydb::PhyDB *phy_db_ptr);

    // LEF/DEF loader
    void ReadLefFile(std::string const &name_of_file);

    void ReadDefFile(std::string const &name_of_file);

    // simple CELL parser
    void ReadCellFile(std::string const &name_of_file);

    /****API to retrieve technology and design****/
    Tech *getTech();

    Tech &getTechRef();

    Design *getDesign();

    Design &getDesignRef();

    /************************************************
     * The following APIs are for in LEF
     * ************************************************/
    /****API to set and get database unit****/
    void setDatabaseMicron(int database_micron);

    int DatabaseMicron() const;

    void setManufacturingGrid(double manufacture_grid);

    double ManufacturingGrid() const;

    /****API to set grid value****/
    void SetGridValue(double grid_value_x, double grid_value_y);

    void SetGridUsingMetalPitch();

    double GridValueX() const;

    double GridValueY() const;

    void setRowHeight(double row_height);

    double getDBRowHeight() const;

    int getINTRowHeight() const;

    /****API to set metal layers: deprecated
     * now the metal layer information are all stored in openDB data structure
     * ****/
    std::unordered_map<std::string, int> *MetalNameMap();

    bool IsMetalLayerExist(std::string &metal_name);

    int MetalLayerIndex(std::string &metal_name);

    MetalLayer *getMetalLayerPtr(std::string &metal_name);

    void AddMetalLayer(
        std::string &metal_name,
        double width,
        double spacing,
        double min_area,
        double pitch_x,
        double pitch_y,
        MetalDirection metal_direction
    );

    void ReportMetalLayers();

    /****API for BlockType****/
    std::unordered_map<std::string, BlockType *> *BlockTypeMap();

    bool IsBlockTypeExist(std::string &block_type_name);

    BlockType *getBlockType(std::string &block_type_name);

    void BlockTypeSizeMicrometerToGridValue(
        const std::string &block_type_name,
        double width,
        double height,
        int &gridded_width,
        int &gridded_height
    );

    BlockType *AddBlockType(
        std::string &block_type_name,
        double width,
        double height
    );

    void SetBlockTypeSize(BlockType *blk_type_ptr, double width, double height);

    BlockType *AddWellTapBlockType(
        std::string &block_type_name,
        double width,
        double height
    );

    Pin *AddBlkTypePin(
        std::string &block_type_name,
        std::string &pin_name,
        bool is_input
    );

    Pin *AddBlkTypePin(
        BlockType *blk_type_ptr,
        std::string &pin_name,
        bool is_input
    );

    void AddBlkTypePinRect(
        std::string &block_type_name,
        std::string &pin_name,
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
    void setUnitsDistanceMicrons(int distance_microns);

    int DistanceMicrons() const;

    void setDieArea(int lower_x, int lower_y, int upper_x, int upper_y);

    int RegionLLX() const;

    int RegionURX() const;

    int RegionLLY() const;

    int RegionURY() const;

    int RegionWidth() const;

    int RegionHeight() const;

    void setListCapacity(int components_count, int pins_count, int nets_count);

    /****API for Block
     * These are COMPONENTS section in DEF
     * ****/
    std::vector<Block> *getBlockList();

    std::vector<Block> &BlockListRef();

    bool IsBlockExist(std::string &block_name);

    int BlockIndex(std::string &block_name);

    Block *getBlockPtr(std::string &block_name);

    void AddBlock(
        std::string &block_name,
        std::string &block_type_name,
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
    std::vector<IOPin> *getIOPinList();

    bool IsIOPinExist(std::string &iopin_name);

    int IOPinIndex(std::string &iopin_name);

    IOPin *getIOPin(std::string &iopin_name);

    IOPin *AddIOPin(
        std::string &iopin_name,
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
};

}

#endif //DALI_CIRCUIT_HPP
