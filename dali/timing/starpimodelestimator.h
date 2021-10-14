//
// Created by Yihang Yang on 10/13/21.
//

#ifndef DALI_DALI_TIMING_STARPIMODELESTIMATOR_H_
#define DALI_DALI_TIMING_STARPIMODELESTIMATOR_H_

#include <phydb/rcestimator.h>

#include "dali/circuit/circuit.h"
#include "dali/common/misc.h"

namespace dali {

class StarPiModelEstimator : protected phydb::RCEstimator {
  public:
    StarPiModelEstimator(phydb::PhyDB *phydb_ptr, Circuit *circuit) :
        RCEstimator(phydb_ptr), circuit_(circuit) {}
    ~StarPiModelEstimator() override = default;
    void PushNetRCToManager() override;
  private:
    Circuit *circuit_;
    int distance_micron_ = 0;
    bool edge_pushed_to_spef_manager_ = false;
    phydb::Layer *horizontal_layer_ = nullptr;
    phydb::Layer *vertical_layer_ = nullptr;
    void AddEdgesToManager();
    void FindFirstHorizontalAndVerticalMetalLayer();
    void GetResistanceAndCapacitance(
        double2d &driver_loc,
        double2d &load_loc,
        double &resistance,
        double &capacitance
    );
};

}

#endif //DALI_DALI_TIMING_STARPIMODELESTIMATOR_H_
