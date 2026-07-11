/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#ifndef DALI_PLACER_GLOBAL_PLACER_PLACEMENT_CAPACITY_MODEL_H_
#define DALI_PLACER_GLOBAL_PLACER_PLACEMENT_CAPACITY_MODEL_H_

#include <vector>

#include "dali/circuit/component.h"
#include "dali/placer/well_legalizer/gridded_capacity_estimator.h"

namespace dali {

/** Geometric role of a capacity query in look-ahead spreading. */
enum class CapacityEvaluationPurpose {
  kDensityBin,
  kHotspot,
  kSpreadingRegion,
};

/** Demand and capacity reported for one global-placement region. */
struct PlacementCapacity {
  double demand = 0.0;
  double capacity = 0.0;
  double target_utilization = 1.0;

  double Utilization() const;
  double Overflow() const;
  bool IsOverfilled() const;
};

/** Policy used by global spreading to interpret regional free space. */
class PlacementCapacityModel {
 public:
  virtual ~PlacementCapacityModel() = default;

  /** Evaluate movable demand against whitespace in a rectangular region. */
  virtual PlacementCapacity Evaluate(
      const std::vector<Component*>& components, int region_width,
      int region_height, unsigned long long whitespace_area,
      double target_density, CapacityEvaluationPurpose purpose) const = 0;
};

/** Traditional capacity model based only on component and whitespace area. */
class AreaCapacityModel : public PlacementCapacityModel {
 public:
  PlacementCapacity Evaluate(const std::vector<Component*>& components,
                             int region_width, int region_height,
                             unsigned long long whitespace_area,
                             double target_density,
                             CapacityEvaluationPurpose purpose) const override;
};

/** Capacity model that accounts for gridded rows, wells, and row completion. */
class GriddedPlacementCapacityModel : public PlacementCapacityModel {
 public:
  explicit GriddedPlacementCapacityModel(GriddedCapacityConfig config);

  PlacementCapacity Evaluate(const std::vector<Component*>& components,
                             int region_width, int region_height,
                             unsigned long long whitespace_area,
                             double target_density,
                             CapacityEvaluationPurpose purpose) const override;

 private:
  GriddedCapacityConfig config_;
};

}  // namespace dali

#endif
