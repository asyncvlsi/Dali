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
#include "dali/placer/well_legalizer/packed_stripe_boundary_planner.h"

#include <gtest/gtest.h>

namespace dali {

TEST(PackedStripeBoundaryPlannerTest, KeepsUniformPackingUniform) {
  AdaptiveStripeBoundaryConfig config;
  config.region_right = 100;
  config.column_count = 4;
  config.minimum_column_pitch = 15;
  config.maximum_column_pitch = 35;
  config.boundary_step = 5;
  PackedStripeBoundaryPlanner planner(config);
  std::vector<StripePackingSample> samples;
  for (int x = 0; x < 100; x += 5) {
    samples.push_back({x + 2.5, 5, 10, 0});
  }

  AdaptiveStripeBoundaryResult result = planner.Plan(samples);

  ASSERT_TRUE(result.feasible);
  EXPECT_EQ(result.boundaries, (std::vector<int>{0, 25, 50, 75, 100}));
}

TEST(PackedStripeBoundaryPlannerTest, AccountsForPartiallyFilledShelves) {
  AdaptiveStripeBoundaryConfig config;
  config.region_right = 100;
  config.column_count = 2;
  config.minimum_column_pitch = 25;
  config.maximum_column_pitch = 75;
  config.boundary_step = 5;
  config.spacing_per_column = 5;
  PackedStripeBoundaryPlanner planner(config);
  std::vector<StripePackingSample> samples = {
      {5.0, 20, 10, 0},  {10.0, 20, 10, 0}, {15.0, 20, 10, 0},
      {70.0, 10, 10, 0}, {90.0, 10, 10, 0}};

  AdaptiveStripeBoundaryResult result = planner.Plan(samples);

  ASSERT_TRUE(result.feasible);
  ASSERT_EQ(result.boundaries.size(), 3U);
  EXPECT_GT(result.boundaries[1], 50);
}

TEST(PackedStripeBoundaryPlannerTest, KeepsIncompatibleSignaturesSeparate) {
  AdaptiveStripeBoundaryConfig config;
  config.region_right = 100;
  config.column_count = 2;
  config.minimum_column_pitch = 25;
  config.maximum_column_pitch = 75;
  config.boundary_step = 5;
  PackedStripeBoundaryPlanner planner(config);
  std::vector<StripePackingSample> samples = {
      {5.0, 20, 10, 0},  {10.0, 20, 10, 0}, {15.0, 20, 20, 1},
      {70.0, 20, 10, 0}, {90.0, 20, 10, 0}};

  AdaptiveStripeBoundaryResult result = planner.Plan(samples);

  ASSERT_TRUE(result.feasible);
  EXPECT_EQ(result.boundaries.front(), 0);
  EXPECT_EQ(result.boundaries.back(), 100);
}

}  // namespace dali
