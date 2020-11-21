//
// Created by Yihang Yang on 11/27/19.
//

#include "status.h"

#include <iostream>

#include "common/logging.h"

namespace dali {

MetalDirection StrToMetalDirection(std::string &str_metal_direction) {
  MetalDirection metal_direction = HORIZONTAL_;
  if (str_metal_direction == "HORIZONTAL") {
    metal_direction = HORIZONTAL_;
  } else if (str_metal_direction == "VERTICAL") {
    metal_direction = VERTICAL_;
  } else if (str_metal_direction == "DIAG45") {
    metal_direction = DIAG45_;
  } else if (str_metal_direction == "DIAG135") {
    metal_direction = DIAG135_;
  } else {
    BOOST_LOG_TRIVIAL(info) << "Unknown MetalLayer direction: " << str_metal_direction << std::endl;
    exit(1);
  }
  return metal_direction;
}

std::string MetalDirectionStr(MetalDirection metal_direction) {
  std::string s;
  switch (metal_direction) {
    case 0: { s = "HORIZONTAL"; }
      break;
    case 1: { s = "VERTICAL"; }
      break;
    case 2: { s = "DIAG45"; }
      break;
    case 3: { s = "DIAG135"; }
      break;
    default: {
      BOOST_LOG_TRIVIAL(info) << "MetalLayer direction error! This should never happen!" << std::endl;
      exit(1);
    }
  }
  return s;
}

BlockOrient StrToOrient(std::string &str_orient) {
  BlockOrient orient = N_;
  if (str_orient == "N" || str_orient == "R0") {
    orient = N_;
  } else if (str_orient == "S" || str_orient == "R180") {
    orient = S_;
  } else if (str_orient == "W" || str_orient == "R90") {
    orient = W_;
  } else if (str_orient == "E" || str_orient == "R270") {
    orient = E_;
  } else if (str_orient == "FN" || str_orient == "MY") {
    orient = FN_;
  } else if (str_orient == "FS" || str_orient == "MX") {
    orient = FS_;
  } else if (str_orient == "FW" || str_orient == "MX90" || str_orient == "MXR90") {
    orient = FW_;
  } else if (str_orient == "FE" || str_orient == "MY90" || str_orient == "MYR90") {
    orient = FE_;
  } else {
    BOOST_LOG_TRIVIAL(info) << "Unknown Block orientation: " << str_orient << std::endl;
    exit(1);
  }
  return orient;
}

std::string OrientStr(BlockOrient orient) {
  std::string s;
  switch (orient) {
    case 0: { s = "N"; }
      break;
    case 1: { s = "S"; }
      break;
    case 2: { s = "W"; }
      break;
    case 3: { s = "E"; }
      break;
    case 4: { s = "FN"; }
      break;
    case 5: { s = "FS"; }
      break;
    case 6: { s = "FW"; }
      break;
    case 7: { s = "FE"; }
      break;
    default: {
      BOOST_LOG_TRIVIAL(info) << "Block orientation error! This should never happen!" << std::endl;
      exit(1);
    }
  }
  return s;
}

PlaceStatus StrToPlaceStatus(std::string &str_place_status) {
  PlaceStatus place_status = UNPLACED_;
  if (str_place_status == "COVER") {
    place_status = COVER_;
  } else if (str_place_status == "FIXED" || str_place_status == "LOCKED" || str_place_status == "FIRM") {
    place_status = FIXED_;
  } else if (str_place_status == "PLACED" || str_place_status == "SUGGESTED") {
    place_status = PLACED_;
  } else if (str_place_status == "UNPLACED" || str_place_status == "NONE") {
    place_status = UNPLACED_;
  } else if (str_place_status == "NULL_STATE") {
    place_status = NULL_STATE_;
  } else {
    BOOST_LOG_TRIVIAL(info) << "Unknown placement status: " << str_place_status << std::endl;
    exit(1);
  }
  return place_status;
}

std::string PlaceStatusStr(PlaceStatus place_status) {
  std::string s;
  switch (place_status) {
    case 0: { s = "COVER"; }
      break;
    case 1: { s = "FIXED"; }
      break;
    case 2: { s = "PLACED"; }
      break;
    case 3: { s = "UNPLACED"; }
      break;
    case 4: { s = "NULL_STATE"; }
      break;
    default: {
      BOOST_LOG_TRIVIAL(info) << "Unit placement state error! This should never happen!" << std::endl;
      exit(1);
    }
  }
  return s;
}

SignalDirection StrToSignalDirection(std::string &str_signal_direction) {
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
    BOOST_LOG_TRIVIAL(info) << "Unknown SignalDirection: " << str_signal_direction << std::endl;
    exit(0);
  }
  return signal_direction;
}

std::string SignalDirectionStr(SignalDirection signal_direction) {
  std::string s;
  switch (signal_direction) {
    case 0: { s = "INPUT"; }
      break;
    case 1: { s = "OUTPUT"; }
      break;
    case 2: { s = "INOUT"; }
      break;
    case 3: { s = "FEEDTHRU"; }
      break;
    default: {
      BOOST_LOG_TRIVIAL(info) << "IOPIN signal direction error! This should never happen!" << std::endl;
      exit(1);
    }
  }
  return s;
}

SignalUse StrToSignalUse(std::string &str_signal_use) {
  SignalUse signal_use = SIGNAL_;
  if (str_signal_use == "SIGNAL") {
    signal_use = SIGNAL_;
  } else if (str_signal_use == "POWER") {
    signal_use = POWER_;
  } else if (str_signal_use == "GROUND") {
    signal_use = GROUND_;
  } else if (str_signal_use == "CLOCK") {
    signal_use = CLOCK_;
  } else if (str_signal_use == "TIEOFF") {
    signal_use = TIEOFF_;
  } else if (str_signal_use == "ANALOG") {
    signal_use = ANALOG_;
  } else if (str_signal_use == "SCAN") {
    signal_use = SCAN_;
  } else if (str_signal_use == "RESET") {
    signal_use = RESET_;
  } else {
    BOOST_LOG_TRIVIAL(info) << "Unknown SignalUse: " << str_signal_use << std::endl;
    exit(0);
  }
  return signal_use;
}

std::string SignalUseStr(SignalUse signal_use) {
  std::string s;
  switch (signal_use) {
    case 0: { s = "SIGNAL"; }
      break;
    case 1: { s = "POWER"; }
      break;
    case 2: { s = "GROUND"; }
      break;
    case 3: { s = "CLOCK"; }
      break;
    case 4: { s = "TIEOFF"; }
      break;
    case 5: { s = "ANALOG"; }
      break;
    case 6: { s = "SCAN"; }
      break;
    case 7: { s = "RESET"; }
      break;
    default: {
      BOOST_LOG_TRIVIAL(info) << "IOPIN signal use error! This should never happen!" << std::endl;
      exit(1);
    }
  }
  return s;
}

}

