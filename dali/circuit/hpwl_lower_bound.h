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
#ifndef DALI_CIRCUIT_HPWL_LOWER_BOUND_H_
#define DALI_CIRCUIT_HPWL_LOWER_BOUND_H_

namespace dali {

class Circuit;

/** Weighted HPWL lower bound in physical micron units. */
struct HpwlLowerBound {
  double x = 0.0;
  double y = 0.0;

  /** Return the combined X and Y lower bound. */
  double Total() const { return x + y; }
};

/**
 * Compute HPWL forced by fixed pins on each net.
 *
 * Movable pins are unconstrained, so this is a certified but generally weak
 * lower bound for every legal placement.
 */
HpwlLowerBound ComputeFixedTerminalHpwlLowerBound(Circuit& circuit);

/**
 * Compute a net-separable placement-box HPWL lower bound.
 *
 * Each movable pin may independently choose any location reachable by placing
 * its component in the placement box under any orientation. The relaxation
 * ignores overlap, density, wells, and the requirement that pins on one
 * component share a location and orientation. Every legal placement is
 * therefore represented by the relaxed problem, making the result a certified
 * lower bound.
 */
HpwlLowerBound ComputePlacementBoxHpwlLowerBound(Circuit& circuit);

}  // namespace dali

#endif  // DALI_CIRCUIT_HPWL_LOWER_BOUND_H_
