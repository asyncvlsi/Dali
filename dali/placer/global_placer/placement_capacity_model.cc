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
    double target_density) const {
  (void)region_width;
  (void)region_height;
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

}  // namespace dali
