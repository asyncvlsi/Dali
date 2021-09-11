//
// Created by YihangYang on 9/10/2021.
//

#ifndef DALI_TESTS_IOPLACER_HELPER_H
#define DALI_TESTS_IOPLACER_HELPER_H

#include "phydb/phydb.h"

namespace dali {

void SetAllIoPinsToUnplaced(phydb::PhyDB &phydb);
bool IsAllIoPinsOnBoundary(phydb::PhyDB &phydb);

}

#endif //DALI_TESTS_IOPLACER_HELPER_H
