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
    return {true, static_cast<double>(iteration), 0.0};
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

class TestableHpwlOptimizer : public BoundToBoundHpwlOptimizer {
 public:
  explicit TestableHpwlOptimizer(Circuit* circuit)
      : BoundToBoundHpwlOptimizer(circuit, 1) {}

  double AnchorX(int component_id) const { return x_anchor[component_id]; }
  double AnchorY(int component_id) const { return y_anchor[component_id]; }
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

TEST(GlobalUpperBoundRefinerTest, ExplicitAnchorOverridesUpperBoundLocation) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(1, 0, 0);
  circuit.AddMacro("cell", 2, 2);
  circuit.AddComponent("u0", "cell", 10, 20, PLACED);

  TestableHpwlOptimizer optimizer(&circuit);
  optimizer.Initialize();
  optimizer.BackUpComponentLocation();
  circuit.GetComponentPtr("u0")->SetLowerLeft(30, 40);
  optimizer.SetExternalAnchorTargets({50}, {60});
  optimizer.SetIteration(1);

  optimizer.UpdateAnchorLocation();

  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("u0")->LLX(), 10);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("u0")->LLY(), 20);
  EXPECT_DOUBLE_EQ(optimizer.AnchorX(0), 50);
  EXPECT_DOUBLE_EQ(optimizer.AnchorY(0), 60);
}

}  // namespace dali
