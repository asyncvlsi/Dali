/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/ortools_fixed_row_displacement_optimizer.h"

#include <gtest/gtest.h>

#include <unordered_map>

namespace dali {

TEST(OrToolsFixedRowDisplacementOptimizerTest,
     CouplesOneComponentAcrossMultipleRows) {
  if (!OrToolsFixedRowDisplacementOptimizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  FixedRowDisplacementModel model;
  model.components = {
      {0, 4, 8, 0, 16},
      {1, 4, 8, 0, 16},
      {2, 4, 12, 0, 16},
  };
  model.rows = {
      {{0, 1}, 0},
      {{2, 1}, 0},
  };

  FixedRowDisplacementResult result =
      OrToolsFixedRowDisplacementOptimizer().Solve(model);

  ASSERT_TRUE(result.HasSolution()) << result.message;
  std::unordered_map<int, int> locations;
  for (const FixedRowComponentLocation& location : result.locations) {
    locations.emplace(location.component_id, location.x);
  }
  EXPECT_GE(locations.at(1), locations.at(0) + 4);
  EXPECT_GE(locations.at(1), locations.at(2) + 4);
  EXPECT_EQ(result.total_displacement, 8);
}

TEST(OrToolsFixedRowDisplacementOptimizerTest, HonorsSpacingAndBounds) {
  if (!OrToolsFixedRowDisplacementOptimizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  FixedRowDisplacementModel model;
  model.components = {
      {0, 3, -10, 2, 10},
      {1, 2, 20, 2, 10},
  };
  model.rows = {{{0, 1}, 2}};

  FixedRowDisplacementResult result =
      OrToolsFixedRowDisplacementOptimizer().Solve(model);

  ASSERT_TRUE(result.HasSolution()) << result.message;
  ASSERT_EQ(result.locations.size(), 2U);
  EXPECT_GE(result.locations[0].x, 2);
  EXPECT_LE(result.locations[1].x, 10);
  EXPECT_GE(result.locations[1].x, result.locations[0].x + 5);
}

TEST(OrToolsFixedRowDisplacementOptimizerTest, ReportsInfeasibleModel) {
  if (!OrToolsFixedRowDisplacementOptimizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  FixedRowDisplacementModel model;
  model.components = {
      {0, 6, 0, 0, 4},
      {1, 6, 4, 0, 4},
  };
  model.rows = {{{0, 1}, 0}};

  FixedRowDisplacementResult result =
      OrToolsFixedRowDisplacementOptimizer().Solve(model);

  EXPECT_EQ(result.status, FixedRowDisplacementStatus::kInfeasible);
  EXPECT_FALSE(result.HasSolution());
}

TEST(OrToolsFixedRowDisplacementOptimizerTest, RejectsUnknownComponent) {
  FixedRowDisplacementModel model;
  model.components = {{0, 2, 0, 0, 10}};
  model.rows = {{{0, 1}, 0}};

  FixedRowDisplacementResult result =
      OrToolsFixedRowDisplacementOptimizer().Solve(model);

  if (OrToolsFixedRowDisplacementOptimizer::IsAvailable()) {
    EXPECT_EQ(result.status, FixedRowDisplacementStatus::kInvalidModel);
  } else {
    EXPECT_EQ(result.status, FixedRowDisplacementStatus::kUnavailable);
  }
  EXPECT_FALSE(result.HasSolution());
}

}  // namespace dali
