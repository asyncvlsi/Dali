/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/global_placer/global_placer.h"

namespace dali {

class RecordingUpperBoundRefiner : public GlobalUpperBoundRefiner {
 public:
  void Initialize(double placement_density) override {
    (void)placement_density;
  }
  GlobalUpperBoundRefinement Refine(int iteration) override {
    GlobalUpperBoundRefinement refinement;
    refinement.feasible = true;
    refinement.hpwl = static_cast<double>(iteration);
    return refinement;
  }
  double GetTime() const override { return 0.0; }
  void Close() override {}
};

class TestableGlobalPlacer : public GlobalPlacer {
 public:
  using GlobalPlacer::HasCurrentConvergenceUpperBound;
  using GlobalPlacer::ShouldRefineUpperBound;

  void SetIterationForTest(int iteration) { cur_iter_ = iteration; }
  void SetCurrentUpperBoundPhysicalForTest(bool is_physical) {
    current_upper_bound_is_physical_ = is_physical;
  }
  GlobalRefinementFeedbackMode RefinementFeedbackMode() const {
    return refinement_feedback_mode_;
  }
  void ApplyFeedbackForTest(
      const std::vector<std::pair<double, double>>& original_locations,
      const std::vector<int>& component_ids = {}) {
    std::vector<ComponentLocation> placement;
    placement.reserve(original_locations.size());
    for (const auto& location : original_locations) {
      placement.push_back({location.first, location.second});
    }
    ApplyRefinedAnchorFeedback(placement, component_ids);
  }
};

TEST(GlobalUpperBoundRefinerTest, HonorsWarmupAndInterval) {
  TestableGlobalPlacer placer;
  placer.SetUpperBoundRefiner(std::make_unique<RecordingUpperBoundRefiner>(), 2,
                              3);

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
  EXPECT_EQ(placer.RefinementFeedbackMode(),
            GlobalRefinementFeedbackMode::kFull);

  placer.SetUseRefinedUpperBoundAsAnchor(false);

  EXPECT_EQ(placer.RefinementFeedbackMode(),
            GlobalRefinementFeedbackMode::kNone);
}

TEST(GlobalUpperBoundRefinerTest, SelectsOneFeedbackAxis) {
  TestableGlobalPlacer placer;
  placer.SetRefinementFeedbackMode(GlobalRefinementFeedbackMode::kYOnly);

  EXPECT_EQ(placer.RefinementFeedbackMode(),
            GlobalRefinementFeedbackMode::kYOnly);
}

TEST(GlobalUpperBoundRefinerTest, YOnlyFeedbackPreservesAnalyticalX) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 100, 100);
  circuit.ReserveSpaceForDesignImp(1, 0, 0);
  circuit.AddMacro("cell", 2, 2);
  circuit.AddComponent("movable", "cell", 10, 20, PLACED);

  TestableGlobalPlacer placer;
  placer.SetCircuit(&circuit);
  placer.SetRefinementFeedbackMode(GlobalRefinementFeedbackMode::kYOnly);
  placer.ApplyFeedbackForTest({{1, 2}});

  const Component& component = circuit.Components().front();
  EXPECT_DOUBLE_EQ(component.LLX(), 1);
  EXPECT_DOUBLE_EQ(component.LLY(), 20);
}

}  // namespace dali
