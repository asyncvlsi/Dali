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
#include "dali/placer/well_legalizer/gridded_capacity_estimator.h"

#include <algorithm>

#include "dali/common/helper.h"

namespace dali {

GriddedCapacityEstimator::GriddedCapacityEstimator(GriddedCapacityConfig config)
    : config_(config) {
  DaliExpects(config_.reserved_width >= 0,
              "Reserved gridded-row width cannot be negative");
  DaliExpects(config_.minimum_p_well_height >= 0,
              "Minimum P-well height cannot be negative");
  DaliExpects(config_.minimum_n_well_height >= 0,
              "Minimum N-well height cannot be negative");
}

GriddedCapacityEstimate GriddedCapacityEstimator::Estimate(
    const std::vector<Component*>& components, int region_width,
    int region_height, unsigned long long raw_whitespace_area) const {
  DaliExpects(region_width >= 0, "Region width cannot be negative");
  DaliExpects(region_height >= 0, "Region height cannot be negative");

  GriddedCapacityEstimate estimate;
  estimate.usable_row_width =
      std::max(0, region_width - config_.reserved_width);

  if (region_width > 0) {
    unsigned long long width = static_cast<unsigned long long>(region_width);
    unsigned long long usable_width =
        static_cast<unsigned long long>(estimate.usable_row_width);
    // Divide first so area * width cannot overflow on very large designs.
    estimate.available_gridded_area =
        (raw_whitespace_area / width) * usable_width +
        (raw_whitespace_area % width) * usable_width / width;
  }

  struct ComponentDemand {
    int width;
    int row_height;
    unsigned long long raw_area;
  };
  std::vector<ComponentDemand> demands;
  demands.reserve(components.size());
  for (const Component* component : components) {
    if (component == nullptr || !component->IsMovable()) {
      continue;
    }
    const Macro* macro = component->MacroPtr();
    DaliExpects(macro != nullptr, "Movable component has no cell master");

    int p_well_height =
        std::max(macro->FirstPwellHeight(), config_.minimum_p_well_height);
    int n_well_height =
        std::max(macro->FirstNwellHeight(), config_.minimum_n_well_height);
    demands.push_back({component->Width(), p_well_height + n_well_height,
                       static_cast<unsigned long long>(component->Area())});
    estimate.raw_component_area += demands.back().raw_area;
  }

  std::sort(demands.begin(), demands.end(),
            [](const ComponentDemand& lhs, const ComponentDemand& rhs) {
              if (lhs.row_height != rhs.row_height) {
                return lhs.row_height > rhs.row_height;
              }
              return lhs.width > rhs.width;
            });

  struct Shelf {
    int remaining_width;
    int height;
  };
  std::vector<Shelf> shelves;
  unsigned long long horizontal_overflow_area = 0;
  for (const ComponentDemand& demand : demands) {
    if (demand.width > estimate.usable_row_width ||
        estimate.usable_row_width == 0) {
      ++estimate.unplaceable_component_count;
      horizontal_overflow_area +=
          static_cast<unsigned long long>(demand.width) * demand.row_height;
      continue;
    }

    auto shelf = std::find_if(
        shelves.begin(), shelves.end(), [&](const Shelf& candidate) {
          return candidate.remaining_width >= demand.width;
        });
    if (shelf == shelves.end()) {
      shelves.push_back(
          {estimate.usable_row_width - demand.width, demand.row_height});
      estimate.required_row_height += demand.row_height;
    } else {
      shelf->remaining_width -= demand.width;
    }
  }

  estimate.estimated_row_count = static_cast<int>(shelves.size());
  unsigned long long packed_shelf_area =
      static_cast<unsigned long long>(estimate.usable_row_width) *
      estimate.required_row_height;
  estimate.required_gridded_area = packed_shelf_area + horizontal_overflow_area;
  estimate.predicted_overflow_area = horizontal_overflow_area;
  if (packed_shelf_area > estimate.available_gridded_area) {
    estimate.predicted_overflow_area +=
        packed_shelf_area - estimate.available_gridded_area;
  }
  return estimate;
}

}  // namespace dali
