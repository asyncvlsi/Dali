/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/global_placer/global_placer.h"

#include <gtest/gtest.h>

namespace dali {

class RecordingUpperBoundRefiner : public GlobalUpperBoundRefiner {
 public:
  void Initialize(double placement_density) override {
    (void)placement_density;
  }
  GlobalUpperBoundRefinement Refine(int iteration) override {
    return {true, static_cast<double>(iteration), 0.0, {}};
  }
  double GetTime() const override { return 0.0; }
  void Close() override {}
};

class TestableGlobalPlacer : public GlobalPlacer {
 public:
  using GlobalPlacer::ShouldRefineUpperBound;
  using GlobalPlacer::HasCurrentConvergenceUpperBound;

  void SetIterationForTest(int iteration) { cur_iter_ = iteration; }
  void SetCurrentUpperBoundPhysicalForTest(bool is_physical) {
    current_upper_bound_is_physical_ = is_physical;
  }
  bool UsesRefinedUpperBoundAsAnchor() const {
    return use_refined_upper_bound_as_anchor_;
  }
};

TEST(GlobalUpperBoundRefinerTest, HonorsWarmupAndInterval) {
  TestableGlobalPlacer placer;
  placer.SetUpperBoundRefiner(std::make_unique<RecordingUpperBoundRefiner>(),
                              2, 3);

  for (int iteration = 0; iteration < 8; ++iteration) {
    placer.SetIterationForTest(iteration);
    EXPECT_EQ(placer.ShouldRefineUpperBound(),
              iteration == 2 || iteration == 5);
  }
}

TEST(GlobalUpperBoundRefinerTest, CanRunOnEveryIteration) {
  TestableGlobalPlacer placer;
  placer.SetUpperBoundRefiner(std::make_unique<RecordingUpperBoundRefiner>(), 0,
                              1);

  for (int iteration = 0; iteration < 8; ++iteration) {
    placer.SetIterationForTest(iteration);
    EXPECT_TRUE(placer.ShouldRefineUpperBound());
  }
}

TEST(GlobalUpperBoundRefinerTest, RequiresFreshPhysicalConvergenceBound) {
  TestableGlobalPlacer placer;
  EXPECT_TRUE(placer.HasCurrentConvergenceUpperBound());

  placer.SetUpperBoundRefiner(std::make_unique<RecordingUpperBoundRefiner>(), 0,
                              1);
  EXPECT_FALSE(placer.HasCurrentConvergenceUpperBound());

  placer.SetCurrentUpperBoundPhysicalForTest(true);
  EXPECT_TRUE(placer.HasCurrentConvergenceUpperBound());
}

TEST(GlobalUpperBoundRefinerTest, RefinedAnchorFeedbackCanBeDisabled) {
  TestableGlobalPlacer placer;
  EXPECT_TRUE(placer.UsesRefinedUpperBoundAsAnchor());

  placer.SetUseRefinedUpperBoundAsAnchor(false);

  EXPECT_FALSE(placer.UsesRefinedUpperBoundAsAnchor());
}

}  // namespace dali
