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
#ifndef DALI_TESTS_IOPLACER_HELPER_H
#define DALI_TESTS_IOPLACER_HELPER_H
#include <string>

#include "phydb/phydb.h"

namespace dali {

void SetAllIoPinsToUnplaced(phydb::PhyDB *p_phydb);
bool IsEveryIoPinPlacedOnBoundary(phydb::PhyDB *p_phydb);
bool IsNoIoPinOverlapAndSpacingViolation(phydb::PhyDB *p_phydb);
bool IsEveryIoPinOnMetal(
    phydb::PhyDB *p_phydb,
    std::string const &layer_name
);
bool IsEveryIoPinManufacturable(phydb::PhyDB *p_phydb);
bool IsEveryIoPinInsideDieArea(phydb::PhyDB *p_phydb);

void RemoveAllIoPins(phydb::PhyDB *p_phydb);
bool IsEveryIoPinAddedAndPlacedCorrectly(
    phydb::PhyDB *p_phydb0,
    phydb::PhyDB *p_phydb1
);

}

#endif //DALI_TESTS_IOPLACER_HELPER_H
