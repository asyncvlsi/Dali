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
#ifndef DALI_CIRCUIT_STATUS_H_
#define DALI_CIRCUIT_STATUS_H_

#include <string>

namespace dali {

/** DEF/LEF metal routing direction. */
enum MetalDirection { HORIZONTAL = 0, VERTICAL = 1, DIAG45 = 2, DIAG135 = 3 };

/** Convert DEF/LEF metal direction string to enum. */
MetalDirection StrToMetalDirection(std::string const& str_metal_direction);

/** Convert metal direction enum to DEF/LEF string. */
std::string MetalDirectionStr(MetalDirection metal_direction);

/** DEF component orientation. */
enum BlockOrient { N = 0, S = 1, W = 2, E = 3, FN = 4, FS = 5, FW = 6, FE = 7 };

/** Convert DEF orientation string to enum. */
BlockOrient StrToOrient(std::string const& str_orient);

/** Convert orientation enum to DEF string. */
std::string OrientStr(BlockOrient orient);

/** DEF placement status. */
enum PlaceStatus {
  COVER = 0,
  FIXED = 1,
  PLACED = 2,
  UNPLACED = 3,
};

/** Convert DEF placement-status string to enum. */
PlaceStatus StrToPlaceStatus(std::string const& str_place_status);

/** Convert placement-status enum to DEF string. */
std::string PlaceStatusStr(PlaceStatus place_status);

/** DEF signal direction. */
enum SignalDirection { INPUT = 0, OUTPUT = 1, INOUT = 2, FEEDTHRU = 3 };

/** Convert DEF signal-direction string to enum. */
SignalDirection StrToSignalDirection(std::string const& str_signal_direction);

/** Convert signal-direction enum to DEF string. */
std::string SignalDirectionStr(SignalDirection signal_direction);

/** DEF signal use. */
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

/** Convert DEF signal-use string to enum. */
SignalUse StrToSignalUse(std::string const& str_signal_use);

/** Convert signal-use enum to DEF string. */
std::string SignalUseStr(SignalUse signal_use);

}  // namespace dali

#endif  // DALI_CIRCUIT_STATUS_H_
