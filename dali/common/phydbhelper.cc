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
    case COVER:return phydb::PlaceStatus::COVER;
    case FIXED:return phydb::PlaceStatus::FIXED;
    case PLACED:return phydb::PlaceStatus::PLACED;
    case UNPLACED:return phydb::PlaceStatus::UNPLACED;
    default:DaliExpects(false, "Unknown Dali placement status for cells");
      return phydb::PlaceStatus::UNPLACED; // this will never be reached
  }
}

PlaceStatus PlaceStatusPhyDB2Dali(phydb::PlaceStatus phydb_place_status) {
  switch (phydb_place_status) {
    case phydb::PlaceStatus::COVER:return COVER;
    case phydb::PlaceStatus::FIXED:return FIXED;
    case phydb::PlaceStatus::PLACED:return PLACED;
    case phydb::PlaceStatus::UNPLACED:return UNPLACED;
    default:DaliExpects(false, "Unknown PhyDB placement status for cells");
      return UNPLACED; // this will never be reached
  }
}

phydb::CompOrient OrientDali2PhyDB(BlockOrient dali_orient) {
  switch (dali_orient) {
    case N:return phydb::CompOrient::N;
    case S:return phydb::CompOrient::S;
    case W:return phydb::CompOrient::W;
    case E:return phydb::CompOrient::E;
    case FN:return phydb::CompOrient::FN;
    case FS:return phydb::CompOrient::FS;
    case FW:return phydb::CompOrient::FW;
    case FE:return phydb::CompOrient::FE;
    default:DaliExpects(false, "Unknown Dali cell orientation");
      return phydb::CompOrient::N; // this will never be reached
  }
}

BlockOrient OrientPhyDB2Dali(phydb::CompOrient phydb_orient) {
  switch (phydb_orient) {
    case phydb::CompOrient::N:return N;
    case phydb::CompOrient::S:return S;
    case phydb::CompOrient::W:return W;
    case phydb::CompOrient::E:return E;
    case phydb::CompOrient::FN:return FN;
    case phydb::CompOrient::FS:return FS;
    case phydb::CompOrient::FW:return FW;
    case phydb::CompOrient::FE:return FE;
    default:DaliExpects(false, "Unknown PhyDB cell orientation");
      return N; // this will never be reached
  }
}

}

