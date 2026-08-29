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
#include "dali/placer/global_placer/stage_band.h"

#include <cstddef>
#include <numeric>

namespace dali {

std::vector<StageBandInterval> BuildStageBandIntervals(
    const std::vector<double> &areas, double region_lo, double region_hi,
    StageBandSpacing spacing) {
  std::vector<StageBandInterval> intervals;
  const double region_height = region_hi - region_lo;
  if (areas.empty() || region_height <= 0.0) return intervals;

  if (spacing == StageBandSpacing::kUniform) {
    // Every band counts, including an empty one: uniform spacing is chosen for
    // the arrangement it produces, and skipping a stage would shift the rest.
    intervals.reserve(areas.size());
    const double count = static_cast<double>(areas.size());
    for (size_t index = 0; index < areas.size(); ++index) {
      intervals.push_back(
          {region_lo + region_height * (static_cast<double>(index) / count),
           region_lo +
               region_height * (static_cast<double>(index + 1) / count)});
    }
    return intervals;
  }

  double total_area = 0.0;
  for (double area : areas) {
    if (area > 0.0) total_area += area;
  }
  if (total_area <= 0.0) return intervals;

  // Accumulating the boundary rather than the height keeps the last band's top
  // exactly on region_hi instead of a rounding error below it.
  intervals.reserve(areas.size());
  double consumed_area = 0.0;
  double lower = region_lo;
  for (double area : areas) {
    consumed_area += area > 0.0 ? area : 0.0;
    const double upper =
        region_lo + region_height * (consumed_area / total_area);
    intervals.push_back({lower, upper});
    lower = upper;
  }
  return intervals;
}

} // namespace dali
