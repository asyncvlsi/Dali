/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/global_placer/placement_capacity_model.h"

#include <algorithm>
#include <limits>

namespace dali {

double PlacementCapacity::Utilization() const {
  return capacity > 0.0 ? demand / capacity
                        : std::numeric_limits<double>::infinity();
}

double PlacementCapacity::Overflow() const {
  return std::max(0.0, demand - target_utilization * capacity);
}

bool PlacementCapacity::IsOverfilled() const {
  return demand > target_utilization * capacity;
}

PlacementCapacity AreaCapacityModel::Evaluate(
    const std::vector<Component*>& components, int region_width,
    int region_height, unsigned long long whitespace_area,
    double target_density, CapacityEvaluationPurpose purpose) const {
  (void)region_width;
  (void)region_height;
  (void)purpose;
  PlacementCapacity result;
  result.capacity = static_cast<double>(whitespace_area);
  result.target_utilization = target_density;
  for (const Component* component : components) {
    if (component != nullptr && component->IsMovable()) {
      result.demand += component->Area();
    }
  }
  return result;
}

GriddedPlacementCapacityModel::GriddedPlacementCapacityModel(
    GriddedCapacityConfig config)
    : config_(config) {}

PlacementCapacity GriddedPlacementCapacityModel::Evaluate(
    const std::vector<Component*>& components, int region_width,
    int region_height, unsigned long long whitespace_area,
    double target_density, CapacityEvaluationPurpose purpose) const {
  // Grid bins are density samples, not independent future rows. Charging tap
  // and end-cap reservation in every bin would make narrow edge bins appear
  // to have zero capacity and grossly overstate global overflow.
  if (purpose == CapacityEvaluationPurpose::kDensityBin) {
    return AreaCapacityModel().Evaluate(
        components, region_width, region_height, whitespace_area,
        target_density, purpose);
  }
  GriddedCapacityConfig config = config_;
  config.target_density = target_density;
  GriddedCapacityEstimate estimate = GriddedCapacityEstimator(config).Estimate(
      components, region_width, region_height, whitespace_area);

  PlacementCapacity result;
  result.demand = static_cast<double>(estimate.required_gridded_area);
  result.capacity = static_cast<double>(estimate.available_gridded_area);
  // The estimator has already applied target density to row width/capacity.
  result.target_utilization = 1.0;
  return result;
}

}  // namespace dali
