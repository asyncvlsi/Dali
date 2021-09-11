//
// Created by YihangYang on 9/10/2021.
//

#include "helper.h"

namespace dali {

void SetAllIoPinsToUnplaced(phydb::PhyDB &phydb) {
    for (auto &iopin: phydb.GetDesignPtr()->GetIoPinsRef()) {
        iopin.SetPlacementStatus(phydb::UNPLACED);
        iopin.Report();
    }
}

bool IsAllIoPinsOnBoundary(phydb::PhyDB &phydb) {

    return true;
}

}