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
 * The optimizer state that has to survive an engine rebuild.
 *
 * A topology checkpoint tears down the optimizer and builds a new one around
 * the changed netlist. Most of what the optimizer holds is derived from the
 * circuit and is rebuilt with it, but the anchor pseudo-net targets and the
 * accumulated anchor strength are not: they are per-run state that one
 * iteration hands to the next, and a rebuilt optimizer starts with neither.
 *
 * Losing them is not a rounding difference. On bd_pipeline it dropped the lower
 * bound from 63465 to 56975 in the first iteration after the rebuild and moved
 * the final placement by more than a percent of HPWL, which is far more than a
 * delay-line insertion would need to be measured against.
 *
 * Each test here compares a run that rebuilds the optimizer mid-way against one
 * that does not, and fails if the carried state is omitted.
 */
#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>

#include "dali/circuit/circuit.h"
#include "dali/placer/global_placer/hpwl_optimizer.h"

namespace dali {
namespace {

class TestBoundToBoundHpwlOptimizer : public BoundToBoundHpwlOptimizer {
 public:
  using BoundToBoundHpwlOptimizer::BoundToBoundHpwlOptimizer;

  void PoisonUnsetAnchors(double value) {
    x_anchor.setConstant(value);
    y_anchor.setConstant(-value);
    x_anchor_set = false;
    y_anchor_set = false;
  }
};

/** A small design with enough freedom for the solve to move cells. */
Circuit BuildCircuit(int cell_count = 12) {
  Circuit circuit;
  circuit.SetDatabaseMicrons(1000);
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetRowHeight(10);
  circuit.SetDieArea(0, 0, 400, 400);
  circuit.ReserveSpaceForDesignImp(cell_count, 0, cell_count);
  Macro* cell = circuit.AddMacro("cell", 10, 10);
  circuit.AddMacroPin(cell, "in", true)->SetOffset(1, 2);
  circuit.AddMacroPin(cell, "out", false)->SetOffset(9, 8);
  for (int index = 0; index < cell_count; ++index) {
    circuit.AddComponent("c" + std::to_string(index), "cell", 10.0 * index,
                         10.0 * index, PLACED, N, true);
  }
  for (int index = 0; index + 1 < cell_count; ++index) {
    const std::string net = "n" + std::to_string(index);
    circuit.AddNet(net, 2);
    circuit.AddComponentPinToNet("c" + std::to_string(index), "out", net);
    circuit.AddComponentPinToNet("c" + std::to_string(index + 1), "in", net);
  }
  circuit.UpdateTotalComponentArea();
  return circuit;
}

std::unique_ptr<BoundToBoundHpwlOptimizer> MakeOptimizer(Circuit* circuit) {
  auto optimizer = std::make_unique<BoundToBoundHpwlOptimizer>(circuit, 1);
  optimizer->Initialize();
  return optimizer;
}

/** Solves `iterations` times, optionally rebuilding before the last one. */
double RunSolves(Circuit* circuit, int iterations, bool rebuild_before_last,
                 bool carry_state, bool change_orientation = false,
                 bool perturb_before_last = false) {
  auto optimizer = MakeOptimizer(circuit);
  double last = 0.0;
  for (int iteration = 0; iteration < iterations; ++iteration) {
    if (change_orientation && iteration == iterations / 2) {
      for (size_t index = 1; index < circuit->Components().size(); index += 2) {
        circuit->Components()[index].SetOrient(FS);
      }
    }
    if (perturb_before_last && iteration == iterations - 1) {
      for (size_t index = 0; index < circuit->Components().size(); ++index) {
        Component& component = circuit->Components()[index];
        component.SetLoc(component.LLX() + (index % 2 == 0 ? 5.0 : -3.0),
                         component.LLY() + (index % 3 == 0 ? 4.0 : -2.0));
      }
    }
    if (rebuild_before_last && iteration == iterations - 1) {
      HpwlOptimizer::AnchorState state = optimizer->ExportAnchorState();
      optimizer->Close();
      optimizer = MakeOptimizer(circuit);
      if (carry_state) optimizer->ImportAnchorState(state);
    }
    optimizer->SetIteration(iteration);
    last = optimizer->OptimizeHpwl();
  }
  optimizer->Close();
  return last;
}

constexpr int kIterations = 8;

TEST(OptimizerAnchorStateTest,
     RebuildAfterOrientationChangesMatchesAnUninterruptedRun) {
  Circuit baseline = BuildCircuit();
  Circuit rebuilt = BuildCircuit();

  const double uninterrupted =
      RunSolves(&baseline, kIterations, false, false, true);
  const double carried = RunSolves(&rebuilt, kIterations, true, true, true);

  EXPECT_NEAR(uninterrupted, carried, 1e-9);
  ASSERT_EQ(baseline.Components().size(), rebuilt.Components().size());
  for (size_t index = 0; index < baseline.Components().size(); ++index) {
    EXPECT_NEAR(baseline.Components()[index].LLX(),
                rebuilt.Components()[index].LLX(), 1e-9)
        << "component " << index;
    EXPECT_NEAR(baseline.Components()[index].LLY(),
                rebuilt.Components()[index].LLY(), 1e-9)
        << "component " << index;
  }
}

// The property the checkpoint depends on: rebuilding the optimizer and carrying
// its per-run state produces the same solve as never rebuilding at all.
TEST(OptimizerAnchorStateTest, CarriedStateReproducesAnUninterruptedRun) {
  Circuit baseline = BuildCircuit();
  Circuit rebuilt = BuildCircuit();

  const double uninterrupted =
      RunSolves(&baseline, kIterations, false, false, false, true);
  const double carried =
      RunSolves(&rebuilt, kIterations, true, true, false, true);

  EXPECT_NEAR(uninterrupted, carried, 1e-9)
      << "a rebuild that carried its state did not reproduce the solve";
  ASSERT_EQ(baseline.Components().size(), rebuilt.Components().size());
  for (size_t index = 0; index < baseline.Components().size(); ++index) {
    EXPECT_NEAR(baseline.Components()[index].LLX(),
                rebuilt.Components()[index].LLX(), 1e-9)
        << "component " << index;
    EXPECT_NEAR(baseline.Components()[index].LLY(),
                rebuilt.Components()[index].LLY(), 1e-9)
        << "component " << index;
  }
}

// The same test with the state deliberately omitted, which is what the
// checkpoint did before this state was carried. If this ever stops differing,
// the test above has stopped proving anything.
TEST(OptimizerAnchorStateTest, OmittingTheStateChangesTheSolve) {
  Circuit baseline = BuildCircuit();
  Circuit rebuilt = BuildCircuit();

  const double uninterrupted =
      RunSolves(&baseline, kIterations, false, false, false, true);
  const double dropped =
      RunSolves(&rebuilt, kIterations, true, false, false, true);

  EXPECT_GT(std::abs(uninterrupted - dropped), 1e-9)
      << "dropping the anchor state made no difference, so carrying it is "
         "untested";
}

// Anchors are one target per component, so the state only describes the netlist
// it came from. Importing a mismatched one is refused rather than silently
// applied to the wrong components.
TEST(OptimizerAnchorStateTest, MismatchedStateIsRefused) {
  Circuit small = BuildCircuit(8);
  Circuit large = BuildCircuit(12);

  auto source = MakeOptimizer(&small);
  for (int iteration = 0; iteration < 3; ++iteration) {
    source->SetIteration(iteration);
    source->OptimizeHpwl();
  }
  HpwlOptimizer::AnchorState state = source->ExportAnchorState();
  ASSERT_TRUE(state.is_set);
  source->Close();

  auto target = MakeOptimizer(&large);
  EXPECT_DEATH(target->ImportAnchorState(state), "");
}

// Nothing to carry before the first anchored solve, and importing that is a
// no-op rather than an error.
TEST(OptimizerAnchorStateTest, UnanchoredStateIsEmptyAndImportsHarmlessly) {
  Circuit circuit = BuildCircuit();
  auto optimizer = MakeOptimizer(&circuit);
  const HpwlOptimizer::AnchorState state = optimizer->ExportAnchorState();
  EXPECT_FALSE(state.is_set);
  EXPECT_TRUE(state.x.empty());
  optimizer->ImportAnchorState(state);
  optimizer->Close();
}

TEST(OptimizerAnchorStateTest, FirstSolveInitializesTheAnchorState) {
  Circuit circuit = BuildCircuit();
  auto optimizer = MakeOptimizer(&circuit);
  optimizer->SetIteration(0);
  optimizer->OptimizeHpwl();

  const HpwlOptimizer::AnchorState state = optimizer->ExportAnchorState();
  EXPECT_TRUE(state.is_set);
  EXPECT_EQ(state.x.size(), circuit.Components().size());
  EXPECT_EQ(state.y.size(), circuit.Components().size());
  optimizer->Close();
}

TEST(OptimizerAnchorStateTest,
     RebuiltOptimizerAtNonzeroIterationIgnoresUnsetAnchorStorage) {
  Circuit first = BuildCircuit();
  Circuit second = BuildCircuit();
  TestBoundToBoundHpwlOptimizer first_optimizer(&first, 1);
  TestBoundToBoundHpwlOptimizer second_optimizer(&second, 1);
  first_optimizer.Initialize();
  second_optimizer.Initialize();
  first_optimizer.PoisonUnsetAnchors(1000000.0);
  second_optimizer.PoisonUnsetAnchors(-1000000.0);
  first_optimizer.SetIteration(13);
  second_optimizer.SetIteration(13);

  const double first_hpwl = first_optimizer.OptimizeHpwl();
  const double second_hpwl = second_optimizer.OptimizeHpwl();

  EXPECT_NEAR(first_hpwl, second_hpwl, 1e-9);
  ASSERT_EQ(first.Components().size(), second.Components().size());
  for (size_t index = 0; index < first.Components().size(); ++index) {
    EXPECT_NEAR(first.Components()[index].LLX(), second.Components()[index].LLX(),
                1e-9)
        << "component " << index;
    EXPECT_NEAR(first.Components()[index].LLY(), second.Components()[index].LLY(),
                1e-9)
        << "component " << index;
  }
  first_optimizer.Close();
  second_optimizer.Close();
}

}  // namespace
}  // namespace dali
