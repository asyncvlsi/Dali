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
#ifndef DALI_PLACER_WELL_LEGALIZER_PACKED_STRIPE_BOUNDARY_PLANNER_H_
#define DALI_PLACER_WELL_LEGALIZER_PACKED_STRIPE_BOUNDARY_PLANNER_H_

#include <vector>

#include "dali/placer/well_legalizer/adaptive_stripe_boundary_planner.h"

namespace dali {

/** One component's contribution to a compatible gridded-row signature. */
struct StripePackingSample {
  double x = 0.0;
  int width = 0;
  int signature_height = 0;
  int signature_id = 0;
};

/**
 * Optimizes stripe boundaries using compatible-row packing demand.
 *
 * Components with the same complete P/N-well signature may share horizontal
 * shelves. For every candidate interval, the planner computes the shelf count
 * required by each signature at that interval's usable width. Dynamic
 * programming chooses legal cutlines that balance the resulting packed row
 * height across columns. This makes row-boundary overhead and partially filled
 * shelves visible to the objective before full legalization.
 */
class PackedStripeBoundaryPlanner {
 public:
  explicit PackedStripeBoundaryPlanner(
      AdaptiveStripeBoundaryConfig config);

  /** Optimize boundaries and return an infeasible result if pitches conflict. */
  AdaptiveStripeBoundaryResult Plan(
      const std::vector<StripePackingSample>& samples) const;

 private:
  AdaptiveStripeBoundaryConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_PACKED_STRIPE_BOUNDARY_PLANNER_H_
