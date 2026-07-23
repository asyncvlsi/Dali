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

/**
 * @file
 * Capacity model that reports gridded stripe occupancy to global placement.
 *
 * Lets the density model account for the height a set of components will
 * actually occupy once grouped into rows, rather than assuming area alone.
 */
#include "dali/placer/well_legalizer/gridded_stripe_capacity_model.h"

namespace dali {

GriddedStripeCapacityModel::GriddedStripeCapacityModel(
    GriddedCapacityConfig config)
    : estimator_(config) {}

GriddedStripeCapacitySummary GriddedStripeCapacityModel::Estimate(
    const StripeColumn& column) const {
  GriddedStripeCapacitySummary summary;
  summary.entries.reserve(column.stripe_list_.size());
  for (const Stripe& stripe : column.stripe_list_) {
    const unsigned long long whitespace_area =
        static_cast<unsigned long long>(stripe.Width()) * stripe.Height();
    GriddedCapacityEstimate estimate =
        estimator_.Estimate(stripe.component_ptrs_vec_, stripe.Width(),
                            stripe.Height(), whitespace_area);
    summary.entries.push_back({&stripe, estimate});
    summary.raw_component_area += estimate.raw_component_area;
    summary.required_gridded_area += estimate.required_gridded_area;
    summary.available_gridded_area += estimate.available_gridded_area;
    summary.predicted_overflow_area += estimate.predicted_overflow_area;
    summary.estimated_row_count += estimate.estimated_row_count;
    summary.unplaceable_component_count += estimate.unplaceable_component_count;
    summary.single_region_fallback_count +=
        estimate.single_region_fallback_count;
    if (estimate.predicted_overflow_area > 0) {
      ++summary.overflowing_stripe_count;
    }
  }
  return summary;
}

}  // namespace dali
