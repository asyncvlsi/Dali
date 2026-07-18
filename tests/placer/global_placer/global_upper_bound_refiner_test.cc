/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include <gtest/gtest.h>

#include <string>
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
  using GlobalPlacer::BuildRelativeYConstraints;
  using GlobalPlacer::HasCurrentConvergenceUpperBound;
  using GlobalPlacer::ShouldRefineUpperBound;

  void SetIterationForTest(int iteration) { cur_iter_ = iteration; }
  void SetCurrentUpperBoundPhysicalForTest(bool is_physical) {
    current_upper_bound_is_physical_ = is_physical;
  }
  GlobalRefinementFeedbackMode RefinementFeedbackMode() const {
    return refinement_feedback_mode_;
  }
  void SaveFeedbackCheckpointForTest() {
    previous_feedback_checkpoint_ = SaveCurrentPlacement();
  }
  bool RollbackFeedbackForTest(const GlobalUpperBoundRefinement& refinement) {
    return RollbackRefinementFeedbackIfRequested(refinement);
  }
  void ApplyFeedbackForTest(
      const std::vector<std::pair<double, double>>& original_locations,
      bool anchor_all_components = true,
      const std::vector<int>& component_ids = {},
      const std::vector<std::vector<int>>& component_rows = {}) {
    std::vector<ComponentLocation> placement;
    placement.reserve(original_locations.size());
    for (const auto& location : original_locations) {
      placement.push_back({location.first, location.second});
    }
    ApplyRefinedAnchorFeedback(placement, anchor_all_components, component_ids,
                               component_rows);
  }
  void RestoreSavedPlacementForTest() {
    const std::vector<ComponentLocation> placement = SaveCurrentPlacement();
    for (Component& component : ckt_ptr_->Components()) {
      component.SetLowerLeft(-1.0, -1.0);
      component.SetOrient(N);
    }
    RestorePlacement(placement);
  }
};

class TestableHpwlOptimizer : public BoundToBoundHpwlOptimizer {
 public:
  explicit TestableHpwlOptimizer(Circuit* circuit)
      : BoundToBoundHpwlOptimizer(circuit, 1) {}

  void AddRelativeYConstraintsForTest(
      std::vector<RelativeYConstraint> constraints, double anchor_alpha,
      double height_epsilon) {
    Initialize();
    relative_y_constraints_ = std::move(constraints);
    alpha = anchor_alpha;
    height_epsilon_ = height_epsilon;
    coefficients_y_.clear();
    by.setZero();
    AddRelativeYConstraints();
  }

  double Coefficient(int row, int column) const {
    double value = 0.0;
    for (const SparseTriplet& coefficient : coefficients_y_) {
      if (coefficient.row() == row && coefficient.col() == column) {
        value += coefficient.value();
      }
    }
    return value;
  }

  double RightHandSide(int row) const { return by[row]; }
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

TEST(GlobalUpperBoundRefinerTest, RestoresRequestedFeedbackCheckpoint) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 100, 100);
  circuit.ReserveSpaceForDesignImp(1, 0, 0);
  circuit.AddMacro("cell", 1, 1);
  circuit.AddComponent("movable", "cell", 10, 20, PLACED);
  circuit.Components().front().SetOrient(FS);

  TestableGlobalPlacer placer;
  placer.SetCircuit(&circuit);
  placer.SaveFeedbackCheckpointForTest();
  circuit.Components().front().SetLowerLeft(30, 40);
  circuit.Components().front().SetOrient(N);

  GlobalUpperBoundRefinement refinement;
  refinement.rollback_previous_anchor_feedback = true;
  EXPECT_TRUE(placer.RollbackFeedbackForTest(refinement));
  EXPECT_DOUBLE_EQ(circuit.Components().front().LLX(), 10);
  EXPECT_DOUBLE_EQ(circuit.Components().front().LLY(), 20);
  EXPECT_EQ(circuit.Components().front().Orient(), FS);
}

TEST(GlobalUpperBoundRefinerTest, PlacementSnapshotPreservesCompleteState) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 100, 100);
  circuit.ReserveSpaceForDesignImp(1, 0, 0);
  circuit.AddMacro("cell", 1, 1);
  circuit.AddComponent("movable", "cell", 10.25, 20.75, PLACED);
  circuit.Components().front().SetOrient(FS);

  TestableGlobalPlacer placer;
  placer.SetCircuit(&circuit);
  placer.RestoreSavedPlacementForTest();

  EXPECT_DOUBLE_EQ(circuit.Components().front().LLX(), 10.25);
  EXPECT_DOUBLE_EQ(circuit.Components().front().LLY(), 20.75);
  EXPECT_EQ(circuit.Components().front().Orient(), FS);
}

