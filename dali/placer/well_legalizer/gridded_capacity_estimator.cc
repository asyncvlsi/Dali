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
 * Estimates how much stripe height a set of components will consume.
 *
 * Because a gridded row is as tall as its tallest cell, height depends on how
 * components group into rows, not just on their total area. This estimates that
 * before the rows are built, so global placement can be steered by a capacity
 * model rather than discovering overflow at legalization time.
 */
#include "dali/placer/well_legalizer/gridded_capacity_estimator.h"

#include <algorithm>
#include <cmath>
#include <utility>

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
  DaliExpects(config_.target_density > 0.0 && config_.target_density <= 1.0,
              "Gridded capacity target density must be in (0, 1]");
}

unsigned long long GriddedCapacityEstimator::EstimateStandaloneDemand(
    const Component& component) const {
  const Macro* macro = component.MacroPtr();
  DaliExpects(macro != nullptr, "Movable component has no cell master");

  int total_height = 0;
  if (macro->HasCompleteWellRegions()) {
    for (int region_id = 0; region_id < macro->RegionCount(); ++region_id) {
      total_height +=
          std::max(macro->PwellHeight(region_id, component.IsFlipped()),
                   config_.minimum_p_well_height) +
          std::max(macro->NwellHeight(region_id, component.IsFlipped()),
                   config_.minimum_n_well_height);
    }
  } else {
    total_height =
        std::max(macro->FirstPwellHeight(), config_.minimum_p_well_height) +
        std::max(macro->FirstNwellHeight(), config_.minimum_n_well_height);
  }
  return static_cast<unsigned long long>(component.Width()) * total_height;
}

/**
 * Estimate the stripe height a set of components will occupy once grouped into
 * rows.
 *
 * Height depends on how components pack into rows, not just their area, so this
 * models the grouping rather than summing areas.
 * @param region_width the width components must pack within.
 * @return required height and the assumptions behind it.
 */
