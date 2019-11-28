//
// Created by yihang on 11/27/19.
//

#ifndef HPCC_SRC_CIRCUIT_STATUS_H_
#define HPCC_SRC_CIRCUIT_STATUS_H_

#include <string>

enum BlockOrient{
  N = 0,
  S = 1,
  W = 2,
  E = 3,
  FN = 4,
  FS = 5,
  FW = 6,
  FE = 7
};

std::string OrientStr(BlockOrient orient);

enum PlaceStatus{
  COVER = 0,
  FIXED = 1,
  PLACED = 2,
  UNPLACED = 3,
  NULL_STATE = 4
};

std::string PlaceStatusStr(PlaceStatus place_status);

#endif //HPCC_SRC_CIRCUIT_STATUS_H_
