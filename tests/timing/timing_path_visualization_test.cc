/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/
/*
 * What the viewer is allowed to draw.
 *
 * These pin the classification rather than the picture: which cells are on both
 * paths, which on one, which edges are shared, and what happens to the cases a
 * diagnostic view is most likely to lose -- a constraint no site owns, one that
 * two sites claim, and a witness that resolves to nothing at all.
 */
#include "dali/timing/timing_path_visualization.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <string>

namespace dali {
namespace {

/** Component names map to ids by a fixed table; anything else is not a cell. */
ComponentIdResolver TableResolver(const std::map<std::string, int> &table) {
  return [table](const std::string &name) {
    auto found = table.find(name);
    return found == table.end() ? -1 : found->second;
  };
}

TimingPathStep Step(const std::string &from, const std::string &to,
                    const std::string &net, double delay) {
  return TimingPathStep(from, to, net, delay);
}

/**
 * A constraint whose paths share their first hop and then diverge.
 *
 *   root u0 --n0--> u1 --n1--> u2   (slow)
 *   root u0 --n0--> u1 --n2--> u3   (fast)
 *
 * u0 and u1 are common, u2 is slow-only, u3 is fast-only, and n0 is the one
 * shared edge.
 */
RelativeTimingConstraintSnapshot ForkedConstraint(int id, double slack) {
  RelativeTimingConstraintSnapshot constraint;
  constraint.constraint_id = id;
  constraint.slack = slack;
  constraint.slow_path.root_pin = "u0:Y";
  constraint.slow_path.terminal_pin = "u2:A";
  constraint.slow_path.steps = {Step("u0:Y", "u1:A", "n0", 10.0),
                                Step("u1:Y", "u2:A", "n1", 25.0)};
  constraint.fast_path.root_pin = "u0:Y";
  constraint.fast_path.terminal_pin = "u3:A";
  constraint.fast_path.steps = {Step("u0:Y", "u1:A", "n0", 10.0),
                                Step("u1:Y", "u3:A", "n2", 5.0)};
  return constraint;
}

const std::map<std::string, int> kComponents = {
    {"u0", 10}, {"u1", 11}, {"u2", 12}, {"u3", 13}};

TEST(TimingPathVisualizationTest, GroupsConstraintsUnderTheOwningDelayLine) {
  TimingSnapshot snapshot;
  RelativeTimingConstraintSnapshot owned = ForkedConstraint(7, -50.0);
  owned.delay_repair_candidate_nets = {"dl2_ainv_1_6:Y"};
  RelativeTimingConstraintSnapshot other = ForkedConstraint(8, -10.0);
  other.delay_repair_candidate_nets = {"dl3_ainv_1_6:Y"};
  snapshot.relative_constraints = {owned, other};

  const TimingVisualizationResult result = BuildTimingPathVisualization(
      snapshot, {"dl2", "dl3"}, TableResolver(kComponents));

  ASSERT_EQ(result.delay_lines.size(), 2U);
  EXPECT_EQ(result.delay_lines[0].delay_line_name, "dl2");
  ASSERT_EQ(result.delay_lines[0].constraints.size(), 1U);
  EXPECT_EQ(result.delay_lines[0].constraints[0].constraint_id, 7);
  EXPECT_EQ(result.delay_lines[0].constraints[0].attributed_delay_line, "dl2");
  EXPECT_EQ(result.delay_lines[1].delay_line_name, "dl3");
  ASSERT_EQ(result.delay_lines[1].constraints.size(), 1U);
  EXPECT_EQ(result.delay_lines[1].constraints[0].constraint_id, 8);
  EXPECT_TRUE(result.unattributed.empty());
  EXPECT_TRUE(result.ambiguous.empty());
}

// A registered line with nothing attributed is listed empty rather than
// omitted: a line that stopped being measured must be visible as such.
TEST(TimingPathVisualizationTest, ARegisteredLineWithNoConstraintsIsStillListed) {
  TimingSnapshot snapshot;
  const TimingVisualizationResult result = BuildTimingPathVisualization(
      snapshot, {"dl0", "dl1"}, TableResolver(kComponents));

  ASSERT_EQ(result.delay_lines.size(), 2U);
  EXPECT_TRUE(result.delay_lines[0].constraints.empty());
  EXPECT_EQ(result.delay_lines[0].worst_constraint_id, -1);
}

TEST(TimingPathVisualizationTest, WorstConstraintIsTheMinimumSlackOne) {
  TimingSnapshot snapshot;
  for (const auto &pair : std::vector<std::pair<int, double>>{
           {5, 109.3}, {83, 80.7}, {114, 106.3}}) {
    RelativeTimingConstraintSnapshot constraint =
        ForkedConstraint(pair.first, pair.second);
    constraint.delay_repair_candidate_nets = {"dl2_ainv_1_6:Y"};
    snapshot.relative_constraints.push_back(constraint);
  }

  const TimingVisualizationResult result = BuildTimingPathVisualization(
      snapshot, {"dl2"}, TableResolver(kComponents));

  ASSERT_EQ(result.delay_lines.size(), 1U);
  const PlacementDelayLineTimingVisualization &line = result.delay_lines[0];
  EXPECT_EQ(line.worst_constraint_id, 83);
  EXPECT_DOUBLE_EQ(line.worst_slack_ps, 80.7);
  // Sorted worst first, which is the order the tree shows them in.
  ASSERT_EQ(line.constraints.size(), 3U);
  EXPECT_EQ(line.constraints[0].constraint_id, 83);
  EXPECT_EQ(line.constraints[1].constraint_id, 114);
  EXPECT_EQ(line.constraints[2].constraint_id, 5);
}

// Equal slacks must not make the worst constraint depend on capture order.
TEST(TimingPathVisualizationTest, EqualSlacksBreakTiesByConstraintId) {
  TimingSnapshot snapshot;
  for (int id : {40, 12, 31}) {
    RelativeTimingConstraintSnapshot constraint = ForkedConstraint(id, -4.0);
    constraint.delay_repair_candidate_nets = {"dl0_ainv_1_6:Y"};
    snapshot.relative_constraints.push_back(constraint);
  }
  const TimingVisualizationResult result = BuildTimingPathVisualization(
      snapshot, {"dl0"}, TableResolver(kComponents));
  EXPECT_EQ(result.delay_lines[0].worst_constraint_id, 12);
}

// The numeric id is a diagnostic handle; the semantic identity is the key that
// survives a renumbering, and the two are carried separately on purpose.
TEST(TimingPathVisualizationTest, SemanticIdentityIsCarriedBesideTheNumericId) {
  TimingSnapshot snapshot;
  RelativeTimingConstraintSnapshot constraint = ForkedConstraint(83, -1.0);
  constraint.delay_repair_candidate_nets = {"dl2_ainv_1_6:Y"};
  snapshot.relative_constraints = {constraint};

  const TimingVisualizationResult result = BuildTimingPathVisualization(
      snapshot, {"dl2"}, TableResolver(kComponents));
  const PlacementTimingPathVisualization &view =
      result.delay_lines[0].constraints[0];

  EXPECT_EQ(view.constraint_id, 83);
  EXPECT_EQ(view.semantic_identity, constraint.SemanticIdentity());
  EXPECT_FALSE(view.semantic_identity.empty());
  // Renumbering the constraint leaves the identity untouched.
  RelativeTimingConstraintSnapshot renumbered = constraint;
  renumbered.constraint_id = 500;
  EXPECT_EQ(renumbered.SemanticIdentity(), view.semantic_identity);
}

TEST(TimingPathVisualizationTest, ClassifiesComponentsAsFastSlowAndCommon) {
  TimingSnapshot snapshot;
  RelativeTimingConstraintSnapshot constraint = ForkedConstraint(1, -1.0);
  constraint.delay_repair_candidate_nets = {"dl0_ainv_1_6:Y"};
  snapshot.relative_constraints = {constraint};

  const TimingVisualizationResult result = BuildTimingPathVisualization(
      snapshot, {"dl0"}, TableResolver(kComponents));
  ASSERT_FALSE(result.delay_lines[0].constraints.empty());
  const PlacementTimingPathVisualization &view =
      result.delay_lines[0].constraints[0];

  EXPECT_EQ(view.common_component_ids, (std::vector<int>{10, 11}));
  EXPECT_EQ(view.fast_only_component_ids, (std::vector<int>{13}));
  EXPECT_EQ(view.slow_only_component_ids, (std::vector<int>{12}));
  EXPECT_TRUE(view.has_geometry);
}

TEST(TimingPathVisualizationTest, ClassifiesEdgesAsFastSlowAndCommon) {
  TimingSnapshot snapshot;
  RelativeTimingConstraintSnapshot constraint = ForkedConstraint(1, -1.0);
  constraint.delay_repair_candidate_nets = {"dl0_ainv_1_6:Y"};
  snapshot.relative_constraints = {constraint};

  const TimingVisualizationResult result = BuildTimingPathVisualization(
      snapshot, {"dl0"}, TableResolver(kComponents));
  ASSERT_FALSE(result.delay_lines[0].constraints.empty());
  const PlacementTimingPathVisualization &view =
      result.delay_lines[0].constraints[0];

  ASSERT_EQ(view.common_edges.size(), 1U);
  EXPECT_EQ(view.common_edges[0].net_name, "n0");
  ASSERT_EQ(view.slow_only_edges.size(), 1U);
  EXPECT_EQ(view.slow_only_edges[0].net_name, "n1");
  ASSERT_EQ(view.fast_only_edges.size(), 1U);
  EXPECT_EQ(view.fast_only_edges[0].net_name, "n2");
}

// A repeated step describes the same connection twice; drawing it twice would
// double a line without saying anything new.
TEST(TimingPathVisualizationTest, RepeatedStepsCollapseToOneEdge) {
  TimingSnapshot snapshot;
  RelativeTimingConstraintSnapshot constraint = ForkedConstraint(1, -1.0);
  constraint.slow_path.steps.push_back(Step("u0:Y", "u1:A", "n0", 10.0));
  constraint.delay_repair_candidate_nets = {"dl0_ainv_1_6:Y"};
  snapshot.relative_constraints = {constraint};

  const TimingVisualizationResult result = BuildTimingPathVisualization(
      snapshot, {"dl0"}, TableResolver(kComponents));
  ASSERT_FALSE(result.delay_lines[0].constraints.empty());
  const PlacementTimingPathVisualization &view =
      result.delay_lines[0].constraints[0];

  EXPECT_EQ(view.common_edges.size(), 1U);
  EXPECT_EQ(std::count(view.common_component_ids.begin(),
                       view.common_component_ids.end(), 10),
            1);
}

TEST(TimingPathVisualizationTest, PathTotalsMatchTheWitnessTotals) {
  TimingSnapshot snapshot;
  RelativeTimingConstraintSnapshot constraint = ForkedConstraint(1, -1.0);
  constraint.delay_repair_candidate_nets = {"dl0_ainv_1_6:Y"};
  snapshot.relative_constraints = {constraint};

  const TimingVisualizationResult result = BuildTimingPathVisualization(
      snapshot, {"dl0"}, TableResolver(kComponents));
  ASSERT_FALSE(result.delay_lines[0].constraints.empty());
  const PlacementTimingPathVisualization &view =
      result.delay_lines[0].constraints[0];

  EXPECT_DOUBLE_EQ(view.slow_delay_ps, constraint.slow_path.TotalDelay());
  EXPECT_DOUBLE_EQ(view.fast_delay_ps, constraint.fast_path.TotalDelay());
  EXPECT_DOUBLE_EQ(view.slow_delay_ps, 35.0);
  EXPECT_DOUBLE_EQ(view.fast_delay_ps, 15.0);
}

TEST(TimingPathVisualizationTest, UnattributedConstraintsRemainObservable) {
  TimingSnapshot snapshot;
  RelativeTimingConstraintSnapshot orphan = ForkedConstraint(21, -3.0);
  orphan.delay_repair_candidate_nets = {"some_other_net:Y"};
  snapshot.relative_constraints = {orphan};

  const TimingVisualizationResult result = BuildTimingPathVisualization(
      snapshot, {"dl0", "dl1"}, TableResolver(kComponents));

  EXPECT_TRUE(result.delay_lines[0].constraints.empty());
  ASSERT_EQ(result.unattributed.size(), 1U);
  EXPECT_EQ(result.unattributed[0].constraint_id, 21);
  EXPECT_TRUE(result.unattributed[0].attributed_delay_line.empty());
  EXPECT_FALSE(result.unattributed[0].ambiguous_attribution);
}

TEST(TimingPathVisualizationTest, AmbiguousAttributionRemainsObservable) {
  TimingSnapshot snapshot;
  RelativeTimingConstraintSnapshot shared = ForkedConstraint(22, -6.0);
  shared.delay_repair_candidate_nets = {"dl0_ainv_1_6:Y", "dl1_ainv_2_6:Y"};
  snapshot.relative_constraints = {shared};

  const TimingVisualizationResult result = BuildTimingPathVisualization(
      snapshot, {"dl0", "dl1"}, TableResolver(kComponents));

  EXPECT_TRUE(result.delay_lines[0].constraints.empty());
  EXPECT_TRUE(result.delay_lines[1].constraints.empty());
  ASSERT_EQ(result.ambiguous.size(), 1U);
  EXPECT_TRUE(result.ambiguous[0].ambiguous_attribution);
  EXPECT_EQ(result.ambiguous[0].candidate_delay_lines,
            (std::vector<std::string>{"dl0", "dl1"}));
  EXPECT_TRUE(result.ambiguous[0].attributed_delay_line.empty());
}

// An I/O endpoint, or a pin inside a replaceable site, resolves to no
// component. That is a fact about the design, and the answer is to say there is
// no geometry rather than to invent a coordinate.
TEST(TimingPathVisualizationTest, UnresolvedPinsProduceNoGeometryAndNoCrash) {
  TimingSnapshot snapshot;
  RelativeTimingConstraintSnapshot constraint;
  constraint.constraint_id = 3;
  constraint.slack = -2.0;
  constraint.slow_path.root_pin = "delay-site:dl0:Y";
  constraint.slow_path.terminal_pin = "din[3]";
  constraint.slow_path.steps = {Step("delay-site:dl0:Y", "din[3]", "n9", 4.0)};
  constraint.delay_repair_candidate_nets = {"dl0_ainv_1_6:Y"};
  snapshot.relative_constraints = {constraint};

  const TimingVisualizationResult result = BuildTimingPathVisualization(
      snapshot, {"dl0"}, TableResolver(kComponents));
  ASSERT_FALSE(result.delay_lines[0].constraints.empty());
  const PlacementTimingPathVisualization &view =
      result.delay_lines[0].constraints[0];

  EXPECT_FALSE(view.has_geometry);
  EXPECT_TRUE(view.fast_only_component_ids.empty());
  EXPECT_TRUE(view.slow_only_component_ids.empty());
  EXPECT_TRUE(view.common_component_ids.empty());
  EXPECT_TRUE(view.slow_only_edges.empty());
  // The measured totals survive even when nothing can be drawn.
  EXPECT_DOUBLE_EQ(view.slow_delay_ps, 4.0);
}

TEST(TimingPathVisualizationTest, EmptyWitnessesAreSafe) {
  TimingSnapshot snapshot;
  RelativeTimingConstraintSnapshot constraint;
  constraint.constraint_id = 4;
  constraint.slack = 1.0;
  constraint.delay_repair_candidate_nets = {"dl0_ainv_1_6:Y"};
  snapshot.relative_constraints = {constraint};

  const TimingVisualizationResult result = BuildTimingPathVisualization(
      snapshot, {"dl0"}, TableResolver(kComponents));
  ASSERT_FALSE(result.delay_lines[0].constraints.empty());
  const PlacementTimingPathVisualization &view =
      result.delay_lines[0].constraints[0];

  EXPECT_FALSE(view.has_geometry);
  EXPECT_DOUBLE_EQ(view.fast_delay_ps, 0.0);
  EXPECT_DOUBLE_EQ(view.slow_delay_ps, 0.0);
  EXPECT_EQ(view.root_component_id, -1);
}

TEST(TimingPathVisualizationTest, ComponentNameIsSplitAtTheLastColon) {
  EXPECT_EQ(ComponentNameFromPin("u12:Y"), "u12");
  EXPECT_EQ(ComponentNameFromPin("delay-site:dl0:Y"), "delay-site:dl0");
  EXPECT_EQ(ComponentNameFromPin("nocolon"), "nocolon");
}

} // namespace
} // namespace dali
