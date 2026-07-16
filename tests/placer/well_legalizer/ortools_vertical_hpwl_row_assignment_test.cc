/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 ******************************************************************************/
#include "dali/placer/well_legalizer/ortools_vertical_hpwl_row_assignment.h"

#include <gtest/gtest.h>

#include <unordered_map>

namespace dali {

TEST(OrToolsVerticalHpwlRowAssignmentTest, FindsInterleavedNetAwarePartition) {
  if (!OrToolsVerticalHpwlRowAssignment::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  VerticalHpwlRowAssignmentModel model;
  model.rows = {{0, 3}, {1, 3}};
  for (int component_id = 0; component_id < 6; ++component_id) {
    model.components.push_back(
        {component_id, 1, component_id < 3 ? 0 : 1, {{0, 0.0}, {1, 10.0}}});
    const double anchor_y = component_id % 2 == 0 ? 0.0 : 10.0;
    model.nets.push_back(
        {{{component_id, 0.0, {0.0, 0.0}}, {-1, anchor_y, {}}}, 1.0});
  }

  VerticalHpwlRowAssignmentConfig config;
  config.maximum_time_seconds = 10.0;
  const VerticalHpwlRowAssignmentResult result =
      OrToolsVerticalHpwlRowAssignment().Solve(model, config);

  ASSERT_TRUE(result.HasSolution()) << result.message;
  std::unordered_map<int, int> assigned_rows;
  for (const VerticalHpwlRowLocation& assignment : result.assignments) {
    assigned_rows.emplace(assignment.component_id, assignment.row_id);
  }
  EXPECT_EQ(assigned_rows.at(0), 0);
  EXPECT_EQ(assigned_rows.at(2), 0);
  EXPECT_EQ(assigned_rows.at(4), 0);
  EXPECT_EQ(assigned_rows.at(1), 1);
  EXPECT_EQ(assigned_rows.at(3), 1);
  EXPECT_EQ(assigned_rows.at(5), 1);
}

TEST(OrToolsVerticalHpwlRowAssignmentTest, AccountsForFixedOutsidePins) {
  if (!OrToolsVerticalHpwlRowAssignment::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  VerticalHpwlRowAssignmentModel model;
  model.rows = {{0, 1}, {1, 1}};
  model.components = {{0, 1, 1, {{0, 0.0}, {1, 10.0}}}};
  model.nets = {{{{0, 0.0, {0.0, 0.0}}, {-1, 0.0, {}}}, 1.0}};

  VerticalHpwlRowAssignmentConfig config;
  config.maximum_time_seconds = 10.0;
  const VerticalHpwlRowAssignmentResult result =
      OrToolsVerticalHpwlRowAssignment().Solve(model, config);

  ASSERT_TRUE(result.HasSolution()) << result.message;
  ASSERT_EQ(result.assignments.size(), 1U);
  EXPECT_EQ(result.assignments.front().row_id, 0);
}

}  // namespace dali
