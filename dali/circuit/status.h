//
// Created by Yihang Yang on 11/27/19.
//

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

#define NUM_OF_ORIENT 8
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
