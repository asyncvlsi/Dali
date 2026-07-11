/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/global_placer/placement_capacity_model.h"

#include <algorithm>
#include <limits>

#include "dali/common/helper.h"

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
    GriddedCapacityConfig config, double demand_normalization)
    : config_(config), demand_normalization_(demand_normalization) {
  DaliExpects(demand_normalization_ > 0.0,
              "Gridded demand normalization must be positive");
}

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
  result.demand = static_cast<double>(estimate.required_gridded_area) /
                  demand_normalization_;
  result.capacity = static_cast<double>(estimate.available_gridded_area);
  // The estimator has already applied target density to row width/capacity.
  result.target_utilization = 1.0;

  PlacementCapacity area_capacity = AreaCapacityModel().Evaluate(
      components, region_width, region_height, whitespace_area,
      target_density, purpose);
  double gridded_pressure = result.Utilization();
  double area_pressure =
      area_capacity.Utilization() / area_capacity.target_utilization;
  // Well-aware capacity is an additional physical constraint. It may require
  // a larger spreading region, but must never relax the raw density target.
  return gridded_pressure > area_pressure ? result : area_capacity;
}

}  // namespace dali
