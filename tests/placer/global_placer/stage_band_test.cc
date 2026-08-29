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
#include "dali/placer/global_placer/stage_band.h"

#include <gtest/gtest.h>

#include <cstddef>

namespace dali {
namespace {

TEST(StageBandTest, BandsStackInDeclarationOrderFromTheBottom) {
  const std::vector<StageBandInterval> intervals =
      BuildStageBandIntervals({1.0, 1.0, 1.0, 1.0}, 0.0, 100.0);
  ASSERT_EQ(intervals.size(), 4u);
  EXPECT_DOUBLE_EQ(intervals[0].y_lo, 0.0);
  EXPECT_DOUBLE_EQ(intervals[0].y_hi, 25.0);
  EXPECT_DOUBLE_EQ(intervals[3].y_lo, 75.0);
  EXPECT_DOUBLE_EQ(intervals[3].y_hi, 100.0);
}

TEST(StageBandTest, BandsAreContiguousAndSpanTheWholeRegion) {
  const std::vector<StageBandInterval> intervals =
      BuildStageBandIntervals({3.0, 7.0, 11.0}, 20.0, 80.0);
  ASSERT_EQ(intervals.size(), 3u);
  EXPECT_DOUBLE_EQ(intervals.front().y_lo, 20.0);
  EXPECT_DOUBLE_EQ(intervals.back().y_hi, 80.0);
  for (size_t i = 1; i < intervals.size(); ++i) {
    EXPECT_DOUBLE_EQ(intervals[i].y_lo, intervals[i - 1].y_hi);
  }
}

// The property that keeps bands from creating a density hotspot: every band
// ends up with the same area per unit height as the region as a whole.
TEST(StageBandTest, HeightIsProportionalToArea) {
  const std::vector<double> areas = {10.0, 20.0, 70.0};
  const std::vector<StageBandInterval> intervals =
      BuildStageBandIntervals(areas, 0.0, 200.0);
  ASSERT_EQ(intervals.size(), 3u);
  EXPECT_DOUBLE_EQ(intervals[0].Height(), 20.0);
  EXPECT_DOUBLE_EQ(intervals[1].Height(), 40.0);
  EXPECT_DOUBLE_EQ(intervals[2].Height(), 140.0);

  const double reference = areas[0] / intervals[0].Height();
  for (size_t i = 0; i < areas.size(); ++i) {
    EXPECT_NEAR(areas[i] / intervals[i].Height(), reference, 1e-12);
  }
}

// A stage whose cells are all fixed still holds its place in the stack, so a
// caller may index the result by stage number.
TEST(StageBandTest, EmptyBandKeepsItsPositionWithZeroHeight) {
  const std::vector<StageBandInterval> intervals =
      BuildStageBandIntervals({5.0, 0.0, 5.0}, 0.0, 10.0);
  ASSERT_EQ(intervals.size(), 3u);
  EXPECT_DOUBLE_EQ(intervals[1].Height(), 0.0);
  EXPECT_DOUBLE_EQ(intervals[1].y_lo, 5.0);
  EXPECT_DOUBLE_EQ(intervals[2].y_lo, 5.0);
  EXPECT_DOUBLE_EQ(intervals[2].y_hi, 10.0);
}

TEST(StageBandTest, NoDivisionExistsWithoutHeightOrArea) {
  EXPECT_TRUE(BuildStageBandIntervals({1.0, 1.0}, 50.0, 50.0).empty());
  EXPECT_TRUE(BuildStageBandIntervals({0.0, 0.0}, 0.0, 100.0).empty());
  EXPECT_TRUE(BuildStageBandIntervals({}, 0.0, 100.0).empty());
}

// Negative areas cannot arise from real cells, but a caller that computes them
// by subtraction could produce one; it must not steal height from its
// neighbours.
TEST(StageBandTest, NegativeAreaIsTreatedAsEmpty) {
  const std::vector<StageBandInterval> intervals =
      BuildStageBandIntervals({-4.0, 1.0, 1.0}, 0.0, 100.0);
  ASSERT_EQ(intervals.size(), 3u);
  EXPECT_DOUBLE_EQ(intervals[0].Height(), 0.0);
  EXPECT_DOUBLE_EQ(intervals[1].Height(), 50.0);
  EXPECT_DOUBLE_EQ(intervals[2].y_hi, 100.0);
}

// Equal spacing is chosen precisely because wirelength cannot distinguish
// arrangements along a chain, so it must ignore area entirely -- including a
// stage large enough to dominate an area-proportional split.
TEST(StageBandTest, UniformSpacingIgnoresArea) {
  const std::vector<StageBandInterval> intervals = BuildStageBandIntervals(
      {1.0, 98.0, 1.0}, 0.0, 300.0, StageBandSpacing::kUniform);
  ASSERT_EQ(intervals.size(), 3u);
  for (const StageBandInterval &interval : intervals) {
    EXPECT_NEAR(interval.Height(), 100.0, 1e-12);
  }
  EXPECT_DOUBLE_EQ(intervals.front().y_lo, 0.0);
  EXPECT_DOUBLE_EQ(intervals.back().y_hi, 300.0);
}

// An empty stage keeps a full slot under uniform spacing, unlike the
// area-proportional split where it collapses; skipping it would shift every
// band above it and defeat the point of asking for equal spacing.
TEST(StageBandTest, UniformSpacingKeepsAFullSlotForAnEmptyBand) {
  const std::vector<StageBandInterval> intervals = BuildStageBandIntervals(
      {1.0, 0.0, 1.0, 1.0}, 0.0, 400.0, StageBandSpacing::kUniform);
  ASSERT_EQ(intervals.size(), 4u);
  EXPECT_DOUBLE_EQ(intervals[1].y_lo, 100.0);
  EXPECT_DOUBLE_EQ(intervals[1].y_hi, 200.0);
  EXPECT_DOUBLE_EQ(intervals[2].y_lo, 200.0);
}

} // namespace
} // namespace dali
