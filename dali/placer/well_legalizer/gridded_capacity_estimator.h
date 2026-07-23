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
#ifndef DALI_PLACER_WELL_LEGALIZER_GRIDDED_CAPACITY_ESTIMATOR_H_
#define DALI_PLACER_WELL_LEGALIZER_GRIDDED_CAPACITY_ESTIMATOR_H_

#include <vector>

#include "dali/circuit/component.h"

namespace dali {

/** Physical row costs that raw component area does not include. */
struct GriddedCapacityConfig {
  int reserved_width = 0;
  int minimum_p_well_height = 0;
  int minimum_n_well_height = 0;
  double target_density = 1.0;
};

/** Read-only estimate of how components pack into a rectangular region. */
struct GriddedCapacityEstimate {
  unsigned long long raw_component_area = 0;
  unsigned long long required_gridded_area = 0;
  unsigned long long available_gridded_area = 0;
  unsigned long long predicted_overflow_area = 0;
  int usable_row_width = 0;
  int target_row_width = 0;
  int required_row_height = 0;
  int estimated_row_count = 0;
  int unplaceable_component_count = 0;
  int single_region_fallback_count = 0;
};

/**
 * Estimates gridded-row demand without changing component placement.
 *
 * Components are sorted by required N/P-well height and packed into horizontal
 * shelves using a best-fit decreasing heuristic. Each shelf tracks maximum P
 * and N heights separately and reserves the configured tap/end-cap width. This
 * models row-height and row-boundary overhead while remaining much cheaper than
 * trial well legalization.
 */
class GriddedCapacityEstimator {
 public:
  /** Estimates the stripe height a component set occupies once grouped into rows. */
  explicit GriddedCapacityEstimator(GriddedCapacityConfig config);

  /**
   * Estimate demand and overflow for one rectangular placement region.
   *
   * `raw_whitespace_area` excludes fixed obstacles. The estimate scales it by
   * the usable-row-width ratio to account for physical completion reservation.
   */
  GriddedCapacityEstimate Estimate(
      const std::vector<Component*>& components, int region_width,
      int region_height, unsigned long long raw_whitespace_area) const;

  /**
   * Estimate one component's standalone gridded demand.
   *
   * The value is component width multiplied by the sum of its P/N-well region
   * heights after applying physical-cell minimum heights. It intentionally
   * excludes row sharing, which is unknown before stripe boundaries exist.
   */
  unsigned long long EstimateStandaloneDemand(const Component& component) const;

 private:
  GriddedCapacityConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_GRIDDED_CAPACITY_ESTIMATOR_H_
