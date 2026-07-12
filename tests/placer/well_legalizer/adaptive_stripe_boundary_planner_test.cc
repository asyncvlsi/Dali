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
#include "dali/placer/well_legalizer/adaptive_stripe_boundary_planner.h"

#include <gtest/gtest.h>

namespace dali {

TEST(AdaptiveStripeBoundaryPlannerTest, KeepsUniformDemandUniform) {
  AdaptiveStripeBoundaryConfig config;
  config.region_right = 100;
  config.column_count = 4;
  config.minimum_column_pitch = 15;
  config.maximum_column_pitch = 35;
  config.boundary_step = 5;
  AdaptiveStripeBoundaryPlanner planner(config);
  std::vector<StripeDemandSample> samples;
  for (int x = 0; x < 100; ++x) {
    samples.push_back({x + 0.5, 1.0});
  }

  AdaptiveStripeBoundaryResult result = planner.Plan(samples);

  ASSERT_TRUE(result.feasible);
  EXPECT_EQ(result.boundaries, (std::vector<int>{0, 25, 50, 75, 100}));
}

TEST(AdaptiveStripeBoundaryPlannerTest, GivesDenseRegionAdjacentWhitespace) {
  AdaptiveStripeBoundaryConfig config;
  config.region_right = 100;
  config.column_count = 2;
  config.minimum_column_pitch = 25;
  config.maximum_column_pitch = 75;
  config.boundary_step = 5;
  AdaptiveStripeBoundaryPlanner planner(config);
  std::vector<StripeDemandSample> samples = {
      {5.0, 20.0}, {10.0, 20.0}, {15.0, 20.0}, {70.0, 10.0}, {90.0, 10.0}};

  AdaptiveStripeBoundaryResult result = planner.Plan(samples);

  ASSERT_TRUE(result.feasible);
  ASSERT_EQ(result.boundaries.size(), 3U);
  EXPECT_GT(result.boundaries[1], 50);
}

TEST(AdaptiveStripeBoundaryPlannerTest, HonorsPitchAndSpacingConstraints) {
  AdaptiveStripeBoundaryConfig config;
  config.region_left = 10;
  config.region_right = 110;
  config.column_count = 4;
  config.minimum_column_pitch = 20;
  config.maximum_column_pitch = 30;
  config.boundary_step = 3;
  config.spacing_per_column = 2;
  AdaptiveStripeBoundaryPlanner planner(config);

  AdaptiveStripeBoundaryResult result = planner.Plan({{15.0, 100.0}});

  ASSERT_TRUE(result.feasible);
  ASSERT_EQ(result.boundaries.size(), 5U);
  EXPECT_EQ(result.boundaries.front(), 10);
  EXPECT_EQ(result.boundaries.back(), 110);
  for (size_t i = 1; i < result.boundaries.size(); ++i) {
    int pitch = result.boundaries[i] - result.boundaries[i - 1];
    EXPECT_GE(pitch, 20);
    EXPECT_LE(pitch, 30);
  }
}

TEST(AdaptiveStripeBoundaryPlannerTest, ReportsInfeasiblePitchBudget) {
  AdaptiveStripeBoundaryConfig config;
  config.region_right = 100;
  config.column_count = 4;
  config.minimum_column_pitch = 30;
  AdaptiveStripeBoundaryPlanner planner(config);

  AdaptiveStripeBoundaryResult result = planner.Plan({});

  EXPECT_FALSE(result.feasible);
  EXPECT_TRUE(result.boundaries.empty());
}

}  // namespace dali
