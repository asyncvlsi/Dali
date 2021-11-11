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
#ifndef DALI_DALI_CIRCUIT_STATUS_H_
#define DALI_DALI_CIRCUIT_STATUS_H_

#include <string>

namespace dali {

enum MetalDirection {
  HORIZONTAL = 0,
  VERTICAL = 1,
  DIAG45 = 2,
  DIAG135 = 3
};

MetalDirection StrToMetalDirection(std::string const &str_metal_direction);
std::string MetalDirectionStr(MetalDirection metal_direction);

enum BlockOrient {
  N = 0,
  S = 1,
  W = 2,
  E = 3,
  FN = 4,
  FS = 5,
  FW = 6,
  FE = 7
};

BlockOrient StrToOrient(std::string const &str_orient);
std::string OrientStr(BlockOrient orient);

enum PlaceStatus {
  COVER = 0,
  FIXED = 1,
  PLACED = 2,
  UNPLACED = 3,
};

PlaceStatus StrToPlaceStatus(std::string const &str_place_status);
std::string PlaceStatusStr(PlaceStatus place_status);

enum SignalDirection {
  INPUT = 0,
  OUTPUT = 1,
  INOUT = 2,
  FEEDTHRU = 3
};

SignalDirection StrToSignalDirection(std::string const &str_signal_direction);
std::string SignalDirectionStr(SignalDirection signal_direction);

enum SignalUse {
  SIGNAL = 0,
  POWER = 1,
  GROUND = 2,
  CLOCK = 3,
  TIEOFF = 4,
  ANALOG = 5,
  SCAN = 6,
  RESET = 7
};

SignalUse StrToSignalUse(std::string const &str_signal_use);
std::string SignalUseStr(SignalUse signal_use);

}

#endif //DALI_DALI_CIRCUIT_STATUS_H_
