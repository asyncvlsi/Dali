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
#ifndef DALI_PLACER_WELL_LEGALIZER_GRIDDED_STRIPE_CAPACITY_MODEL_H_
#define DALI_PLACER_WELL_LEGALIZER_GRIDDED_STRIPE_CAPACITY_MODEL_H_

#include <vector>

#include "dali/placer/well_legalizer/gridded_capacity_estimator.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Capacity estimate for one rectangular whitespace fragment. */
struct GriddedStripeCapacityEntry {
  const Stripe* stripe = nullptr;
  GriddedCapacityEstimate estimate;
};

/** Aggregate capacity diagnostics while preserving each whitespace fragment. */
struct GriddedStripeCapacitySummary {
  std::vector<GriddedStripeCapacityEntry> entries;
  int overflowing_stripe_count = 0;
  int estimated_row_count = 0;
  int unplaceable_component_count = 0;
  int single_region_fallback_count = 0;
  unsigned long long raw_component_area = 0;
  unsigned long long required_gridded_area = 0;
  unsigned long long available_gridded_area = 0;
  unsigned long long predicted_overflow_area = 0;
};

/**
 * Evaluates each legalizable whitespace fragment independently.
 *
 * A StripeColumn can contain disconnected rectangles because fixed macros
 * block only part of its rows. Components in different rectangles cannot share
 * a gridded row or a tap/end-cap reservation, so callers must not combine
 * their component lists before estimating capacity. This model preserves that
 * physical boundary and is read-only: it never changes ownership or placement.
 */
class GriddedStripeCapacityModel {
 public:
  /** Capacity model reporting gridded stripe occupancy to global placement. */
  explicit GriddedStripeCapacityModel(GriddedCapacityConfig config);

  /** Estimate every rectangular whitespace fragment in one stripe column. */
  GriddedStripeCapacitySummary Estimate(const StripeColumn& column) const;

 private:
  GriddedCapacityEstimator estimator_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_GRIDDED_STRIPE_CAPACITY_MODEL_H_
