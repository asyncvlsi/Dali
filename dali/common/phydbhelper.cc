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
#include "phydbhelper.h"

#include "logging.h"

namespace dali {

phydb::PlaceStatus PlaceStatusDali2PhyDB(PlaceStatus dali_place_status) {
  switch (dali_place_status) {
    case COVER:return phydb::COVER;
    case FIXED:return phydb::FIXED;
    case PLACED:return phydb::PLACED;
    case UNPLACED:return phydb::UNPLACED;
    default:DaliExpects(false, "Unknown Dali placement status for cells");
      return phydb::UNPLACED; // this will never be reached
  }
}

PlaceStatus PlaceStatusPhyDB2Dali(phydb::PlaceStatus phydb_place_status) {
  switch (phydb_place_status) {
    case phydb::COVER:return COVER;
    case phydb::FIXED:return FIXED;
    case phydb::PLACED:return PLACED;
    case phydb::UNPLACED:return UNPLACED;
    default:DaliExpects(false, "Unknown PhyDB placement status for cells");
      return UNPLACED; // this will never be reached
  }
}

phydb::CompOrient OrientDali2PhyDB(BlockOrient dali_orient) {
  switch (dali_orient) {
    case N:return phydb::N;
    case S:return phydb::S;
    case W:return phydb::W;
    case E:return phydb::E;
    case FN:return phydb::FN;
    case FS:return phydb::FS;
    case FW:return phydb::FW;
    case FE:return phydb::FE;
    default:DaliExpects(false, "Unknown Dali cell orientation");
      return phydb::N; // this will never be reached
  }
}

BlockOrient OrientPhyDB2Dali(phydb::CompOrient phydb_orient) {
  switch (phydb_orient) {
    case phydb::N:return N;
    case phydb::S:return S;
    case phydb::W:return W;
    case phydb::E:return E;
    case phydb::FN:return FN;
    case phydb::FS:return FS;
    case phydb::FW:return FW;
    case phydb::FE:return FE;
    default:DaliExpects(false, "Unknown PhyDB cell orientation");
      return N; // this will never be reached
  }
}

}

