/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/

/**
 * @file
 * Asks how much room a set of components will really need.
 *
 * Standard cells occupy their area, so the default model is area over white
 * space. The gridded flow substitutes a model that accounts for row
 * quantization, which lets global placement see the capacity legalization will
 * actually impose rather than discovering it later.
 */
#include "dali/placer/global_placer/placement_capacity_model.h"

#include <algorithm>
#include <limits>
#include <utility>

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
    return AreaCapacityModel().Evaluate(components, region_width, region_height,
                                        whitespace_area, target_density,
                                        purpose);
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

  PlacementCapacity area_capacity =
      AreaCapacityModel().Evaluate(components, region_width, region_height,
                                   whitespace_area, target_density, purpose);
  double gridded_pressure = result.Utilization();
  double area_pressure =
      area_capacity.Utilization() / area_capacity.target_utilization;
  // Well-aware capacity is an additional physical constraint. It may require
  // a larger spreading region, but must never relax the raw density target.
  return gridded_pressure > area_pressure ? result : area_capacity;
}

LegalizationPressureCapacityModel::LegalizationPressureCapacityModel(
    std::shared_ptr<const PlacementCapacityModel> base_model)
    : base_model_(std::move(base_model)) {
  DaliExpects(base_model_ != nullptr,
              "Legalization pressure requires a base capacity model");
}

void LegalizationPressureCapacityModel::SetDemandMultipliers(
    std::vector<double> demand_multipliers) {
  for (double multiplier : demand_multipliers) {
    DaliExpects(multiplier >= 1.0,
                "Legalization pressure multipliers cannot reduce demand");
  }
  demand_multipliers_ = std::move(demand_multipliers);
}

PlacementCapacity LegalizationPressureCapacityModel::Evaluate(
    const std::vector<Component*>& components, int region_width,
    int region_height, unsigned long long whitespace_area,
    double target_density, CapacityEvaluationPurpose purpose) const {
  PlacementCapacity capacity =
      base_model_->Evaluate(components, region_width, region_height,
                            whitespace_area, target_density, purpose);

  double movable_area = 0.0;
  double weighted_multiplier = 0.0;
  for (const Component* component : components) {
    if (component == nullptr || !component->IsMovable()) continue;
    double component_area = static_cast<double>(component->Area());
    double multiplier = 1.0;
    if (component->Id() >= 0 &&
        component->Id() < static_cast<int>(demand_multipliers_.size())) {
      multiplier = demand_multipliers_[component->Id()];
    }
    movable_area += component_area;
    weighted_multiplier += component_area * multiplier;
  }
  if (movable_area > 0.0) {
    capacity.demand *= weighted_multiplier / movable_area;
  }
  return capacity;
}

}  // namespace dali
