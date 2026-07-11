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

#include <gtest/gtest.h>

#include "dali/circuit/circuit.h"

namespace dali {

class GriddedCapacityEstimatorTest : public testing::Test {
 protected:
  GriddedCapacityEstimatorTest() {
    circuit_.SetManufacturingGrid(1);
    circuit_.SetUnitsDistanceMicrons(1);
    circuit_.SetGridValue(1, 1);
    circuit_.ReserveSpaceForDesignImp(8, 0, 0);
  }

  Component* AddComponent(const std::string& name, int width, int p_height,
                          int n_height, PlaceStatus status = PLACED) {
    std::string master_name = "master_" + std::to_string(width) + "_" +
                              std::to_string(p_height) + "_" +
                              std::to_string(n_height);
    if (!circuit_.IsMacroExisting(master_name)) {
      Macro* macro = circuit_.AddMacro(master_name, width, p_height + n_height);
      macro->AddWellRect(false, 0, 0, width, p_height);
      macro->AddWellRect(true, 0, p_height, width, p_height + n_height);
    }
    circuit_.AddComponent(name, master_name, 0, 0, status);
    return circuit_.GetComponentPtr(name);
  }

  Circuit circuit_;
};

TEST_F(GriddedCapacityEstimatorTest, PacksEqualHeightCellsIntoSharedRows) {
  std::vector<Component*> components = {AddComponent("a", 4, 4, 6),
                                        AddComponent("b", 4, 4, 6),
                                        AddComponent("c", 4, 4, 6)};
  GriddedCapacityConfig config;
  config.reserved_width = 2;
  GriddedCapacityEstimator estimator(config);

  GriddedCapacityEstimate estimate =
      estimator.Estimate(components, 10, 20, 200);

  EXPECT_EQ(estimate.usable_row_width, 8);
  EXPECT_EQ(estimate.estimated_row_count, 2);
  EXPECT_EQ(estimate.required_row_height, 20);
  EXPECT_EQ(estimate.required_gridded_area, 160);
  EXPECT_EQ(estimate.available_gridded_area, 160);
  EXPECT_EQ(estimate.predicted_overflow_area, 0);
}

TEST_F(GriddedCapacityEstimatorTest, IncludesTapAndEndCapWellHeights) {
  std::vector<Component*> components = {AddComponent("a", 4, 2, 3)};
  GriddedCapacityConfig config;
  config.reserved_width = 2;
  config.minimum_p_well_height = 5;
  config.minimum_n_well_height = 7;
  GriddedCapacityEstimator estimator(config);

  GriddedCapacityEstimate estimate =
      estimator.Estimate(components, 10, 10, 100);

  EXPECT_EQ(estimate.required_row_height, 12);
  EXPECT_EQ(estimate.required_gridded_area, 96);
  EXPECT_EQ(estimate.available_gridded_area, 80);
  EXPECT_EQ(estimate.predicted_overflow_area, 16);
}

TEST_F(GriddedCapacityEstimatorTest, AppliesTargetDensityToRowCapacity) {
  std::vector<Component*> components = {AddComponent("a", 4, 4, 6),
                                        AddComponent("b", 4, 4, 6)};
  GriddedCapacityConfig config;
  config.reserved_width = 2;
  config.target_density = 0.5;
  GriddedCapacityEstimator estimator(config);

  GriddedCapacityEstimate estimate =
      estimator.Estimate(components, 10, 20, 200);

  EXPECT_EQ(estimate.usable_row_width, 8);
  EXPECT_EQ(estimate.target_row_width, 4);
  EXPECT_EQ(estimate.estimated_row_count, 2);
  EXPECT_EQ(estimate.required_row_height, 20);
}

TEST_F(GriddedCapacityEstimatorTest, TracksPAndNWellHeightMaximaSeparately) {
  Macro* p_tall_master = circuit_.AddMacro("p_tall_master", 4, 10);
  p_tall_master->AddWellRect(false, 0, 0, 4, 8);
  p_tall_master->AddWellRect(true, 0, 8, 4, 10);
  Macro* n_tall_master = circuit_.AddMacro("n_tall_master", 4, 10);
  n_tall_master->AddWellRect(false, 0, 0, 4, 2);
  n_tall_master->AddWellRect(true, 0, 2, 4, 10);
  circuit_.AddComponent("p_tall", "p_tall_master", 0, 0, PLACED);
  circuit_.AddComponent("n_tall", "n_tall_master", 0, 0, PLACED);
  std::vector<Component*> components = {circuit_.GetComponentPtr("p_tall"),
                                        circuit_.GetComponentPtr("n_tall")};
  GriddedCapacityConfig config;
  config.reserved_width = 2;
  GriddedCapacityEstimator estimator(config);

  GriddedCapacityEstimate estimate =
      estimator.Estimate(components, 10, 20, 200);

  EXPECT_EQ(estimate.estimated_row_count, 1);
  EXPECT_EQ(estimate.required_row_height, 16);
  EXPECT_EQ(estimate.required_gridded_area, 128);
}

TEST_F(GriddedCapacityEstimatorTest, AccountsForEveryMacroWellRegion) {
  Macro* macro = circuit_.AddMacro("multi_region_master", 4, 20);
  macro->AddWellRect(false, 0, 0, 4, 4);
  macro->AddWellRect(true, 0, 4, 4, 10);
  macro->AddWellRect(false, 0, 10, 4, 14);
  macro->AddWellRect(true, 0, 14, 4, 20);
  circuit_.AddComponent("multi", "multi_region_master", 0, 0, PLACED);
  std::vector<Component*> components = {circuit_.GetComponentPtr("multi")};
  GriddedCapacityConfig config;
  config.reserved_width = 2;
  GriddedCapacityEstimator estimator(config);

  GriddedCapacityEstimate estimate =
      estimator.Estimate(components, 10, 20, 200);

  EXPECT_EQ(estimate.estimated_row_count, 2);
  EXPECT_EQ(estimate.required_row_height, 20);
  EXPECT_EQ(estimate.required_gridded_area, 160);
}

TEST_F(GriddedCapacityEstimatorTest, FallsBackForUnpairedWellGeometry) {
  Macro* macro = circuit_.AddMacro("fallback_master", 4, 10);
  macro->AddWellRect(true, 0, 4, 4, 10);
  circuit_.AddComponent("fallback", "fallback_master", 0, 0, PLACED);
  std::vector<Component*> components = {circuit_.GetComponentPtr("fallback")};
  GriddedCapacityConfig config;
  GriddedCapacityEstimator estimator(config);

  GriddedCapacityEstimate estimate =
      estimator.Estimate(components, 10, 10, 100);

  EXPECT_EQ(estimate.single_region_fallback_count, 1);
  EXPECT_EQ(estimate.estimated_row_count, 1);
  EXPECT_EQ(estimate.required_row_height, 10);
}

TEST_F(GriddedCapacityEstimatorTest, ReportsCellsWiderThanUsableRow) {
  std::vector<Component*> components = {AddComponent("wide", 9, 4, 6)};
  GriddedCapacityConfig config;
  config.reserved_width = 2;
  GriddedCapacityEstimator estimator(config);

  GriddedCapacityEstimate estimate =
      estimator.Estimate(components, 10, 20, 200);

  EXPECT_EQ(estimate.unplaceable_component_count, 1);
  EXPECT_EQ(estimate.estimated_row_count, 0);
  EXPECT_EQ(estimate.predicted_overflow_area, 90);
  EXPECT_EQ(estimate.required_gridded_area, 90);
}

TEST_F(GriddedCapacityEstimatorTest, IgnoresFixedComponents) {
  std::vector<Component*> components = {AddComponent("movable", 4, 4, 6),
                                        AddComponent("fixed", 4, 4, 6, FIXED)};
  GriddedCapacityConfig config;
  config.reserved_width = 2;
  GriddedCapacityEstimator estimator(config);

  GriddedCapacityEstimate estimate =
      estimator.Estimate(components, 10, 10, 100);

  EXPECT_EQ(estimate.raw_component_area, 40);
  EXPECT_EQ(estimate.estimated_row_count, 1);
}

}  // namespace dali
