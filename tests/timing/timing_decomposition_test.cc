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
/**
 * @file
 * Cell/wire decomposition of relative-timing constraints.
 *
 * Every case here builds real witness steps and runs them through the same
 * classifier the report uses, because the question the decomposition exists to
 * answer -- how much of a deficit is wire that placement could shorten -- is
 * decided entirely by which steps count as which. A test that handed the writer
 * pre-summed totals would agree with itself and prove nothing about that.
 */
#include "dali/timing/timing_snapshot.h"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace dali {
namespace {

/** A cell arc carries no net name; that is the graph's own discriminator. */
TimingPathStep CellStep(const std::string &source, const std::string &target,
                        double delay) {
  return TimingPathStep(source, target, "", delay);
}

TimingPathStep WireStep(const std::string &source, const std::string &target,
                        const std::string &net, double delay) {
  return TimingPathStep(source, target, net, delay);
}

TimingPathSnapshot Path(const std::string &root, const std::string &terminal,
                        std::vector<TimingPathStep> steps) {
  TimingPathSnapshot path;
  path.root_pin = root;
  path.terminal_pin = terminal;
  path.steps = std::move(steps);
  return path;
}

/** A constraint whose reported slack is exactly slow_total - fast_total. */
RelativeTimingConstraintSnapshot Constraint(int id, TimingPathSnapshot fast,
                                            TimingPathSnapshot slow) {
  RelativeTimingConstraintSnapshot constraint;
  constraint.constraint_id = id;
  constraint.fast_path = std::move(fast);
  constraint.slow_path = std::move(slow);
  constraint.slack =
      constraint.slow_path.TotalDelay() - constraint.fast_path.TotalDelay();
  return constraint;
}

TimingSnapshot SnapshotOf(
    std::vector<RelativeTimingConstraintSnapshot> constraints) {
  TimingSnapshot snapshot;
  snapshot.relative_constraint_count = static_cast<int>(constraints.size());
  snapshot.relative_constraints = std::move(constraints);
  return snapshot;
}

std::string TempPath(const std::string &stem) {
  return ::testing::TempDir() + "dali_timing_decomposition_" + stem + ".json";
}

std::string ReadFile(const std::string &path) {
  std::ifstream file(path);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

/** Every valid decomposition must satisfy these, whatever the path shape. */
void ExpectSelfConsistent(const TimingPathDecomposition &path,
                          const TimingPathSnapshot &source) {
  EXPECT_DOUBLE_EQ(path.cell_delay + path.wire_delay, path.total_delay);
  EXPECT_EQ(path.cell_steps + path.wire_steps, path.total_steps);
  EXPECT_EQ(path.total_steps, static_cast<int>(source.steps.size()));
  EXPECT_DOUBLE_EQ(path.total_delay, source.TotalDelay())
      << "the emitted total is not the sum of the original steps";
}

// --- step classification ----------------------------------------------------

TEST(TimingDecompositionTest, CellOnlyPathHasNoWire) {
  const TimingPathSnapshot path =
      Path("a:A", "a:Y", {CellStep("a:A", "a:Y", 12.5), CellStep("a:Y", "b:Y", 7.5)});
  TimingPathDecomposition decomposition;
  std::string error;

  ASSERT_TRUE(DecomposeTimingPath(path, &decomposition, &error)) << error;

  EXPECT_DOUBLE_EQ(decomposition.cell_delay, 20.0);
  EXPECT_DOUBLE_EQ(decomposition.wire_delay, 0.0);
  EXPECT_EQ(decomposition.cell_steps, 2);
  EXPECT_EQ(decomposition.wire_steps, 0);
  ExpectSelfConsistent(decomposition, path);
}

TEST(TimingDecompositionTest, WireOnlyPathHasNoCell) {
  const TimingPathSnapshot path =
      Path("a:Y", "b:A", {WireStep("a:Y", "b:A", "n0", 3.25),
                          WireStep("b:A", "c:A", "n1", 1.75)});
  TimingPathDecomposition decomposition;
  std::string error;

  ASSERT_TRUE(DecomposeTimingPath(path, &decomposition, &error)) << error;

  EXPECT_DOUBLE_EQ(decomposition.wire_delay, 5.0);
  EXPECT_DOUBLE_EQ(decomposition.cell_delay, 0.0);
  EXPECT_EQ(decomposition.wire_steps, 2);
  EXPECT_EQ(decomposition.cell_steps, 0);
  ExpectSelfConsistent(decomposition, path);
}

TEST(TimingDecompositionTest, MixedPathSplitsByEdgeType) {
  const TimingPathSnapshot path =
      Path("t:Y", "d:Y", {WireStep("t:Y", "a:A", "n0", 2.0),
                          CellStep("a:A", "a:Y", 60.0),
                          WireStep("a:Y", "b:A", "n1", 0.5),
                          CellStep("b:A", "b:Y", 40.0)});
  TimingPathDecomposition decomposition;
  std::string error;

  ASSERT_TRUE(DecomposeTimingPath(path, &decomposition, &error)) << error;

  EXPECT_DOUBLE_EQ(decomposition.cell_delay, 100.0);
  EXPECT_DOUBLE_EQ(decomposition.wire_delay, 2.5);
  EXPECT_EQ(decomposition.cell_steps, 2);
  EXPECT_EQ(decomposition.wire_steps, 2);
  ExpectSelfConsistent(decomposition, path);
}

TEST(TimingDecompositionTest, RepeatedStepIsCountedEachTime) {
  // A fanout reconvergence can legitimately traverse one component twice.
  const TimingPathSnapshot path =
      Path("a:A", "a:Y", {CellStep("a:A", "a:Y", 5.0),
                          WireStep("a:Y", "a:A", "loop", 1.0),
                          CellStep("a:A", "a:Y", 5.0)});
  TimingPathDecomposition decomposition;
  std::string error;

  ASSERT_TRUE(DecomposeTimingPath(path, &decomposition, &error)) << error;

  EXPECT_EQ(decomposition.cell_steps, 2);
  EXPECT_EQ(decomposition.wire_steps, 1);
  EXPECT_DOUBLE_EQ(decomposition.cell_delay, 10.0);
  ExpectSelfConsistent(decomposition, path);
}

TEST(TimingDecompositionTest, ZeroDelayWireStaysWire) {
  const TimingPathSnapshot path =
      Path("a:Y", "b:A", {WireStep("a:Y", "b:A", "n0", 0.0),
                          CellStep("b:A", "b:Y", 9.0)});
  TimingPathDecomposition decomposition;
  std::string error;

  ASSERT_TRUE(DecomposeTimingPath(path, &decomposition, &error)) << error;

  EXPECT_EQ(decomposition.wire_steps, 1)
      << "a zero-delay net leg is still a net leg";
  EXPECT_DOUBLE_EQ(decomposition.wire_delay, 0.0);
  EXPECT_EQ(decomposition.cell_steps, 1);
  ExpectSelfConsistent(decomposition, path);
}

TEST(TimingDecompositionTest, NonFiniteStepIsRejected) {
  for (const double bad : {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity()}) {
    const TimingPathSnapshot path =
        Path("a:A", "a:Y", {CellStep("a:A", "a:Y", bad)});
    TimingPathDecomposition decomposition;
    std::string error;

    EXPECT_FALSE(DecomposeTimingPath(path, &decomposition, &error));
    EXPECT_NE(error.find("non-finite"), std::string::npos) << error;
  }
}

TEST(TimingDecompositionTest, NegativeStepIsRejected) {
  const TimingPathSnapshot path =
      Path("a:A", "a:Y", {CellStep("a:A", "a:Y", -1.0)});
  TimingPathDecomposition decomposition;
  std::string error;

  EXPECT_FALSE(DecomposeTimingPath(path, &decomposition, &error));
  EXPECT_NE(error.find("negative"), std::string::npos) << error;
}

// --- constraint-level decomposition -----------------------------------------

TEST(TimingDecompositionTest, ResidualUsesTheTimersReportedSlack) {
  RelativeTimingConstraintSnapshot constraint = Constraint(
      3, Path("r", "f", {CellStep("a:A", "a:Y", 100.0)}),
      Path("r", "s", {CellStep("b:A", "b:Y", 40.0)}));
  // The timer's own slack, deliberately a hair off the difference.
  constraint.slack = -60.0004;
  TimingConstraintDecomposition decomposition;
  std::string error;

  ASSERT_TRUE(DecomposeTimingConstraint(constraint, &decomposition, &error))
      << error;

  EXPECT_DOUBLE_EQ(decomposition.slack, -60.0004)
      << "the reported slack must be carried, not recomputed";
  EXPECT_NEAR(decomposition.reconciliation_residual, 0.0004, 1e-9);
}

TEST(TimingDecompositionTest, UnresolvedEndpointStillDecomposesNumerically) {
  RelativeTimingConstraintSnapshot constraint =
      Constraint(0, Path("", "", {CellStep("a:A", "a:Y", 10.0)}),
                 Path("", "", {WireStep("b:Y", "c:A", "n", 4.0)}));
  TimingConstraintDecomposition decomposition;
  std::string error;

  ASSERT_TRUE(DecomposeTimingConstraint(constraint, &decomposition, &error))
      << error;

  EXPECT_TRUE(decomposition.semantic_identity.empty())
      << "an unresolved endpoint has no stable identity";
  EXPECT_DOUBLE_EQ(decomposition.fast.cell_delay, 10.0);
  EXPECT_DOUBLE_EQ(decomposition.slow.wire_delay, 4.0);
}

TEST(TimingDecompositionTest, AttributionStatesAreNotCollapsed) {
  const TimingPathSnapshot fast = Path("r:Y", "f:Y", {CellStep("a:A", "a:Y", 8.0)});
  const TimingPathSnapshot slow = Path("r:Y", "s:Y", {CellStep("b:A", "b:Y", 5.0)});

  RelativeTimingConstraintSnapshot none = Constraint(0, fast, slow);
  RelativeTimingConstraintSnapshot one = Constraint(1, fast, slow);
  one.delay_repair_candidate_sites = {"dl2"};
  RelativeTimingConstraintSnapshot many = Constraint(2, fast, slow);
  many.delay_repair_candidate_sites = {"dl2", "dl3"};

  TimingConstraintDecomposition decomposition;
  std::string error;

  ASSERT_TRUE(DecomposeTimingConstraint(none, &decomposition, &error));
  EXPECT_EQ(decomposition.attribution, ConstraintAttributionState::kUnattributed);
  EXPECT_TRUE(decomposition.candidate_sites.empty());
  EXPECT_TRUE(decomposition.unique_site.empty());

  ASSERT_TRUE(DecomposeTimingConstraint(one, &decomposition, &error));
  EXPECT_EQ(decomposition.attribution, ConstraintAttributionState::kUnique);
  EXPECT_EQ(decomposition.unique_site, "dl2");

  ASSERT_TRUE(DecomposeTimingConstraint(many, &decomposition, &error));
  EXPECT_EQ(decomposition.attribution, ConstraintAttributionState::kAmbiguous);
  EXPECT_EQ(decomposition.candidate_sites, (std::vector<std::string>{"dl2", "dl3"}))
      << "every candidate must survive; the report must not pick one";
  EXPECT_TRUE(decomposition.unique_site.empty());
}

TEST(TimingDecompositionTest, RenumberingPreservesSemanticIdentity) {
  const TimingPathSnapshot fast = Path("root:Y", "fast:Y", {CellStep("a:A", "a:Y", 3.0)});
  const TimingPathSnapshot slow = Path("root:Y", "slow:Y", {CellStep("b:A", "b:Y", 9.0)});
  TimingConstraintDecomposition first, renumbered;
  std::string error;

  ASSERT_TRUE(DecomposeTimingConstraint(Constraint(7, fast, slow), &first, &error));
  ASSERT_TRUE(
      DecomposeTimingConstraint(Constraint(401, fast, slow), &renumbered, &error));

  EXPECT_NE(first.constraint_id, renumbered.constraint_id);
  EXPECT_EQ(first.semantic_identity, renumbered.semantic_identity);
  EXPECT_FALSE(first.semantic_identity.empty());
}

// --- the writer -------------------------------------------------------------

TEST(TimingDecompositionWriterTest, RoundTripsAndEscapes) {
  RelativeTimingConstraintSnapshot constraint = Constraint(
      0, Path("r\"oot:Y", "fa\\st:Y", {WireStep("a:Y", "b:A", "n\"et", 2.0),
                                       CellStep("b:A", "b:Y", 30.0)}),
      Path("r\"oot:Y", "slo\tw:Y", {CellStep("c:A", "c:Y", 45.0)}));
  constraint.delay_repair_candidate_sites = {"dl0", "dl1"};
  const std::string path = TempPath("roundtrip");

  ASSERT_TRUE(WriteTimingDecompositionJson(SnapshotOf({constraint}), path));
  const std::string text = ReadFile(path);
  std::remove(path.c_str());

  EXPECT_NE(text.find("\"schema_version\": 1"), std::string::npos);
  EXPECT_NE(text.find("\"constraint_count\": 1"), std::string::npos);
  EXPECT_NE(text.find("\"reconciliation_tolerance_ps\""), std::string::npos);
  EXPECT_NE(text.find("\"identity_digest\""), std::string::npos);
  EXPECT_NE(text.find("\"attribution\": \"ambiguous\""), std::string::npos);
  EXPECT_NE(text.find("[\"dl0\", \"dl1\"]"), std::string::npos);
  EXPECT_NE(text.find("\"cell_delay_ps\": 30"), std::string::npos);
  EXPECT_NE(text.find("\"wire_delay_ps\": 2"), std::string::npos);
  // Escaped, and never emitted raw.
  EXPECT_NE(text.find("\\\""), std::string::npos);
  EXPECT_EQ(text.find("slo\tw"), std::string::npos);
  EXPECT_EQ(text.find("nan"), std::string::npos);
  EXPECT_EQ(text.find("inf"), std::string::npos);
}

TEST(TimingDecompositionWriterTest, ReconciliationFailureLeavesNoFile) {
  RelativeTimingConstraintSnapshot constraint =
      Constraint(0, Path("r:Y", "f:Y", {CellStep("a:A", "a:Y", 100.0)}),
                 Path("r:Y", "s:Y", {CellStep("b:A", "b:Y", 40.0)}));
  constraint.slack = -60.0 + 2e-3;  // twice the tolerance away
  const std::string path = TempPath("residual");

  EXPECT_FALSE(WriteTimingDecompositionJson(SnapshotOf({constraint}), path));

  std::ifstream leftover(path);
  EXPECT_FALSE(leftover.good())
      << "a rejected report must not leave a destination behind";
  std::remove(path.c_str());
}

TEST(TimingDecompositionWriterTest, ResidualWithinToleranceIsAccepted) {
  RelativeTimingConstraintSnapshot constraint =
      Constraint(0, Path("r:Y", "f:Y", {CellStep("a:A", "a:Y", 100.0)}),
                 Path("r:Y", "s:Y", {CellStep("b:A", "b:Y", 40.0)}));
  constraint.slack = -60.0 + 4.6e-4;  // Gate 0's measured worst residual
  const std::string path = TempPath("accepted");

  EXPECT_TRUE(WriteTimingDecompositionJson(SnapshotOf({constraint}), path));
  std::remove(path.c_str());
}

TEST(TimingDecompositionWriterTest, DuplicateSemanticIdentityIsRejected) {
  const TimingPathSnapshot fast = Path("r:Y", "f:Y", {CellStep("a:A", "a:Y", 1.0)});
  const TimingPathSnapshot slow = Path("r:Y", "s:Y", {CellStep("b:A", "b:Y", 2.0)});
  const std::string path = TempPath("duplicate");

  EXPECT_FALSE(WriteTimingDecompositionJson(
      SnapshotOf({Constraint(0, fast, slow), Constraint(1, fast, slow)}), path));
  std::remove(path.c_str());
}

TEST(TimingDecompositionWriterTest, NonFiniteStepIsRejectedByTheWriter) {
  const std::string path = TempPath("nonfinite");
  const RelativeTimingConstraintSnapshot constraint = Constraint(
      0, Path("r:Y", "f:Y",
              {CellStep("a:A", "a:Y", std::numeric_limits<double>::infinity())}),
      Path("r:Y", "s:Y", {CellStep("b:A", "b:Y", 2.0)}));

  EXPECT_FALSE(WriteTimingDecompositionJson(SnapshotOf({constraint}), path));
  std::ifstream leftover(path);
  EXPECT_FALSE(leftover.good());
  std::remove(path.c_str());
}

TEST(TimingDecompositionWriterTest, UnwritableDestinationIsReported) {
  const RelativeTimingConstraintSnapshot constraint =
      Constraint(0, Path("r:Y", "f:Y", {CellStep("a:A", "a:Y", 1.0)}),
                 Path("r:Y", "s:Y", {CellStep("b:A", "b:Y", 2.0)}));

  EXPECT_FALSE(WriteTimingDecompositionJson(
      SnapshotOf({constraint}), "/nonexistent-directory/decomposition.json"));
}

TEST(TimingDecompositionWriterTest, CountMismatchIsRejected) {
  TimingSnapshot snapshot =
      SnapshotOf({Constraint(0, Path("r:Y", "f:Y", {CellStep("a:A", "a:Y", 1.0)}),
                             Path("r:Y", "s:Y", {CellStep("b:A", "b:Y", 2.0)}))});
  snapshot.relative_constraint_count = 2;
  const std::string path = TempPath("count");

  EXPECT_FALSE(WriteTimingDecompositionJson(snapshot, path));
  std::remove(path.c_str());
}

} // namespace
} // namespace dali
