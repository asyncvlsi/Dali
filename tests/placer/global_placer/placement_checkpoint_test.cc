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
#include "dali/placer/global_placer/placement_checkpoint.h"

#include <gtest/gtest.h>

#include <cstddef>

namespace dali {
namespace {

PlacementCheckpoint MakeCheckpoint(int iteration, double hpwl, int count) {
  PlacementCheckpoint checkpoint;
  checkpoint.iteration = iteration;
  checkpoint.accepted_hpwl = hpwl;
  checkpoint.physical_upper_bound_count = count;
  return checkpoint;
}

TEST(PlacementCheckpointTest, RecordingObserverNeverAsksForATopologyChange) {
  RecordingCheckpointObserver observer;
  for (int iteration = 0; iteration < 8; ++iteration) {
    EXPECT_EQ(observer.Observe(MakeCheckpoint(iteration, 100.0, iteration + 1)),
              CheckpointDecision::kContinue);
  }
}

TEST(PlacementCheckpointTest, RecordingObserverKeepsEveryCheckpointInOrder) {
  RecordingCheckpointObserver observer;
  observer.Observe(MakeCheckpoint(3, 500.0, 1));
  observer.Observe(MakeCheckpoint(7, 400.0, 2));

  ASSERT_EQ(observer.Count(), 2u);
  EXPECT_EQ(observer.Checkpoints()[0].iteration, 3);
  EXPECT_DOUBLE_EQ(observer.Checkpoints()[0].accepted_hpwl, 500.0);
  EXPECT_EQ(observer.Checkpoints()[1].iteration, 7);
  EXPECT_EQ(observer.Checkpoints()[1].physical_upper_bound_count, 2);
}

/**
 * A run that never produces a physical upper bound must never offer a
 * checkpoint. The placer enforces this by consulting the observer only inside
 * the accepted-physical-refinement branch; this pins the other half, that an
 * observer which is never consulted reports nothing rather than defaulting to
 * some checkpoint at iteration zero.
 */
TEST(PlacementCheckpointTest, NoPhysicalUpperBoundMeansNoCheckpoint) {
  RecordingCheckpointObserver observer;
  EXPECT_EQ(observer.Count(), 0u);
  EXPECT_TRUE(observer.Checkpoints().empty());
}

} // namespace
} // namespace dali
