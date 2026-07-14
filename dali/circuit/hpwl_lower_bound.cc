/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/
#include "dali/circuit/hpwl_lower_bound.h"

#include <algorithm>
#include <array>
#include <cfloat>

#include "dali/circuit/circuit.h"

namespace dali {

struct PinLocationInterval {
  double lower_x = 0.0;
  double upper_x = 0.0;
  double lower_y = 0.0;
  double upper_y = 0.0;
};

/** Return true when an orientation exchanges macro width and height. */
static bool OrientationSwapsDimensions(ComponentOrient orient) {
  return orient == W || orient == E || orient == FW || orient == FE;
}

/** Return the convex hull of all placement-box locations for a movable pin. */
static PinLocationInterval MovablePinLocationInterval(Circuit& circuit,
                                                      NetPin& net_pin) {
  static constexpr std::array<ComponentOrient, 8> kOrientations = {
      N, S, W, E, FN, FS, FW, FE};

  Component* component = net_pin.ComponentPtr();
  Pin* pin = net_pin.PinPtr();
  Macro* macro = component->MacroPtr();
  PinLocationInterval interval{DBL_MAX, -DBL_MAX, DBL_MAX, -DBL_MAX};
  for (ComponentOrient orient : kOrientations) {
    int width =
        OrientationSwapsDimensions(orient) ? macro->Height() : macro->Width();
    int height =
        OrientationSwapsDimensions(orient) ? macro->Width() : macro->Height();
    if (width > circuit.RegionURX() - circuit.RegionLLX() ||
        height > circuit.RegionURY() - circuit.RegionLLY()) {
      continue;
    }

    double offset_x = pin->OffsetX(orient);
    double offset_y = pin->OffsetY(orient);
    interval.lower_x =
        std::min(interval.lower_x, circuit.RegionLLX() + offset_x);
    interval.upper_x =
        std::max(interval.upper_x, circuit.RegionURX() - width + offset_x);
    interval.lower_y =
        std::min(interval.lower_y, circuit.RegionLLY() + offset_y);
    interval.upper_y =
        std::max(interval.upper_y, circuit.RegionURY() - height + offset_y);
  }

  if (interval.lower_x == DBL_MAX) {
    return {static_cast<double>(circuit.RegionLLX()),
            static_cast<double>(circuit.RegionURX()),
            static_cast<double>(circuit.RegionLLY()),
            static_cast<double>(circuit.RegionURY())};
  }
  return interval;
}

/** Return a fixed point or a relaxed movable-pin location interval. */
static PinLocationInterval RelaxedPinLocationInterval(Circuit& circuit,
                                                      NetPin& net_pin) {
  if (net_pin.ComponentPtr()->IsFixed()) {
    return {net_pin.AbsX(), net_pin.AbsX(), net_pin.AbsY(), net_pin.AbsY()};
  }
  return MovablePinLocationInterval(circuit, net_pin);
}

HpwlLowerBound ComputeFixedTerminalHpwlLowerBound(Circuit& circuit) {
  HpwlLowerBound result;
  for (Net& net : circuit.Nets()) {
    double min_x = DBL_MAX;
    double max_x = -DBL_MAX;
    double min_y = DBL_MAX;
    double max_y = -DBL_MAX;
    for (NetPin& pin : net.ComponentPins()) {
      if (!pin.ComponentPtr()->IsFixed()) {
        continue;
      }
      min_x = std::min(min_x, pin.AbsX());
      max_x = std::max(max_x, pin.AbsX());
      min_y = std::min(min_y, pin.AbsY());
      max_y = std::max(max_y, pin.AbsY());
    }
    if (min_x <= max_x) {
      result.x += (max_x - min_x) * net.Weight();
      result.y += (max_y - min_y) * net.Weight();
    }
  }
  result.x *= circuit.GridValueX();
  result.y *= circuit.GridValueY();
  return result;
}

HpwlLowerBound ComputePlacementBoxHpwlLowerBound(Circuit& circuit) {
  HpwlLowerBound result;
  for (Net& net : circuit.Nets()) {
    if (net.ComponentPins().size() <= 1) {
      continue;
    }

    double greatest_lower_x = -DBL_MAX;
    double least_upper_x = DBL_MAX;
    double greatest_lower_y = -DBL_MAX;
    double least_upper_y = DBL_MAX;
    for (NetPin& pin : net.ComponentPins()) {
      PinLocationInterval interval = RelaxedPinLocationInterval(circuit, pin);
      greatest_lower_x = std::max(greatest_lower_x, interval.lower_x);
      least_upper_x = std::min(least_upper_x, interval.upper_x);
      greatest_lower_y = std::max(greatest_lower_y, interval.lower_y);
      least_upper_y = std::min(least_upper_y, interval.upper_y);
    }
    result.x += std::max(0.0, greatest_lower_x - least_upper_x) * net.Weight();
    result.y += std::max(0.0, greatest_lower_y - least_upper_y) * net.Weight();
  }
  result.x *= circuit.GridValueX();
  result.y *= circuit.GridValueY();
  return result;
}

}  // namespace dali
