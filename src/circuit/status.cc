//
// Created by yihang on 11/27/19.
//

#include "status.h"
#include <iostream>

MetalDirection StrToMetalDirection(std::string &str_metal_direction)  {
  MetalDirection metal_direction = HORIZONTAL;
  if (str_metal_direction == "HORIZONTAL") {
    metal_direction = HORIZONTAL;
  } else if (str_metal_direction == "VERTICAL") {
    metal_direction = VERTICAL;
  } else if (str_metal_direction == "DIAG45") {
    metal_direction = DIAG45;
  } else if (str_metal_direction == "DIAG135") {
    metal_direction = DIAG135;
  } else {
    std::cout << "Unknown MetalLayer direction: " << str_metal_direction << std::endl;
    exit(1);
  }
  return metal_direction;
}

std::string MetalDirectionStr(MetalDirection metal_direction) {
  std::string s;
  switch (metal_direction) {
    case 0: { s = "HORIZONTAL"; } break;
    case 1: { s = "VERTICAL"; } break;
    case 2: { s = "DIAG45"; } break;
    case 3: { s = "DIAG135"; } break;
    default: {
      std::cout << "MetalLayer direction error! This should never happen!" << std::endl;
      exit(1);
    }
  }
  return s;
}

BlockOrient StrToOrient(std::string &str_orient) {
  BlockOrient orient = N;
  if (str_orient == "N" || str_orient == "R0") {
    orient = N;
  } else if (str_orient == "S" || str_orient == "R180") {
    orient = S;
  } else if (str_orient == "W" || str_orient == "R90") {
    orient = W;
  } else if (str_orient == "E" || str_orient == "R270") {
    orient = E;
  } else if (str_orient == "FN" || str_orient == "MY") {
    orient = FN;
  } else if (str_orient == "FS" || str_orient == "MX") {
    orient = FS;
  } else if (str_orient == "FW" || str_orient == "MX90" || str_orient == "MXR90") {
    orient = FW;
  } else if (str_orient == "FE" || str_orient == "MY90" || str_orient == "MYR90") {
    orient = FE;
  } else {
    std::cout << "Unknown Block orientation: " << str_orient << std::endl;
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
      std::cout << "Block orientation error! This should never happen!" << std::endl;
      exit(1);
    }
  }
  return s;
}

PlaceStatus StrToPlaceStatus(std::string &str_place_status) {
  PlaceStatus place_status = UNPLACED;
  if (str_place_status == "COVER") {
    place_status = COVER;
  } else if (str_place_status == "FIXED" || str_place_status == "LOCKED" || str_place_status == "FIRM") {
    place_status = FIXED;
  } else if (str_place_status == "PLACED" || str_place_status == "SUGGESTED") {
    place_status = PLACED;
  } else if (str_place_status == "UNPLACED" || str_place_status == "NONE") {
    place_status = UNPLACED;
  } else if (str_place_status == "NULL_STATE") {
    place_status = NULL_STATE;
  } else {
    std::cout << "Unknown placement status: " << str_place_status << std::endl;
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
      std::cout << "Unit placement state error! This should never happen!" << std::endl;
      exit(1);
    }
  }
  return s;
}

SignalDirection  StrToSignalDirection(std::string &str_signal_direction) {
  SignalDirection signal_direction = INPUT_;
  if (str_signal_direction == "INPUT") {
    signal_direction = INPUT_;
  } else if (str_signal_direction == "OUTPUT") {
    signal_direction = OUTPUT_;
  } else if (str_signal_direction == "INOUT") {
    signal_direction = INOUT_;
  } else if (str_signal_direction == "FEEDTHRU") {
    signal_direction = FEEDTHRU_;
  } else {
    std::cout << "Unknown SignalDirection: " << str_signal_direction << std::endl;
    exit(0);
  }
  return signal_direction;
}

std::string SignalDirectionStr(SignalDirection signal_direction) {
  std::string s;
  switch (signal_direction) {
    case 0: { s = "INPUT"; } break;
    case 1: { s = "OUTPUT"; } break;
    case 2: { s = "INOUT"; } break;
    case 3: { s = "FEEDTHRU"; } break;
    default: {
      std::cout << "IOPIN signal direction error! This should never happen!" << std::endl;
      exit(1);
    }
  }
  return s;
}
