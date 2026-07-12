/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *******************************************************************************/
#ifndef DALI_PLACER_WELL_LEGALIZER_ADAPTIVE_STRIPE_BOUNDARY_PLANNER_H_
#define DALI_PLACER_WELL_LEGALIZER_ADAPTIVE_STRIPE_BOUNDARY_PLANNER_H_

#include <vector>

namespace dali {

/** One movable component's horizontal location and gridded packing demand. */
struct StripeDemandSample {
  double x = 0.0;
  double demand = 0.0;
};

/** Physical and numerical constraints for adaptive stripe planning. */
struct AdaptiveStripeBoundaryConfig {
  int region_left = 0;
  int region_right = 0;
  int column_count = 1;
  int minimum_column_pitch = 1;
  int maximum_column_pitch = 0;
  int boundary_step = 1;
  int spacing_per_column = 0;
};

/** Result of one adaptive stripe-boundary optimization. */
struct AdaptiveStripeBoundaryResult {
  bool feasible = false;
  double objective = 0.0;
  std::vector<int> boundaries;
};

/**
 * Chooses contiguous stripe boundaries that balance gridded demand against
 * usable horizontal capacity.
 *
 * Candidate cutlines are sampled at a configured grid step. Dynamic
 * programming then minimizes the squared mismatch between demand assigned to
 * each interval and the demand supported by that interval's usable width.
 * Minimum and maximum pitch constraints keep every result physically useful
 * and prevent the optimizer from creating pathological narrow or wide rows.
 */
class AdaptiveStripeBoundaryPlanner {
 public:
  explicit AdaptiveStripeBoundaryPlanner(
      AdaptiveStripeBoundaryConfig config);

  /** Optimize stripe boundaries for the supplied horizontal demand samples. */
  AdaptiveStripeBoundaryResult Plan(
      const std::vector<StripeDemandSample>& samples) const;

 private:
  AdaptiveStripeBoundaryConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_ADAPTIVE_STRIPE_BOUNDARY_PLANNER_H_
