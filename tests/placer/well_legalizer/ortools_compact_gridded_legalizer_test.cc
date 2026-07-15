/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/ortools_compact_gridded_legalizer.h"

#include <gtest/gtest.h>

#include <unordered_map>

namespace dali {

ExactGriddedNet MakeCompactAnchoredNet(int component_id, double fixed_x,
                                       double fixed_y) {
  ExactGriddedNet net;
  net.pins.push_back({component_id, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0});
  net.pins.push_back({-1, 0.0, 0.0, 0.0, 0.0, fixed_x, fixed_y});
  return net;
}

TEST(OrToolsCompactGriddedLegalizerTest,
     FindsOptimalAssignmentOrientationAndOrdering) {
  if (!OrToolsCompactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  ExactGriddedLegalizationModel model;
  model.stripes = {{0, 0, 0, 6, 4, 2, 0, 0, 1, 1, {}}};
  model.cells = {
      {0, 4, 2, 0, 0, {{1, 1, true}}, {0}},
      {1, 4, 2, 0, 2, {{1, 1, true}}, {0}},
  };
  model.nets = {MakeCompactAnchoredNet(0, 1.0, 1.0),
                MakeCompactAnchoredNet(1, 1.0, 3.0)};

  ExactGriddedLegalizationConfig config;
  config.maximum_time_seconds = 10.0;
  ExactGriddedLegalizationResult result =
      OrToolsCompactGriddedLegalizer().Solve(model, config);

  ASSERT_EQ(result.status, ExactGriddedLegalizationStatus::kOptimal)
      << result.message;
  EXPECT_DOUBLE_EQ(result.weighted_hpwl, 0.0);
  EXPECT_GT(result.model_variable_count, 0);
  EXPECT_GT(result.model_constraint_count, 0);
  ASSERT_EQ(result.cells.size(), 2U);
  std::unordered_map<int, ExactGriddedCellPlacement> placements;
  for (const ExactGriddedCellPlacement& placement : result.cells) {
    placements.emplace(placement.component_id, placement);
  }
  EXPECT_EQ(placements.at(0).row_index, 0);
  EXPECT_EQ(placements.at(0).x, 0);
  EXPECT_EQ(placements.at(0).y, 0);
  EXPECT_EQ(placements.at(1).row_index, 1);
  EXPECT_EQ(placements.at(1).x, 0);
  EXPECT_EQ(placements.at(1).y, 2);
  EXPECT_NE(placements.at(0).is_flipped, placements.at(1).is_flipped);
}

TEST(OrToolsCompactGriddedLegalizerTest, ValidatesCompleteLegalHint) {
  if (!OrToolsCompactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  ExactGriddedLegalizationModel model;
  model.stripes = {
      {0, 0, 0, 4, 6, 2, 0, 0, 1, 1, {{true, 0, 2, 2}, {false, 4, 0, 0}}}};
  model.cells = {{0, 2, 2, 0, 1, {{1, 1, true}}, {0}, 0, 0, false}};
  model.nets = {MakeCompactAnchoredNet(0, 1.0, 2.0)};

  ExactGriddedLegalizationConfig config;
  config.maximum_time_seconds = 10.0;
  config.validate_solution_hint = true;
  ExactGriddedLegalizationResult result =
      OrToolsCompactGriddedLegalizer().Solve(model, config);

  EXPECT_TRUE(result.HasSolution()) << result.message;
  EXPECT_EQ(result.hint_validation_status,
            ExactGriddedLegalizationStatus::kOptimal)
      << result.hint_validation_message;
  EXPECT_DOUBLE_EQ(result.hinted_weighted_hpwl, 0.0);
}

TEST(OrToolsCompactGriddedLegalizerTest,
     LegalizesOverlappingCellsWithinAFixedRow) {
  if (!OrToolsCompactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  ExactGriddedLegalizationModel model;
  model.stripes = {{0, 0, 0, 6, 4, 1, 0, 0, 1, 1, {{true, 0, 2, 2}}}};
  model.cells = {
      {0, 3, 2, 0, 1, {{1, 1, true}}, {0}, 0, 0, false},
      {1, 3, 2, 0, 1, {{1, 1, true}}, {0}, 0, 0, false},
  };
  model.nets = {MakeCompactAnchoredNet(0, 1.0, 2.0),
                MakeCompactAnchoredNet(1, 4.0, 2.0)};

  ExactGriddedLegalizationConfig config;
  config.maximum_time_seconds = 10.0;
  config.maximum_row_displacement = 0;
  config.validate_solution_hint = true;
  ExactGriddedLegalizationResult result =
      OrToolsCompactGriddedLegalizer().Solve(model, config);

  ASSERT_EQ(result.status, ExactGriddedLegalizationStatus::kOptimal)
      << result.message;
  EXPECT_EQ(result.hint_validation_status,
            ExactGriddedLegalizationStatus::kInfeasible);
  EXPECT_NE(result.hint_validation_message.find("overlap"), std::string::npos);
  EXPECT_DOUBLE_EQ(result.weighted_hpwl, 0.0);
}

TEST(OrToolsCompactGriddedLegalizerTest,
     AllowsMissingRowHintsInAZeroRadiusModel) {
  if (!OrToolsCompactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  ExactGriddedLegalizationModel model;
  model.stripes = {{0, 0, 0, 3, 4, 2, 0, 0, 1, 1, {}}};
  model.cells = {{0, 3, 2, 0, 1, {{1, 1, true}}, {0}, -1, -1, false}};

  ExactGriddedLegalizationConfig config;
  config.maximum_time_seconds = 10.0;
  config.maximum_row_displacement = 0;
  ExactGriddedLegalizationResult result =
      OrToolsCompactGriddedLegalizer().Solve(model, config);

  ASSERT_EQ(result.status, ExactGriddedLegalizationStatus::kOptimal)
      << result.message;
  EXPECT_EQ(result.row_assignment_choice_count, 2);
}

TEST(OrToolsCompactGriddedLegalizerTest, CanKeepComponentXAtItsHint) {
  if (!OrToolsCompactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  ExactGriddedLegalizationModel model;
  model.stripes = {{0, 0, 0, 6, 2, 1, 0, 0, 1, 1, {{true, 0, 1, 1}}}};
  model.cells = {{0, 2, 2, 0, 1, {{1, 1, true}}, {0}, 0, 0, false}};
  model.nets = {MakeCompactAnchoredNet(0, 5.0, 1.0)};

  ExactGriddedLegalizationConfig config;
  config.maximum_time_seconds = 10.0;
  config.maximum_row_displacement = 0;
  config.fix_row_geometry = true;
  config.fix_cell_x = true;
  config.fix_cell_orientation = true;
  ExactGriddedLegalizationResult result =
      OrToolsCompactGriddedLegalizer().Solve(model, config);

  ASSERT_EQ(result.status, ExactGriddedLegalizationStatus::kOptimal)
      << result.message;
  ASSERT_EQ(result.cells.size(), 1U);
  EXPECT_EQ(result.cells[0].x, 0);
  EXPECT_FALSE(result.cells[0].is_flipped);
  EXPECT_DOUBLE_EQ(result.weighted_hpwl, 4.0);
}

TEST(OrToolsCompactGriddedLegalizerTest,
     CanValidateAHintWithoutUsingItForSearch) {
  if (!OrToolsCompactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  ExactGriddedLegalizationModel model;
  model.stripes = {{0, 0, 0, 4, 4, 1, 0, 0, 1, 1, {{true, 0, 2, 2}}}};
  model.cells = {{0, 2, 2, 0, 1, {{1, 1, true}}, {0}, 0, 0, false}};
  model.nets = {MakeCompactAnchoredNet(0, 1.0, 2.0)};

  ExactGriddedLegalizationConfig config;
  config.maximum_time_seconds = 10.0;
  config.maximum_row_displacement = 0;
  config.use_solution_hint = false;
  config.validate_solution_hint = true;
  ExactGriddedLegalizationResult result =
      OrToolsCompactGriddedLegalizer().Solve(model, config);

  EXPECT_EQ(result.hint_validation_status,
            ExactGriddedLegalizationStatus::kOptimal);
  EXPECT_EQ(result.status, ExactGriddedLegalizationStatus::kOptimal)
      << result.message;
}

TEST(OrToolsCompactGriddedLegalizerTest,
     LimitsTheNumberOfChangedRowAssignments) {
  if (!OrToolsCompactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  ExactGriddedLegalizationModel model;
  model.stripes = {
      {0, 0, 0, 6, 4, 2, 0, 0, 1, 1, {{true, 0, 1, 1}, {true, 2, 1, 1}}}};
  model.cells = {
      {0, 4, 2, 0, 0, {{1, 1, true}}, {0}, 0, 0, false},
      {1, 4, 2, 0, 2, {{1, 1, true}}, {0}, 0, 1, true},
  };
  model.nets = {MakeCompactAnchoredNet(0, 1.0, 3.0),
                MakeCompactAnchoredNet(1, 1.0, 1.0)};

  ExactGriddedLegalizationConfig config;
  config.maximum_time_seconds = 10.0;
  config.maximum_row_displacement = 1;
  config.maximum_row_assignment_changes = 1;
  config.fix_row_geometry = true;
  ExactGriddedLegalizationResult one_change =
      OrToolsCompactGriddedLegalizer().Solve(model, config);

  ASSERT_EQ(one_change.status, ExactGriddedLegalizationStatus::kOptimal)
      << one_change.message;
  EXPECT_DOUBLE_EQ(one_change.weighted_hpwl, 4.0);

  config.maximum_row_assignment_changes = 2;
  ExactGriddedLegalizationResult two_changes =
      OrToolsCompactGriddedLegalizer().Solve(model, config);

  ASSERT_EQ(two_changes.status, ExactGriddedLegalizationStatus::kOptimal)
      << two_changes.message;
  EXPECT_DOUBLE_EQ(two_changes.weighted_hpwl, 0.0);
}

}  // namespace dali
