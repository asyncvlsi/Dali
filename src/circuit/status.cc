//
// Created by yihang on 11/27/19.
//

#include "status.h"
#include <iostream>

BlockOrient StrToOrient(std::string &str_orient) {
  BlockOrient orient = N;
  if (str_orient == "N") {
    orient = N;
  } else if (str_orient == "S") {
    orient = S;
  } else if (str_orient == "W") {
    orient = W;
  } else if (str_orient == "E") {
    orient = E;
  } else if (str_orient == "FN") {
    orient = FN;
  } else if (str_orient == "FS") {
    orient = FS;
  } else if (str_orient == "FW") {
    orient = FW;
  } else if (str_orient == "FE") {
    orient = FE;
  } else {
    std::cout << "Block orientation error!" << std::endl;
    exit(1);
  }
  return orient;
}

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

PlaceStatus StrToPlaceStatus(std::string &str_place_status) {
  PlaceStatus place_status = UNPLACED;
  if (str_place_status == "COVER") {
    place_status = COVER;
  } else if (str_place_status == "FIXED") {
    place_status = FIXED;
  } else if (str_place_status == "PLACED") {
    place_status = PLACED;
  } else if (str_place_status == "UNPLACED") {
    place_status = UNPLACED;
  } else if (str_place_status == "NULL_STATE") {
    place_status = NULL_STATE;
  } else {
    std::cout << "Unit placement status error!" << std::endl;
    exit(1);
  }
  return place_status;
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