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
#include "dali/placer/well_legalizer/stripe_boundary_coordinate_optimizer.h"

#include <gtest/gtest.h>

namespace dali {

TEST(StripeBoundaryCoordinateOptimizerTest, AcceptsExactImprovingMove) {
  StripeBoundaryCoordinateConfig config;
  config.step = 5;
  config.minimum_pitch = 20;
  config.maximum_pitch = 80;
  StripeBoundaryCoordinateOptimizer optimizer(config);

  StripeBoundaryCoordinateResult result = optimizer.Optimize(
      {0, 50, 100}, [](const std::vector<int>& boundaries) {
        double error = boundaries[1] - 40.0;
        return StripeBoundaryEvaluation{true, error * error};
      });

  ASSERT_TRUE(result.feasible);
  EXPECT_EQ(result.boundaries, (std::vector<int>{0, 45, 100}));
  EXPECT_EQ(result.accepted_moves, 1);
  EXPECT_EQ(result.evaluated_candidates, 3);
  EXPECT_DOUBLE_EQ(result.initial_cost, 100.0);
  EXPECT_DOUBLE_EQ(result.final_cost, 25.0);
}

TEST(StripeBoundaryCoordinateOptimizerTest, RejectsInfeasibleImprovement) {
  StripeBoundaryCoordinateConfig config;
  config.step = 5;
  config.minimum_pitch = 20;
  StripeBoundaryCoordinateOptimizer optimizer(config);

  StripeBoundaryCoordinateResult result = optimizer.Optimize(
      {0, 50, 100}, [](const std::vector<int>& boundaries) {
        bool feasible = boundaries[1] >= 50;
        return StripeBoundaryEvaluation{feasible,
                                        static_cast<double>(boundaries[1])};
      });

  ASSERT_TRUE(result.feasible);
  EXPECT_EQ(result.boundaries, (std::vector<int>{0, 50, 100}));
  EXPECT_EQ(result.accepted_moves, 0);
}

TEST(StripeBoundaryCoordinateOptimizerTest, EnforcesPitchConstraints) {
  StripeBoundaryCoordinateConfig config;
  config.step = 10;
  config.minimum_pitch = 25;
  config.maximum_pitch = 55;
  StripeBoundaryCoordinateOptimizer optimizer(config);
  int evaluation_count = 0;

  StripeBoundaryCoordinateResult result = optimizer.Optimize(
      {0, 30, 70, 100}, [&evaluation_count](const std::vector<int>&) {
        ++evaluation_count;
        return StripeBoundaryEvaluation{true, 1.0};
      });

  ASSERT_TRUE(result.feasible);
  EXPECT_EQ(evaluation_count, 3);
  EXPECT_EQ(result.evaluated_candidates, 3);
  EXPECT_EQ(result.accepted_moves, 0);
}

}  // namespace dali
