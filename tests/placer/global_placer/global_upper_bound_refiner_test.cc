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
            GlobalRefinementFeedbackMode::kYRowTransactionalPositive);

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

TEST(GlobalUpperBoundRefinerTest, RowScaleFeedbackKeepsOnlyLargeYMoves) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 100, 100);
  circuit.ReserveSpaceForDesignImp(2, 0, 0);
  circuit.AddMacro("cell", 2, 2);
  circuit.AddComponent("small_move", "cell", 10, 20, PLACED);
  circuit.AddComponent("large_move", "cell", 12, 24, PLACED);

  TestableGlobalPlacer placer;
  placer.SetCircuit(&circuit);
  placer.SetRefinementFeedbackMode(GlobalRefinementFeedbackMode::kYRowScale);
  placer.ApplyFeedbackForTest({{1, 19}, {3, 20}});

  const Component& small_move = circuit.Components()[0];
  EXPECT_DOUBLE_EQ(small_move.LLX(), 1);
  EXPECT_DOUBLE_EQ(small_move.LLY(), 19);
  const Component& large_move = circuit.Components()[1];
  EXPECT_DOUBLE_EQ(large_move.LLX(), 3);
  EXPECT_DOUBLE_EQ(large_move.LLY(), 24);
}

TEST(GlobalUpperBoundRefinerTest, HpwlFilterKeepsOnlyNonWorseningYMoves) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 100, 100);
  circuit.ReserveSpaceForDesignImp(4, 0, 2);
  circuit.AddMacro("cell", 2, 2);
  Macro* macro = circuit.GetMacroPtr("cell");
  circuit.AddMacroPin(macro, "p", true)->SetOffset(0, 0);
  circuit.AddComponent("toward_anchor", "cell", 10, 12, PLACED);
  circuit.AddComponent("toward_anchor_fixed", "cell", 10, 0, FIXED);
  circuit.AddComponent("away_from_anchor", "cell", 20, 20, PLACED);
  circuit.AddComponent("away_from_anchor_fixed", "cell", 20, 0, FIXED);
  circuit.AddNet("toward_net", 2);
  circuit.AddComponentPinToNet("toward_anchor", "p", "toward_net");
  circuit.AddComponentPinToNet("toward_anchor_fixed", "p", "toward_net");
  circuit.AddNet("away_net", 2);
  circuit.AddComponentPinToNet("away_from_anchor", "p", "away_net");
  circuit.AddComponentPinToNet("away_from_anchor_fixed", "p", "away_net");

  TestableGlobalPlacer placer;
  placer.SetCircuit(&circuit);
  placer.SetRefinementFeedbackMode(GlobalRefinementFeedbackMode::kYRowHpwl);
  placer.ApplyFeedbackForTest({{10, 20}, {10, 0}, {20, 10}, {20, 0}});

  EXPECT_DOUBLE_EQ(circuit.Components()[0].LLY(), 12);
  EXPECT_DOUBLE_EQ(circuit.Components()[2].LLY(), 10);
}

TEST(GlobalUpperBoundRefinerTest,
     TransactionalFeedbackRejectsConflictingRowMove) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 100, 100);
  circuit.ReserveSpaceForDesignImp(2, 0, 1);
  circuit.AddMacro("cell", 2, 2);
  Macro* macro = circuit.GetMacroPtr("cell");
  circuit.AddMacroPin(macro, "p", true)->SetOffset(0, 0);
  circuit.AddComponent("first", "cell", 10, 9, PLACED);
  circuit.AddComponent("second", "cell", 20, 1, PLACED);
  circuit.AddNet("shared", 2);
  circuit.AddComponentPinToNet("first", "p", "shared");
  circuit.AddComponentPinToNet("second", "p", "shared");

  TestableGlobalPlacer placer;
  placer.SetCircuit(&circuit);
  placer.SetRefinementFeedbackMode(
      GlobalRefinementFeedbackMode::kYRowTransactional);
  placer.ApplyFeedbackForTest({{10, 0}, {20, 10}});

  EXPECT_DOUBLE_EQ(circuit.Components()[0].LLY(), 9);
  EXPECT_DOUBLE_EQ(circuit.Components()[1].LLY(), 10);
  EXPECT_DOUBLE_EQ(circuit.WeightedHPWLY(), 1);
}

TEST(GlobalUpperBoundRefinerTest,
     PositiveTransactionalFeedbackRejectsNeutralMove) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 100, 100);
  circuit.ReserveSpaceForDesignImp(3, 0, 1);
  circuit.AddMacro("cell", 2, 1);
  Macro* macro = circuit.GetMacroPtr("cell");
  circuit.AddMacroPin(macro, "p", true)->SetOffset(0, 0);
  circuit.AddComponent("movable", "cell", 10, 6, PLACED);
  circuit.AddComponent("low_anchor", "cell", 20, 0, FIXED);
  circuit.AddComponent("high_anchor", "cell", 30, 10, FIXED);
  circuit.AddNet("spanning_net", 3);
  circuit.AddComponentPinToNet("movable", "p", "spanning_net");
  circuit.AddComponentPinToNet("low_anchor", "p", "spanning_net");
  circuit.AddComponentPinToNet("high_anchor", "p", "spanning_net");

  TestableGlobalPlacer placer;
  placer.SetCircuit(&circuit);
  placer.SetRefinementFeedbackMode(
      GlobalRefinementFeedbackMode::kYRowTransactional);
  placer.ApplyFeedbackForTest({{10, 5}, {20, 0}, {30, 10}});
  EXPECT_DOUBLE_EQ(circuit.Components()[0].LLY(), 6);

  circuit.Components()[0].SetLLY(6);
  placer.SetRefinementFeedbackMode(
      GlobalRefinementFeedbackMode::kYRowTransactionalPositive);
  placer.ApplyFeedbackForTest({{10, 5}, {20, 0}, {30, 10}});
  EXPECT_DOUBLE_EQ(circuit.Components()[0].LLY(), 5);
}

}  // namespace dali