TEST(GlobalUpperBoundRefinerTest, RefinedAnchorFeedbackCanBeDisabled) {
  TestableGlobalPlacer placer;
  EXPECT_EQ(placer.RefinementFeedbackMode(),
            GlobalRefinementFeedbackMode::kYRowTransactionalConsistent);

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

TEST(GlobalUpperBoundRefinerTest, EmptySelectiveAnchorSetSelectsNothing) {
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
  placer.ApplyFeedbackForTest({{1, 2}}, false);

  EXPECT_DOUBLE_EQ(circuit.Components().front().LLX(), 1);
  EXPECT_DOUBLE_EQ(circuit.Components().front().LLY(), 2);
}

TEST(GlobalUpperBoundRefinerTest,
     BuildsConstraintsOnlyForAdjacentAcceptedComponents) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 100, 100);
  circuit.ReserveSpaceForDesignImp(4, 0, 0);
  circuit.AddMacro("cell", 1, 1);
  for (int i = 0; i < 4; ++i) {
    circuit.AddComponent("component_" + std::to_string(i), "cell", i, 10 + i,
                         PLACED);
  }

  TestableGlobalPlacer placer;
  placer.SetCircuit(&circuit);
  std::vector<RelativeYConstraint> constraints =
      placer.BuildRelativeYConstraints({{0, 1, 2, 3}},
                                       {true, true, false, true});

  ASSERT_EQ(constraints.size(), 1);
  EXPECT_EQ(constraints[0].first_component_id, 0);
  EXPECT_EQ(constraints[0].second_component_id, 1);
  EXPECT_DOUBLE_EQ(constraints[0].offset, -1.0);
}

TEST(GlobalUpperBoundRefinerTest,
     RelativeYConstraintAddsTranslationInvariantQuadraticTerm) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 100, 100);
  circuit.ReserveSpaceForDesignImp(2, 0, 0);
  circuit.AddMacro("cell", 1, 1);
  circuit.AddComponent("first", "cell", 0, 5, PLACED);
  circuit.AddComponent("second", "cell", 0, 2, PLACED);

  TestableHpwlOptimizer optimizer(&circuit);
  optimizer.AddRelativeYConstraintsForTest({{0, 1, 1.0}}, 2.0, 1.0);

  const double expected_weight = 2.0 / 3.0;
  EXPECT_NEAR(optimizer.Coefficient(0, 0), expected_weight, 1e-12);
  EXPECT_NEAR(optimizer.Coefficient(1, 1), expected_weight, 1e-12);
  EXPECT_NEAR(optimizer.Coefficient(0, 1), -expected_weight, 1e-12);
  EXPECT_NEAR(optimizer.Coefficient(1, 0), -expected_weight, 1e-12);
  EXPECT_NEAR(optimizer.RightHandSide(0), expected_weight, 1e-12);
  EXPECT_NEAR(optimizer.RightHandSide(1), -expected_weight, 1e-12);
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

TEST(GlobalUpperBoundRefinerTest,
     ConsistentTransactionalFeedbackRejectsContextCreatedGain) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 100, 100);
  circuit.ReserveSpaceForDesignImp(3, 0, 2);
  circuit.AddMacro("cell", 2, 1);
  Macro* macro = circuit.GetMacroPtr("cell");
  circuit.AddMacroPin(macro, "p", true)->SetOffset(0, 0);
  circuit.AddComponent("center", "cell", 10, 2, PLACED);
  circuit.AddComponent("context_move", "cell", 20, 2, PLACED);
  circuit.AddComponent("other_neighbor", "cell", 30, 0, PLACED);
  circuit.AddNet("context_net", 2);
  circuit.AddComponentPinToNet("center", "p", "context_net");
  circuit.AddComponentPinToNet("context_move", "p", "context_net");
  circuit.AddNet("other_net", 2);
  circuit.AddComponentPinToNet("center", "p", "other_net");
  circuit.AddComponentPinToNet("other_neighbor", "p", "other_net");

  TestableGlobalPlacer placer;
  placer.SetCircuit(&circuit);
  placer.SetRefinementFeedbackMode(
      GlobalRefinementFeedbackMode::kYRowTransactionalPositive);
  placer.ApplyFeedbackForTest({{10, 0}, {20, 1}, {30, 2}});
  EXPECT_DOUBLE_EQ(circuit.Components()[1].LLY(), 2);
  EXPECT_DOUBLE_EQ(circuit.WeightedHPWLY(), 0);

  circuit.Components()[0].SetLLY(2);
  circuit.Components()[1].SetLLY(2);
  circuit.Components()[2].SetLLY(0);
  placer.SetRefinementFeedbackMode(
      GlobalRefinementFeedbackMode::kYRowTransactionalConsistent);
  placer.ApplyFeedbackForTest({{10, 0}, {20, 1}, {30, 2}});

  EXPECT_DOUBLE_EQ(circuit.Components()[0].LLY(), 2);
  EXPECT_DOUBLE_EQ(circuit.Components()[1].LLY(), 1);
  EXPECT_DOUBLE_EQ(circuit.Components()[2].LLY(), 2);
  EXPECT_DOUBLE_EQ(circuit.WeightedHPWLY(), 1);
}

}  // namespace dali
