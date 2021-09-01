//
// Created by Yihang Yang on 8/25/21.
//

#ifndef DALI_DALI_PLACER_IOPLACER_IOPLACER_H_
#define DALI_DALI_PLACER_IOPLACER_IOPLACER_H_

#include <vector>

#include <phydb/phydb.h>

#include "dali/circuit.h"
#include "dali/placer.h"

#include "ioboundaryspace.h"

namespace dali {

/**
 * A class for placing IOPINs.
 * This class mainly provides two functions:
 * 1. manually place IOPINs before placement using given locations
 * 2. automatically place IOPINs after placement
 */
class IoPlacer {
    Circuit *circuit_ = nullptr;
    phydb::PhyDB *phy_db_ptr_ = nullptr;
    std::vector<IoBoundarySpace> boundary_spaces_;
    double left_;
    double right_;
    double bottom_;
    double top_;
  public:
    IoPlacer() = default;
    explicit IoPlacer(phydb::PhyDB *phy_db, Circuit *circuit_);

    void SetCiruit(Circuit *circuit);
    void SetPhyDB(phydb::PhyDB *phy_db_ptr);

    bool AddIoPin(std::string &iopin_name, std::string &net_name,
                  std::string &direction, std::string &use);
    bool AddCmd(int argc, char **argv);
    bool PlaceIoPin(std::string &iopin_name,
                    std::string &metal_name,
                    int shape_lx,
                    int shape_ly,
                    int shape_ux,
                    int shape_uy,
                    std::string &place_status,
                    int loc_x,
                    int loc_y,
                    std::string &orient);
    bool PlaceCmd(int argc, char **argv);

    bool PartialPlaceIoPin();
    bool PartialPlaceCmd(int argc, char **argv);

    bool ConfigSetGlobalMetalLayer(int metal_layer_index);
    bool ConfigAutoPlace();
    bool ConfigCmd(int argc, char **argv);

    bool BuildResourceMap();
    bool AssignIoPinToBoundaryLayers();
    bool PlaceIoPinOnEachBoundary();

    bool AutoPlaceIoPin();
    bool AutoPlaceCmd(int argc, char **argv);
};

}

#endif //DALI_DALI_PLACER_IOPLACER_IOPLACER_H_