GriddedCapacityEstimate GriddedCapacityEstimator::Estimate(
    const std::vector<Component*>& components, int region_width,
    int region_height, unsigned long long raw_whitespace_area) const {
  DaliExpects(region_width >= 0, "Region width cannot be negative");
  DaliExpects(region_height >= 0, "Region height cannot be negative");

  GriddedCapacityEstimate estimate;
  estimate.usable_row_width =
      std::max(0, region_width - config_.reserved_width);
  estimate.target_row_width = static_cast<int>(
      std::floor(estimate.usable_row_width * config_.target_density));

  if (region_width > 0) {
    unsigned long long width = static_cast<unsigned long long>(region_width);
    unsigned long long target_width =
        static_cast<unsigned long long>(estimate.target_row_width);
    // Divide first so area * width cannot overflow on very large designs.
    estimate.available_gridded_area =
        (raw_whitespace_area / width) * target_width +
        (raw_whitespace_area % width) * target_width / width;
  }

  struct ComponentDemand {
    struct RegionDemand {
      int p_well_height;
      int n_well_height;
    };

    int width;
    std::vector<RegionDemand> regions;

    int TotalHeight() const {
      int total_height = 0;
      for (const RegionDemand& region : regions) {
        total_height += region.p_well_height + region.n_well_height;
      }
      return total_height;
    }
  };
  std::vector<ComponentDemand> demands;
  demands.reserve(components.size());
  for (const Component* component : components) {
    if (component == nullptr || !component->IsMovable()) {
      continue;
    }
    const Macro* macro = component->MacroPtr();
    DaliExpects(macro != nullptr, "Movable component has no cell master");

    ComponentDemand demand;
    demand.width = component->Width();
    if (macro->HasCompleteWellRegions()) {
      demand.regions.reserve(macro->RegionCount());
      for (int region_id = 0; region_id < macro->RegionCount(); ++region_id) {
        int p_well_height =
            std::max(macro->PwellHeight(region_id, component->IsFlipped()),
                     config_.minimum_p_well_height);
        int n_well_height =
            std::max(macro->NwellHeight(region_id, component->IsFlipped()),
                     config_.minimum_n_well_height);
        demand.regions.push_back({p_well_height, n_well_height});
      }
    } else {
      ++estimate.single_region_fallback_count;
      int p_well_height =
          std::max(macro->FirstPwellHeight(), config_.minimum_p_well_height);
      int n_well_height =
          std::max(macro->FirstNwellHeight(), config_.minimum_n_well_height);
      demand.regions.push_back({p_well_height, n_well_height});
    }
    demands.push_back(std::move(demand));
    estimate.raw_component_area +=
        static_cast<unsigned long long>(component->Area());
  }

  std::sort(demands.begin(), demands.end(),
            [](const ComponentDemand& lhs, const ComponentDemand& rhs) {
              if (lhs.regions.size() != rhs.regions.size()) {
                return lhs.regions.size() > rhs.regions.size();
              }
              if (lhs.TotalHeight() != rhs.TotalHeight()) {
                return lhs.TotalHeight() > rhs.TotalHeight();
              }
              return lhs.width > rhs.width;
            });

  struct Shelf {
    int remaining_width;
    std::vector<ComponentDemand::RegionDemand> regions;

    int TotalHeight() const {
      int total_height = 0;
      for (const ComponentDemand::RegionDemand& region : regions) {
        total_height += region.p_well_height + region.n_well_height;
      }
      return total_height;
    }
  };
  std::vector<Shelf> shelves;
  unsigned long long horizontal_overflow_area = 0;
  for (const ComponentDemand& demand : demands) {
    if (demand.width > estimate.target_row_width ||
        estimate.target_row_width == 0) {
      ++estimate.unplaceable_component_count;
      horizontal_overflow_area +=
          static_cast<unsigned long long>(demand.width) * demand.TotalHeight();
      continue;
    }

    auto best_shelf = shelves.end();
    int best_height_increase = demand.TotalHeight();
    int best_remaining_width = estimate.target_row_width - demand.width;
    for (auto shelf = shelves.begin(); shelf != shelves.end(); ++shelf) {
      if (shelf->remaining_width < demand.width ||
          shelf->regions.size() != demand.regions.size()) {
        continue;
      }
      int height_increase = 0;
      for (size_t region_id = 0; region_id < demand.regions.size();
           ++region_id) {
        const auto& shelf_region = shelf->regions[region_id];
        const auto& demand_region = demand.regions[region_id];
        height_increase +=
            std::max(shelf_region.p_well_height, demand_region.p_well_height) +
            std::max(shelf_region.n_well_height, demand_region.n_well_height) -
            shelf_region.p_well_height - shelf_region.n_well_height;
      }
      int remaining_width = shelf->remaining_width - demand.width;
      if (best_shelf == shelves.end() ||
          height_increase < best_height_increase ||
          (height_increase == best_height_increase &&
           remaining_width < best_remaining_width)) {
        best_shelf = shelf;
        best_height_increase = height_increase;
        best_remaining_width = remaining_width;
      }
    }

    if (best_shelf == shelves.end()) {
      shelves.push_back(
          {estimate.target_row_width - demand.width, demand.regions});
      estimate.required_row_height += demand.TotalHeight();
      estimate.estimated_row_count += static_cast<int>(demand.regions.size());
    } else {
      best_shelf->remaining_width -= demand.width;
      for (size_t region_id = 0; region_id < demand.regions.size();
           ++region_id) {
        auto& shelf_region = best_shelf->regions[region_id];
        const auto& demand_region = demand.regions[region_id];
        shelf_region.p_well_height =
            std::max(shelf_region.p_well_height, demand_region.p_well_height);
        shelf_region.n_well_height =
            std::max(shelf_region.n_well_height, demand_region.n_well_height);
      }
      estimate.required_row_height += best_height_increase;
    }
  }

  unsigned long long packed_shelf_area =
      static_cast<unsigned long long>(estimate.target_row_width) *
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
