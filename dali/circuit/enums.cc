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

#include "enums.h"

#include "dali/common/logging.h"

namespace dali {

MetalDirection StrToMetalDirection(std::string const &str_metal_direction) {
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
    DaliExpects(false, "Unknown MetalLayer direction: " + str_metal_direction);
  }
  return metal_direction;
}

std::string MetalDirectionStr(MetalDirection metal_direction) {
  std::string s;
  switch (metal_direction) {
    case 0: {
      s = "HORIZONTAL";
    } break;
    case 1: {
      s = "VERTICAL";
    } break;
    case 2: {
      s = "DIAG45";
    } break;
    case 3: {
      s = "DIAG135";
    } break;
    default: {
      DaliExpects(false,
                  "MetalLayer direction error! This should never happen!");
    }
  }
  return s;
}

BlockOrient StrToOrient(std::string const &str_orient) {
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
  } else if (str_orient == "FW" || str_orient == "MX90" ||
             str_orient == "MXR90") {
    orient = FW;
  } else if (str_orient == "FE" || str_orient == "MY90" ||
             str_orient == "MYR90") {
    orient = FE;
  } else {
    DaliExpects(false, "Unknown Block orientation: " + str_orient);
  }
  return orient;
}

std::string OrientStr(BlockOrient orient) {
  std::string s;
  switch (orient) {
    case 0: {
      s = "N";
    } break;
    case 1: {
      s = "S";
    } break;
    case 2: {
      s = "W";
    } break;
    case 3: {
      s = "E";
    } break;
    case 4: {
      s = "FN";
    } break;
    case 5: {
      s = "FS";
    } break;
    case 6: {
      s = "FW";
    } break;
    case 7: {
      s = "FE";
    } break;
    default: {
      DaliExpects(false, "Block orientation error! This should never happen!");
    }
  }
  return s;
}

PlaceStatus StrToPlaceStatus(std::string const &str_place_status) {
  PlaceStatus place_status = UNPLACED;
  if (str_place_status == "COVER") {
    place_status = COVER;
  } else if (str_place_status == "FIXED" || str_place_status == "LOCKED" ||
             str_place_status == "FIRM") {
    place_status = FIXED;
  } else if (str_place_status == "PLACED" || str_place_status == "SUGGESTED") {
    place_status = PLACED;
  } else if (str_place_status == "UNPLACED" || str_place_status == "NONE") {
    place_status = UNPLACED;
  } else {
    DaliExpects(false, "Unknown placement status: " + str_place_status);
  }
  return place_status;
}

std::string PlaceStatusStr(PlaceStatus place_status) {
  std::string s;
  switch (place_status) {
    case 0: {
      s = "COVER";
    } break;
    case 1: {
      s = "FIXED";
    } break;
    case 2: {
      s = "PLACED";
    } break;
    case 3: {
      s = "UNPLACED";
    } break;
    default: {
      DaliExpects(false, "Placement status error! This should never happen!");
    }
  }
  return s;
}

SignalDirection StrToSignalDirection(std::string const &str_signal_direction) {
  SignalDirection signal_direction = INPUT;
  if (str_signal_direction == "INPUT") {
    signal_direction = INPUT;
  } else if (str_signal_direction == "OUTPUT") {
    signal_direction = OUTPUT;
  } else if (str_signal_direction == "INOUT") {
    signal_direction = INOUT;
  } else if (str_signal_direction == "FEEDTHRU") {
    signal_direction = FEEDTHRU;
  } else {
    DaliExpects(false, "Unknown SignalDirection: " + str_signal_direction);
  }
  return signal_direction;
}

std::string SignalDirectionStr(SignalDirection signal_direction) {
  std::string s;
  switch (signal_direction) {
    case 0: {
      s = "INPUT";
    } break;
    case 1: {
      s = "OUTPUT";
    } break;
    case 2: {
      s = "INOUT";
    } break;
    case 3: {
      s = "FEEDTHRU";
    } break;
    default: {
      DaliExpects(false,
                  "IOPIN signal direction error! This should never happen!");
    }
  }
  return s;
}

SignalUse StrToSignalUse(std::string const &str_signal_use) {
  SignalUse signal_use = SIGNAL;
  if (str_signal_use == "SIGNAL") {
    signal_use = SIGNAL;
  } else if (str_signal_use == "POWER") {
    signal_use = POWER;
  } else if (str_signal_use == "GROUND") {
    signal_use = GROUND;
  } else if (str_signal_use == "CLOCK") {
    signal_use = CLOCK;
  } else if (str_signal_use == "TIEOFF") {
    signal_use = TIEOFF;
  } else if (str_signal_use == "ANALOG") {
    signal_use = ANALOG;
  } else if (str_signal_use == "SCAN") {
    signal_use = SCAN;
  } else if (str_signal_use == "RESET") {
    signal_use = RESET;
  } else {
    DaliExpects(false, "Unknown SignalUse: " + str_signal_use);
  }
  return signal_use;
}

std::string SignalUseStr(SignalUse signal_use) {
  std::string s;
  switch (signal_use) {
    case 0: {
      s = "SIGNAL";
    } break;
    case 1: {
      s = "POWER";
    } break;
    case 2: {
      s = "GROUND";
    } break;
    case 3: {
      s = "CLOCK";
    } break;
    case 4: {
      s = "TIEOFF";
    } break;
    case 5: {
      s = "ANALOG";
    } break;
    case 6: {
      s = "SCAN";
    } break;
    case 7: {
      s = "RESET";
    } break;
    default: {
      DaliExpects(false, "IOPIN signal use error! This should never happen!");
    }
  }
  return s;
}

}  // namespace dali
