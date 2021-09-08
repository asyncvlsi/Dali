//
// Created by Yihang Yang on 9/8/21.
//

#include "phydbhelper.h"

#include "logging.h"

namespace dali {

phydb::PlaceStatus PlaceStatusDali2PhyDB(PlaceStatus dali_place_status) {
    switch(dali_place_status) {
        case COVER:
            return phydb::COVER;
        case FIXED:
            return phydb::FIXED;
        case PLACED:
            return phydb::PLACED;
        case UNPLACED:
            return phydb::UNPLACED;
        default:
            DaliExpects(false, "Unknown Dali placement status for cells");
    }
}

}

