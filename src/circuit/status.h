//
// Created by Yihang Yang on 11/27/19.
//

#ifndef DALI_SRC_CIRCUIT_STATUS_H_
#define DALI_SRC_CIRCUIT_STATUS_H_

#include <string>

enum MetalDirection {
  HORIZONTAL_ = 0,
  VERTICAL_ = 1,
  DIAG45_ = 2,
  DIAG135_ = 3
};

MetalDirection StrToMetalDirection(std::string &str_metal_direction);
std::string MetalDirectionStr(MetalDirection metal_direction);

enum BlockOrient {
  N_ = 0,
  S_ = 1,
  W_ = 2,
  E_ = 3,
  FN_ = 4,
  FS_ = 5,
  FW_ = 6,
  FE_ = 7
};

BlockOrient StrToOrient(std::string &str_orient);
BlockOrient StrToOrient(const char *str_orient);
std::string OrientStr(BlockOrient orient);

enum PlaceStatus {
  COVER_ = 0,
  FIXED_ = 1,
  PLACED_ = 2,
  UNPLACED_ = 3,
  NULL_STATE_ = 4
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

enum SignalUse {
  SIGNAL_ = 0,
  POWER_ = 1,
  GROUND_ = 2,
  CLOCK_ = 3,
  TIEOFF_ = 4,
  ANALOG_ = 5,
  SCAN_ = 6,
  RESET_ = 7
};

SignalUse StrToSignalUse(std::string &str_signal_use);
std::string SignalUseStr(SignalUse signal_use);

#endif //DALI_SRC_CIRCUIT_STATUS_H_
