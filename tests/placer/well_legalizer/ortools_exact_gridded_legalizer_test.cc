/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/ortools_exact_gridded_legalizer.h"

#include <gtest/gtest.h>

#include <unordered_map>

namespace dali {

ExactGriddedNet MakeAnchoredNet(int component_id, double fixed_x,
                                double fixed_y) {
  ExactGriddedNet net;
  net.pins.push_back({component_id, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0});
  net.pins.push_back({-1, 0.0, 0.0, 0.0, 0.0, fixed_x, fixed_y});
  return net;
}

TEST(OrToolsExactGriddedLegalizerTest, NamesSolverStatuses) {
  EXPECT_STREQ(ExactGriddedLegalizationStatusName(
                   ExactGriddedLegalizationStatus::kUnavailable),
               "unavailable");
  EXPECT_STREQ(ExactGriddedLegalizationStatusName(
                   ExactGriddedLegalizationStatus::kInvalidModel),
               "invalid_model");
  EXPECT_STREQ(ExactGriddedLegalizationStatusName(
                   ExactGriddedLegalizationStatus::kUnknown),
               "unknown");
  EXPECT_STREQ(ExactGriddedLegalizationStatusName(
                   ExactGriddedLegalizationStatus::kInfeasible),
               "infeasible");
  EXPECT_STREQ(ExactGriddedLegalizationStatusName(
                   ExactGriddedLegalizationStatus::kFeasible),
               "feasible");
  EXPECT_STREQ(ExactGriddedLegalizationStatusName(
                   ExactGriddedLegalizationStatus::kOptimal),
               "optimal");
}

TEST(OrToolsExactGriddedLegalizerTest,
     FindsOptimalAssignmentOrientationAndOrdering) {
  if (!OrToolsExactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  ExactGriddedLegalizationModel model;
  model.stripes = {{0, 0, 0, 6, 4, 2, 0, 0, 1, 1, {}}};
  model.cells = {
      {0, 4, 0, 0, {{1, 1, true}}, {0}},
      {1, 4, 0, 2, {{1, 1, true}}, {0}},
  };
  model.nets = {MakeAnchoredNet(0, 1.0, 1.0), MakeAnchoredNet(1, 1.0, 3.0)};

  ExactGriddedLegalizationConfig config;
  config.maximum_time_seconds = 10.0;
  ExactGriddedLegalizationResult result =
      OrToolsExactGriddedLegalizer().Solve(model, config);

  ASSERT_EQ(result.status, ExactGriddedLegalizationStatus::kOptimal)
      << result.message;
  EXPECT_DOUBLE_EQ(result.weighted_hpwl, 0.0);
  ASSERT_EQ(result.cells.size(), 2U);
  ASSERT_EQ(result.rows.size(), 2U);
  std::unordered_map<int, ExactGriddedCellPlacement> placements;
  for (const ExactGriddedCellPlacement& placement : result.cells) {
    placements.emplace(placement.component_id, placement);
  }
  EXPECT_EQ(placements.at(0).row_index, 0);
  EXPECT_FALSE(placements.at(0).is_flipped);
  EXPECT_EQ(placements.at(0).x, 0);
  EXPECT_EQ(placements.at(0).y, 0);
  EXPECT_EQ(placements.at(1).row_index, 1);
  EXPECT_TRUE(placements.at(1).is_flipped);
  EXPECT_EQ(placements.at(1).x, 0);
  EXPECT_EQ(placements.at(1).y, 2);
}

TEST(OrToolsExactGriddedLegalizerTest,
     ChoosesRowOrderAndExactWellHeightMaxima) {
  if (!OrToolsExactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  ExactGriddedLegalizationModel model;
  model.stripes = {{0, 0, 0, 6, 5, 1, 0, 0, 0, 0, {}}};
  model.cells = {
      {0, 3, 3, 0, {{1, 2, true}}, {0}},
      {1, 3, 0, 0, {{2, 1, true}}, {0}},
  };
  model.nets = {MakeAnchoredNet(0, 1.0, 2.0), MakeAnchoredNet(1, 4.0, 1.0)};

  ExactGriddedLegalizationConfig config;
  config.maximum_time_seconds = 10.0;
  ExactGriddedLegalizationResult result =
      OrToolsExactGriddedLegalizer().Solve(model, config);

  ASSERT_EQ(result.status, ExactGriddedLegalizationStatus::kOptimal)
      << result.message;
  ASSERT_EQ(result.rows.size(), 1U);
  EXPECT_EQ(result.rows[0].p_well_height, 2);
  EXPECT_EQ(result.rows[0].n_well_height, 2);
  std::unordered_map<int, ExactGriddedCellPlacement> placements;
  for (const ExactGriddedCellPlacement& placement : result.cells) {
    placements.emplace(placement.component_id, placement);
  }
  EXPECT_EQ(placements.at(0).x, 0);
  EXPECT_EQ(placements.at(1).x, 3);
  EXPECT_LT(placements.at(0).x, placements.at(1).x);
  EXPECT_DOUBLE_EQ(result.weighted_hpwl, 0.0);
}

TEST(OrToolsExactGriddedLegalizerTest, DetectsAnInfeasibleCompleteHint) {
  if (!OrToolsExactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  ExactGriddedLegalizationModel model;
  model.stripes = {{0, 0, 0, 6, 5, 1, 0, 0, 0, 0, {}}};
  model.stripes[0].initial_rows = {{true, 0, 2, 2}};
  model.cells = {
      {0, 3, 0, 1, {{1, 2, true}}, {0}},
      {1, 3, 0, 0, {{2, 1, true}}, {0}},
  };
  for (ExactGriddedCell& cell : model.cells) {
    cell.initial_stripe_id = 0;
    cell.initial_start_row = 0;
  }
  model.nets = {MakeAnchoredNet(0, 1.0, 2.0), MakeAnchoredNet(1, 4.0, 1.0)};

  ExactGriddedLegalizationConfig config;
  config.maximum_time_seconds = 10.0;
  config.validate_solution_hint = true;
  ExactGriddedLegalizationResult result =
      OrToolsExactGriddedLegalizer().Solve(model, config);

  EXPECT_TRUE(result.HasSolution()) << result.message;
  EXPECT_EQ(result.hint_validation_status,
            ExactGriddedLegalizationStatus::kInfeasible);
  EXPECT_NE(result.hint_validation_message.find("overlap"), std::string::npos);
}

TEST(OrToolsExactGriddedLegalizerTest, AllowsWhitespaceBetweenLegalRows) {
  if (!OrToolsExactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  ExactGriddedLegalizationModel model;
  model.stripes = {{0, 0, 0, 4, 10, 2, 0, 0, 1, 1, {}}};
  model.stripes[0].initial_rows = {
      {true, 0, 1, 1},
      {true, 8, 1, 1},
  };
  model.cells = {
      {0, 2, 0, 0, {{1, 1, true}}, {0}},
      {1, 2, 2, 8, {{1, 1, false}}, {0}},
  };
  model.cells[0].initial_stripe_id = 0;
  model.cells[0].initial_start_row = 0;
  model.cells[1].initial_stripe_id = 0;
  model.cells[1].initial_start_row = 1;
  model.nets = {MakeAnchoredNet(0, 1.0, 1.0), MakeAnchoredNet(1, 3.0, 9.0)};

  ExactGriddedLegalizationConfig config;
  config.maximum_time_seconds = 10.0;
  config.validate_solution_hint = true;
  ExactGriddedLegalizationResult result =
      OrToolsExactGriddedLegalizer().Solve(model, config);

  ASSERT_EQ(result.status, ExactGriddedLegalizationStatus::kOptimal)
      << result.message;
  EXPECT_DOUBLE_EQ(result.weighted_hpwl, 0.0);
  EXPECT_EQ(result.hint_validation_status,
            ExactGriddedLegalizationStatus::kOptimal);
  EXPECT_DOUBLE_EQ(result.hinted_weighted_hpwl, 0.0);
  ASSERT_EQ(result.rows.size(), 2U);
  EXPECT_EQ(result.rows[0].y, 0);
  EXPECT_EQ(result.rows[1].y, 8);
}

TEST(OrToolsExactGriddedLegalizerTest, ReportsUnavailableBackend) {
  if (OrToolsExactGriddedLegalizer::IsAvailable()) GTEST_SKIP();
  ExactGriddedLegalizationResult result =
      OrToolsExactGriddedLegalizer().Solve({});
  EXPECT_EQ(result.status, ExactGriddedLegalizationStatus::kUnavailable);
  EXPECT_FALSE(result.HasSolution());
}

}  // namespace dali
