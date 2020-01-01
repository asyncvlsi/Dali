//
// Created by yihang on 11/27/19.
//

#ifndef DALI_SRC_CIRCUIT_STATUS_H_
#define DALI_SRC_CIRCUIT_STATUS_H_

#include <string>

enum MetalDirection {
  HORIZONTAL = 0,
  VERTICAL = 1,
  DIAG45 = 2,
  DIAG135 = 3
};

MetalDirection StrToMetalDirection(std::string &str_metal_direction);
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

BlockOrient StrToOrient(std::string &str_orient);
BlockOrient StrToOrient(const char *str_orient);
std::string OrientStr(BlockOrient orient);

enum PlaceStatus {
  COVER = 0,
  FIXED = 1,
  PLACED = 2,
  UNPLACED = 3,
  NULL_STATE = 4
};

PlaceStatus StrToPlaceStatus(std::string &str_place_status);
std::string PlaceStatusStr(PlaceStatus place_status);

enum SignalDirection {
  INPUT_ = 0,
  OUTPUT_ = 1,
  INOUT_ = 2,
  FEEDTHRU_ = 3
};

SignalDirection StrToSignalDirection(std::string &str_signal_direction);
std::string SignalDirectionStr(SignalDirection signal_direction);

#endif //DALI_SRC_CIRCUIT_STATUS_H_
