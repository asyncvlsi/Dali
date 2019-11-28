//
// Created by yihang on 11/27/19.
//

#include "status.h"
#include <iostream>

std::string OrientStr(BlockOrient orient) {
  std::string s;
  switch (orient) {
    case 0: { s = "N"; } break;
    case 1: { s = "S"; } break;
    case 2: { s = "W"; } break;
    case 3: { s = "E"; } break;
    case 4: { s = "FN"; } break;
    case 5: { s = "FS"; } break;
    case 6: { s = "FW"; } break;
    case 7: { s = "FE"; } break;
    default: {
      std::cout << "Block orientation error!" << std::endl;
      exit(1);
    }
  }
  return s;
}

std::string PlaceStatusStr(PlaceStatus place_status) {
  std::string s;
  switch (place_status) {
    case 0: { s = "COVER"; } break;
    case 1: { s = "FIXED"; } break;
    case 2: { s = "PLACED"; } break;
    case 3: { s = "UNPLACED"; } break;
    case 4: { s = "NULL_STATE"; } break;
    default: {
      std::cout << "Unit placement state error!" << std::endl;
      exit(1);
    }
  }
  return s;
}