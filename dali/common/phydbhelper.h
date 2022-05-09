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
#ifndef DALI_COMMON_PHYDBHELPER_H_
#define DALI_COMMON_PHYDBHELPER_H_

#include <phydb/enumtypes.h>

#include "dali/circuit/status.h"

namespace dali {

phydb::PlaceStatus PlaceStatusDali2PhyDB(PlaceStatus dali_place_status);
PlaceStatus PlaceStatusPhyDB2Dali(phydb::PlaceStatus phydb_place_status);
phydb::CompOrient OrientDali2PhyDB(BlockOrient dali_orient);
BlockOrient OrientPhyDB2Dali(phydb::CompOrient phydb_orient);

}

#endif //DALI_COMMON_PHYDBHELPER_H_
