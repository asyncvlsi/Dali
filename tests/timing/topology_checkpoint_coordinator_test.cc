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
 * Reading a checkpoint off the trajectory instead of encoding one.
 *
 * The previous milestone put the checkpoint at iteration 12 because a profile
 * had once measured the placement settling there. That is an answer, not a
 * criterion: it would have stayed 12 on a design that settled at 30, or never.
 * These tests pin the criterion, and in particular the cases where a run looks
 * settled without being settled.
 */
#include "dali/timing/topology_checkpoint_coordinator.h"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace dali {
namespace {

CheckpointEligibilityConfig Config(int warmup = 5) {
  CheckpointEligibilityConfig config;
  config.warmup_iteration = warmup;
  config.stability_fraction = 0.01;
  config.stability_window = 3;
  return config;
}

CheckpointSample Sample(int iteration, double change, double hpwl = 1000.0) {
  return CheckpointSample{iteration, hpwl, change};
}

TEST(TopologyCheckpointCoordinatorTest, ThreeSettledIterationsAreEligible) {
  TopologyCheckpointCoordinator coordinator(Config());
  EXPECT_FALSE(coordinator.Offer(Sample(5, 0.004)));
  EXPECT_FALSE(coordinator.Offer(Sample(6, -0.003)));
  EXPECT_TRUE(coordinator.Offer(Sample(7, 0.002)));

  // The three that satisfied it are reported, oldest first, so the choice can
  // be checked against the log rather than believed.
  ASSERT_EQ(coordinator.QualifyingSamples().size(), 3u);
  EXPECT_EQ(coordinator.QualifyingSamples()[0].iteration, 5);
  EXPECT_EQ(coordinator.QualifyingSamples()[2].iteration, 7);
}

TEST(TopologyCheckpointCoordinatorTest, WarmupIterationsAreNotCounted) {
  TopologyCheckpointCoordinator coordinator(Config(5));
  // Perfectly still, but too early to mean anything.
  EXPECT_FALSE(coordinator.Offer(Sample(2, 0.0)));
  EXPECT_FALSE(coordinator.Offer(Sample(3, 0.0)));
  EXPECT_FALSE(coordinator.Offer(Sample(4, 0.0)));
  EXPECT_TRUE(coordinator.QualifyingSamples().empty());
  // The window starts at warm-up, so three more are still needed.
  EXPECT_FALSE(coordinator.Offer(Sample(5, 0.0)));
  EXPECT_FALSE(coordinator.Offer(Sample(6, 0.0)));
  EXPECT_TRUE(coordinator.Offer(Sample(7, 0.0)));
}

// Consecutive means consecutive: one unsettled iteration restarts the count,
// rather than being skipped over to reach three settled ones eventually.
TEST(TopologyCheckpointCoordinatorTest, AnUnsettledIterationRestartsTheWindow) {
  TopologyCheckpointCoordinator coordinator(Config());
  EXPECT_FALSE(coordinator.Offer(Sample(5, 0.001)));
  EXPECT_FALSE(coordinator.Offer(Sample(6, 0.001)));
  EXPECT_FALSE(coordinator.Offer(Sample(7, 0.05)));  // moved
  EXPECT_TRUE(coordinator.QualifyingSamples().empty());
  EXPECT_FALSE(coordinator.Offer(Sample(8, 0.001)));
  EXPECT_FALSE(coordinator.Offer(Sample(9, 0.001)));
  EXPECT_TRUE(coordinator.Offer(Sample(10, 0.001)));
  EXPECT_EQ(coordinator.QualifyingSamples()[0].iteration, 8);
}

// The threshold is on magnitude: a placement that improved by 5% is no more
// settled than one that worsened by 5%.
TEST(TopologyCheckpointCoordinatorTest, LargeImprovementIsAlsoUnsettled) {
  TopologyCheckpointCoordinator coordinator(Config());
  EXPECT_FALSE(coordinator.Offer(Sample(5, 0.001)));
  EXPECT_FALSE(coordinator.Offer(Sample(6, -0.05)));
  EXPECT_TRUE(coordinator.QualifyingSamples().empty());
}

TEST(TopologyCheckpointCoordinatorTest, ExactlyAtTheThresholdCounts) {
  TopologyCheckpointCoordinator coordinator(Config());
  EXPECT_FALSE(coordinator.Offer(Sample(5, 0.01)));
  EXPECT_FALSE(coordinator.Offer(Sample(6, -0.01)));
  EXPECT_TRUE(coordinator.Offer(Sample(7, 0.01)));
}

// A value that cannot be compared cannot support a claim that anything held
// still, and it breaks the run of consecutive samples around it.
TEST(TopologyCheckpointCoordinatorTest, NonFiniteSamplesBreakTheWindow) {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  TopologyCheckpointCoordinator coordinator(Config());
  EXPECT_FALSE(coordinator.Offer(Sample(5, 0.001)));
  EXPECT_FALSE(coordinator.Offer(Sample(6, 0.001)));
  EXPECT_FALSE(coordinator.Offer(Sample(7, nan)));
  EXPECT_TRUE(coordinator.QualifyingSamples().empty());

  TopologyCheckpointCoordinator second(Config());
  EXPECT_FALSE(second.Offer(Sample(5, 0.001)));
  EXPECT_FALSE(second.Offer(CheckpointSample{6, nan, 0.001}));
  EXPECT_TRUE(second.QualifyingSamples().empty());
}

// One attempt per run, whatever it decided. A retry after a decline would turn
// one decision into a search.
TEST(TopologyCheckpointCoordinatorTest, OnlyOneAttemptIsPermitted) {
  TopologyCheckpointCoordinator coordinator(Config());
  EXPECT_FALSE(coordinator.Offer(Sample(5, 0.001)));
  EXPECT_FALSE(coordinator.Offer(Sample(6, 0.001)));
  ASSERT_TRUE(coordinator.Offer(Sample(7, 0.001)));

  coordinator.MarkAttempted();
  EXPECT_TRUE(coordinator.HasAttempted());
  EXPECT_FALSE(coordinator.Offer(Sample(8, 0.001)));
  EXPECT_FALSE(coordinator.Offer(Sample(9, 0.001)));
  EXPECT_FALSE(coordinator.Offer(Sample(10, 0.001)));
  EXPECT_NE(coordinator.LastReason().find("already been attempted"),
            std::string::npos);
}

// A design that never settles never offers a checkpoint, which is a result
// rather than a reason to lower the bar.
TEST(TopologyCheckpointCoordinatorTest, ANeverSettlingRunIsNeverEligible) {
  TopologyCheckpointCoordinator coordinator(Config());
  for (int iteration = 5; iteration < 60; ++iteration) {
    EXPECT_FALSE(coordinator.Offer(Sample(iteration, 0.03)));
  }
  EXPECT_TRUE(coordinator.QualifyingSamples().empty());
}

TEST(TopologyCheckpointCoordinatorTest, UnconfiguredWindowIsNeverEligible) {
  CheckpointEligibilityConfig config = Config();
  config.stability_window = 0;
  TopologyCheckpointCoordinator coordinator(config);
  EXPECT_FALSE(coordinator.Offer(Sample(5, 0.0)));
}

} // namespace
} // namespace dali
