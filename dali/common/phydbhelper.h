//
// Created by Yihang Yang on 9/8/21.
//

#ifndef DALI_DALI_COMMON_PHYDBHELPER_H_
#define DALI_DALI_COMMON_PHYDBHELPER_H_

#include <phydb/enumtypes.h>

#include "dali/circuit/status.h"

namespace dali {
    phydb::PlaceStatus PlaceStatusDali2PhyDB(PlaceStatus dali_place_status);
}

#endif //DALI_DALI_COMMON_PHYDBHELPER_H_
