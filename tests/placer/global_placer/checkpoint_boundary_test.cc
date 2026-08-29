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
 * The segmented checkpoint boundary.
 *
 * Global placement has to be interruptible so a delay line can grow partway
 * through it, and the interruption is only safe in one interval. The optimizer
 * holds a matrix sized by the component count and the spreader a grid indexed
 * by component id, so a netlist edit while either is alive corrupts both. These
 * tests pin the ordering that makes the edit safe -- every topology-sized
 * engine closed, then the host, then engines rebuilt -- and the state that has
 * to survive it.
 *
 * Two earlier versions of this file tested less than their names claimed. The
 * first had no host interval at all. The second called its equivalence test
 * "no-op segmented" while installing an observer that never took a checkpoint,
 * so it proved only that observation is inert. The equivalence test here takes
 * a checkpoint, returns no change, and rebuilds and resumes through it.
 */
#include <gtest/gtest.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/circuit/topology_delta.h"
#include "dali/placer/global_placer/global_placer.h"
#include "dali/placer/global_placer/placement_checkpoint.h"

namespace dali {
namespace {

constexpr int kIterations = 6;

/** A small placeable design: cells on a chain of two-pin nets. */
Circuit BuildPlaceableCircuit(int cell_count = 12) {
  Circuit circuit;
  circuit.SetDatabaseMicrons(1000);
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetRowHeight(10);
  circuit.SetDieArea(0, 0, 400, 400);
  circuit.ReserveSpaceForDesignImp(cell_count, 0, cell_count);

  Macro *cell = circuit.AddMacro("cell", 10, 10);
  circuit.AddMacroPin(cell, "in", true)->SetOffset(1, 5);
  circuit.AddMacroPin(cell, "out", false)->SetOffset(9, 5);

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

/** Ordered record of engine and host lifecycle events across a run. */
using EventLog = std::vector<std::string>;

/**
 * A refiner that accepts the placement as it finds it, and records its own
 * lifecycle.
 *
 * A checkpoint is offered only after an upper bound that is physical and
 * accepted, and only a refiner produces one. The real refiner rough-legalizes
 * onto a grid and needs a well legalizer; none of that is under test, so this
 * reports a feasible refinement and moves nothing. Its Close and Initialize
 * calls are recorded because the refiner is the one engine that stays allocated
 * across a checkpoint, so its lifecycle cannot be read off a pointer.
 */
class AcceptingRefiner : public GlobalUpperBoundRefiner {
 public:
  AcceptingRefiner(Circuit *circuit, EventLog *events)
      : circuit_(circuit), events_(events) {}
  void Initialize(double) override { events_->push_back("refiner_initialize"); }
  GlobalUpperBoundRefinement Refine(int) override {
    GlobalUpperBoundRefinement refinement;
    refinement.feasible = true;
    refinement.hpwl = circuit_->WeightedHPWL();
    refinement.anchor_all_components = true;
    return refinement;
  }
  double GetTime() const override { return 0.0; }
  void Close() override { events_->push_back("refiner_close"); }

 private:
  Circuit *circuit_ = nullptr;
  EventLog *events_ = nullptr;
};

/**
 * A refiner that never finds a feasible refinement.
 *
 * The upper bound then stays a spread cloud on every iteration, which is the
 * state a checkpoint must never be offered at: nothing has been shown
 * realizable, so a netlist changed there would be sized against a placement the
 * flow could not produce.
 */
class InfeasibleRefiner : public GlobalUpperBoundRefiner {
 public:
  explicit InfeasibleRefiner(EventLog *events) : events_(events) {}
  void Initialize(double) override { events_->push_back("refiner_initialize"); }
  GlobalUpperBoundRefinement Refine(int) override {
    GlobalUpperBoundRefinement refinement;
    refinement.feasible = false;
    return refinement;
  }
  double GetTime() const override { return 0.0; }
  void Close() override { events_->push_back("refiner_close"); }

 private:
  EventLog *events_ = nullptr;
};

/** Requests one topology change and records what it was handed. */
class SpyObserver : public PlacementCheckpointObserver {
 public:
  SpyObserver(int change_after_nth_offer, EventLog *events)
      : change_after_nth_offer_(change_after_nth_offer), events_(events) {}

  CheckpointDecision Observe(const PlacementCheckpoint &checkpoint) override {
    ++offers_;
    observe_saw_engines_open_.push_back(placer_->ArePlacementEnginesOpen());
    if (offers_ == change_after_nth_offer_ && !requested_) {
      requested_ = true;
      requested_at_iteration_ = checkpoint.iteration;
      return CheckpointDecision::kChangeTopology;
    }
    return CheckpointDecision::kContinue;
  }

  TopologyMutationResult RequestTopologyChange(
      const PlacementCheckpoint &checkpoint,
      const TopologyCheckpointContext &context) override {
    ++requests_;
    events_->push_back("host");
    host_saw_engines_open_ = placer_->ArePlacementEnginesOpen();
    context_ = context;
    checkpoint_iteration_ = checkpoint.iteration;
    return result_;
  }

  void SetPlacer(GlobalPlacer *placer) { placer_ = placer; }
  void SetResult(TopologyMutationResult result) {
    result_ = std::move(result);
  }

  int offers_ = 0;
  int requests_ = 0;
  bool requested_ = false;
  int requested_at_iteration_ = -1;
  int checkpoint_iteration_ = -1;
  bool host_saw_engines_open_ = true;
  TopologyCheckpointContext context_;
  std::vector<bool> observe_saw_engines_open_;

 private:
  int change_after_nth_offer_ = 0;
  EventLog *events_ = nullptr;
  GlobalPlacer *placer_ = nullptr;
  TopologyMutationResult result_ = TopologyMutationResult::NoChange();
};

void ConfigurePlacer(GlobalPlacer &placer, Circuit *circuit, EventLog *events) {
  placer.SetCircuit(circuit);
  placer.SetPlacementDensity(0.7);
  placer.SetMaxIteration(kIterations);
  placer.SetMinIteration(kIterations);
  placer.SetInitializerType(PlacementInitializerType::kKeep);
  placer.SetUpperBoundRefiner(
      std::make_unique<AcceptingRefiner>(circuit, events), 0, 1);
}

/** An ACT-shaped delta: two cells spliced onto the end of the chain. */
TopologyDelta MakeChainExtensionDelta(const std::string &tail_component) {
  TopologyDelta delta;
  delta.added_components.push_back({"dl_ainv_514_6", "cell", 5.0, 5.0});
  delta.added_components.push_back({"dl_ainv_515_6", "cell", 5.0, 5.0});
  delta.added_nets.push_back(
      {"dl_extnet_0", {{tail_component, "out"}, {"dl_ainv_514_6", "in"}}});
  delta.added_nets.push_back(
      {"dl_extnet_1", {{"dl_ainv_514_6", "out"}, {"dl_ainv_515_6", "in"}}});
  return delta;
}

// ---------------------------------------------------------------------------

TEST(CheckpointBoundaryTest, CheckpointIsTakenNotMerelyOffered) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);
  SpyObserver observer(2, &events);
  observer.SetPlacer(&placer);
  observer.SetResult(
      TopologyMutationResult::Applied(MakeChainExtensionDelta("c11")));
  ScopedCheckpointObserver installed(placer, &observer);

  ASSERT_TRUE(placer.StartPlacement());

  EXPECT_GT(observer.offers_, 0);
  EXPECT_EQ(observer.requests_, 1) << "the host boundary never ran";
  EXPECT_EQ(placer.CheckpointRestarts(), 1);
}

TEST(CheckpointBoundaryTest, RuntimeBreakdownAccountsEveryIteration) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);

  ASSERT_TRUE(placer.StartPlacement());

  const GlobalPlacer::RuntimeBreakdown &runtime = placer.GetRuntimeBreakdown();
  EXPECT_EQ(runtime.iterations, kIterations);
  EXPECT_EQ(runtime.physical_refinements, kIterations);
  EXPECT_EQ(runtime.anchor_feedbacks, kIterations);
  EXPECT_EQ(runtime.timing_observer_calls, 0);
  EXPECT_GT(runtime.optimizer_wall_seconds, 0.0);
  EXPECT_GT(runtime.spreader_wall_seconds, 0.0);
  EXPECT_GT(runtime.physical_refinement_wall_seconds, 0.0);
  EXPECT_GT(runtime.AccountedWallSeconds(), 0.0);
}

// The ordering the design exists for, including the refiner, which stays
// allocated across a checkpoint and so cannot be checked by a null pointer.
TEST(CheckpointBoundaryTest, EveryEngineIsClosedBeforeTheHostAndRebuiltAfter) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);
  SpyObserver observer(2, &events);
  observer.SetPlacer(&placer);
  observer.SetResult(
      TopologyMutationResult::Applied(MakeChainExtensionDelta("c11")));
  ScopedCheckpointObserver installed(placer, &observer);

  ASSERT_TRUE(placer.StartPlacement());

  ASSERT_EQ(observer.requests_, 1);
  EXPECT_FALSE(observer.host_saw_engines_open_)
      << "host ran while the optimizer or spreader was alive";
  for (bool open : observer.observe_saw_engines_open_) {
    EXPECT_TRUE(open) << "the decision is made mid-placement, engines up";
  }

  const auto host = std::find(events.begin(), events.end(), "host");
  ASSERT_NE(host, events.end());
  const auto close_before =
      std::find(events.begin(), host, "refiner_close");
  EXPECT_NE(close_before, host) << "refiner was not closed before the host ran";
  const auto init_after =
      std::find(host, events.end(), "refiner_initialize");
  EXPECT_NE(init_after, events.end())
      << "refiner was not re-initialized after the host ran";
}

TEST(CheckpointBoundaryTest, ResumeUsesTheNextAbsoluteIteration) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);
  SpyObserver observer(2, &events);
  observer.SetPlacer(&placer);
  ScopedCheckpointObserver installed(placer, &observer);

  ASSERT_TRUE(placer.StartPlacement());

  ASSERT_EQ(observer.requests_, 1);
  EXPECT_EQ(observer.context_.resume_iteration,
            observer.checkpoint_iteration_ + 1);
  EXPECT_EQ(observer.checkpoint_iteration_, observer.requested_at_iteration_);
}

// The host is told about the placement by value only. A raw Circuit pointer
// would let it leave a half-applied change behind for Dali to finalize.
TEST(CheckpointBoundaryTest, ContextIsValueOnlyAndDescribesTheCircuit) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  const size_t components = circuit.Components().size();
  const size_t nets = circuit.Nets().size();
  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);
  SpyObserver observer(2, &events);
  observer.SetPlacer(&placer);
  ScopedCheckpointObserver installed(placer, &observer);

  ASSERT_TRUE(placer.StartPlacement());

  EXPECT_EQ(observer.context_.component_count, components);
  EXPECT_EQ(observer.context_.net_count, nets);
  EXPECT_EQ(observer.context_.component_headroom,
            circuit.Components().capacity() - components);
  EXPECT_GT(observer.context_.component_headroom, 0u);
}

// The real thing: an ACT-shaped delta with components, nets, pins and stable
// names. Everything that existed keeps its id, name, coordinates and status;
// the new cells are present and connected; the engines see the new counts.
TEST(CheckpointBoundaryTest, ActShapedDeltaAppliesWithoutDisturbingTheDesign) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  const size_t components_before = circuit.Components().size();
  const size_t nets_before = circuit.Nets().size();
  const int c5_id = circuit.GetComponentPtr("c5")->Id();
  const int n3_id = circuit.GetNetPtr("n3")->Id();
  const PlaceStatus c5_status = circuit.GetComponentPtr("c5")->Status();

  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);
  SpyObserver observer(2, &events);
  observer.SetPlacer(&placer);
  observer.SetResult(
      TopologyMutationResult::Applied(MakeChainExtensionDelta("c11")));
  ScopedCheckpointObserver installed(placer, &observer);

  ASSERT_TRUE(placer.StartPlacement());

  EXPECT_EQ(circuit.Components().size(), components_before + 2);
  EXPECT_EQ(circuit.Nets().size(), nets_before + 2);
  // Identity of everything that already existed.
  EXPECT_EQ(circuit.GetComponentPtr("c5")->Id(), c5_id);
  EXPECT_EQ(circuit.GetNetPtr("n3")->Id(), n3_id);
  EXPECT_EQ(circuit.GetComponentPtr("c5")->Status(), c5_status);
  // The ACT-chosen names, unaltered.
  ASSERT_TRUE(circuit.IsComponentExisting("dl_ainv_514_6"));
  ASSERT_TRUE(circuit.IsComponentExisting("dl_ainv_515_6"));
  // New connectivity is real, not just new names.
  Net *bridge = circuit.GetNetPtr("dl_extnet_0");
  ASSERT_NE(bridge, nullptr);
  EXPECT_EQ(bridge->PinCnt(), 2u);
  bool joins_tail = false;
  bool joins_new = false;
  for (NetPin &pin : bridge->ComponentPins()) {
    if (pin.ComponentPtr()->Name() == "c11") joins_tail = true;
    if (pin.ComponentPtr()->Name() == "dl_ainv_514_6") joins_new = true;
  }
  EXPECT_TRUE(joins_tail);
  EXPECT_TRUE(joins_new);
}

// A host that cannot describe its change must not leave placement looking
// successful, and must not have its partial state finalized.
TEST(CheckpointBoundaryTest, HostFailureMakesPlacementFail) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);
  SpyObserver observer(2, &events);
  observer.SetPlacer(&placer);
  observer.SetResult(TopologyMutationResult::Failed("synthetic host failure"));
  ScopedCheckpointObserver installed(placer, &observer);

  EXPECT_FALSE(placer.StartPlacement())
      << "a host failure was reported as successful placement";
}

// An invalid delta is a host failure, caught before anything is applied.
TEST(CheckpointBoundaryTest, InvalidDeltaIsRejectedAndNothingIsApplied) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  const size_t components_before = circuit.Components().size();
  const size_t nets_before = circuit.Nets().size();

  TopologyDelta bad;
  bad.added_components.push_back({"ok_cell", "cell", 0.0, 0.0});
  // Names a master that does not exist, after a component that is fine.
  bad.added_components.push_back({"broken", "no_such_macro", 0.0, 0.0});

  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);
  SpyObserver observer(2, &events);
  observer.SetPlacer(&placer);
  observer.SetResult(TopologyMutationResult::Applied(bad));
  ScopedCheckpointObserver installed(placer, &observer);

  EXPECT_FALSE(placer.StartPlacement());
  EXPECT_EQ(circuit.Components().size(), components_before)
      << "a rejected delta was partially applied";
  EXPECT_EQ(circuit.Nets().size(), nets_before);
}

// Taking a checkpoint and changing nothing must be indistinguishable from never
// having stopped: same iterations, same coordinates, same HPWL.
TEST(CheckpointBoundaryTest, TakenNoChangeCheckpointMatchesUninterruptedRun) {
  EventLog baseline_events;
  Circuit baseline = BuildPlaceableCircuit();
  GlobalPlacer plain;
  ConfigurePlacer(plain, &baseline, &baseline_events);
  ASSERT_TRUE(plain.StartPlacement());

  EventLog events;
  Circuit segmented = BuildPlaceableCircuit();
  GlobalPlacer watched;
  ConfigurePlacer(watched, &segmented, &events);
  SpyObserver observer(2, &events);
  observer.SetPlacer(&watched);
  observer.SetResult(TopologyMutationResult::NoChange());
  {
    ScopedCheckpointObserver installed(watched, &observer);
    ASSERT_TRUE(watched.StartPlacement());
  }

  // The checkpoint really was taken, and really changed nothing.
  ASSERT_EQ(observer.requests_, 1);
  EXPECT_EQ(watched.CheckpointRestarts(), 0) << "no change is not a restart";
  ASSERT_EQ(baseline.Components().size(), segmented.Components().size());

  // Equal to solver noise, not bit-exact, and the distinction is real rather
  // than a weakened assertion. Rebuilding the optimizer restarts an iterative
  // conjugate-gradient solve that stops on a tolerance, so the segmented run
  // lands on an equally valid solution a few ulp away -- measured at ~1.4e-12
  // on coordinates of order 10. The bound below is three orders of magnitude
  // tighter than any real difference in placement would be: a cell that
  // actually moved would differ by a grid unit, not by 1e-9.
  constexpr double kSolverNoise = 1e-9;
  for (size_t index = 0; index < baseline.Components().size(); ++index) {
    EXPECT_NEAR(baseline.Components()[index].LLX(),
                segmented.Components()[index].LLX(), kSolverNoise)
        << "component " << index;
    EXPECT_NEAR(baseline.Components()[index].LLY(),
                segmented.Components()[index].LLY(), kSolverNoise)
        << "component " << index;
  }
  EXPECT_NEAR(baseline.WeightedHPWL(), segmented.WeightedHPWL(),
              kSolverNoise * baseline.Components().size());
}

// The trajectory belongs to the run, not to one topology, so a checkpoint must
// not restart it. Rebuilding the engines used to clear it.
TEST(CheckpointBoundaryTest, TrajectorySurvivesACheckpoint) {
  EventLog baseline_events;
  Circuit baseline = BuildPlaceableCircuit();
  GlobalPlacer plain;
  ConfigurePlacer(plain, &baseline, &baseline_events);
  ASSERT_TRUE(plain.StartPlacement());
  const size_t uninterrupted = plain.AcceptedUpperBoundHpwls().size();

  EventLog events;
  Circuit segmented = BuildPlaceableCircuit();
  GlobalPlacer watched;
  ConfigurePlacer(watched, &segmented, &events);
  SpyObserver observer(2, &events);
  observer.SetPlacer(&watched);
  ScopedCheckpointObserver installed(watched, &observer);
  ASSERT_TRUE(watched.StartPlacement());

  ASSERT_EQ(observer.requests_, 1);
  EXPECT_EQ(watched.AcceptedUpperBoundHpwls().size(), uninterrupted)
      << "the checkpoint restarted the trajectory instead of continuing it";
}

// The negative control for what a checkpoint is allowed to be. An iteration
// whose upper bound was never accepted as physical must not be offered, however
// eagerly an observer would take one -- this observer asks to change topology at
// the first opportunity and never gets one.
TEST(CheckpointBoundaryTest, IterationWithoutAPhysicalUpperBoundIsNeverOffered) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  const size_t components_before = circuit.Components().size();
  GlobalPlacer placer;
  placer.SetCircuit(&circuit);
  placer.SetPlacementDensity(0.7);
  placer.SetMaxIteration(kIterations);
  placer.SetMinIteration(kIterations);
  placer.SetInitializerType(PlacementInitializerType::kKeep);
  placer.SetUpperBoundRefiner(std::make_unique<InfeasibleRefiner>(&events), 0,
                              1);
  SpyObserver observer(1, &events);
  observer.SetPlacer(&placer);
  observer.SetResult(
      TopologyMutationResult::Applied(MakeChainExtensionDelta("c11")));
  ScopedCheckpointObserver installed(placer, &observer);

  ASSERT_TRUE(placer.StartPlacement());

  EXPECT_EQ(observer.offers_, 0)
      << "a checkpoint was offered without an accepted physical upper bound";
  EXPECT_EQ(observer.requests_, 0) << "the host ran without a physical state";
  EXPECT_EQ(placer.CheckpointRestarts(), 0);
  EXPECT_EQ(circuit.Components().size(), components_before);
}

// Convergence semantics at a checkpoint. A checkpointed iteration is evaluated
// for convergence like any other; the question is what happens to that answer.
//
// With nothing changed, the answer still stands, so an interrupted run must end
// on the same iteration an uninterrupted one does. Before this was explicit the
// convergence test was simply skipped on the checkpointed iteration, which
// added one silently.
TEST(CheckpointBoundaryTest, NoChangeCheckpointDoesNotAddAnIteration) {
  EventLog baseline_events;
  Circuit baseline = BuildPlaceableCircuit();
  GlobalPlacer plain;
  ConfigurePlacer(plain, &baseline, &baseline_events);
  // A minimum below the cap, so convergence can actually stop the run early.
  plain.SetMinIteration(2);
  ASSERT_TRUE(plain.StartPlacement());
  const size_t uninterrupted = plain.AcceptedUpperBoundHpwls().size();

  EventLog events;
  Circuit segmented = BuildPlaceableCircuit();
  GlobalPlacer watched;
  ConfigurePlacer(watched, &segmented, &events);
  watched.SetMinIteration(2);
  SpyObserver observer(2, &events);
  observer.SetPlacer(&watched);
  observer.SetResult(TopologyMutationResult::NoChange());
  ScopedCheckpointObserver installed(watched, &observer);
  ASSERT_TRUE(watched.StartPlacement());

  ASSERT_EQ(observer.requests_, 1) << "the checkpoint was never taken";
  EXPECT_EQ(watched.AcceptedUpperBoundHpwls().size(), uninterrupted)
      << "a no-change checkpoint changed how many iterations ran";
}

// A topology change makes the objective a different one, so the convergence
// decision taken before it does not carry over and placement resumes.
TEST(CheckpointBoundaryTest, TopologyChangeResumesPastConvergence) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);
  placer.SetMinIteration(2);
  SpyObserver observer(2, &events);
  observer.SetPlacer(&placer);
  observer.SetResult(
      TopologyMutationResult::Applied(MakeChainExtensionDelta("c11")));
  ScopedCheckpointObserver installed(placer, &observer);

  ASSERT_TRUE(placer.StartPlacement());

  ASSERT_EQ(observer.requests_, 1);
  EXPECT_EQ(placer.CheckpointRestarts(), 1);
  // Resumed rather than stopped at the checkpoint.
  EXPECT_GT(placer.AcceptedUpperBoundHpwls().size(),
            static_cast<size_t>(observer.checkpoint_iteration_ + 1));
}

// The accepted physical placement exists only between the refiner accepting it
// and the anchor feedback restoring coordinates from the analytical solve. An
// observer that fired after that would report a different placement from the
// one the accepted upper bound describes -- which is exactly the defect the
// four-state study was built to find.
// The two observers must bracket the feedback, one on each side, at the same
// iterations. Whether the two placements actually differ is a property of the
// feedback mode and the design, not of this fixture -- whose configured mode
// moves nothing -- so it is measured on the benchmark rather than asserted
// here. What this pins is the ordering, which is what Amendment N found wrong:
// sizing read the post-feedback placement while reporting the accepted one.
TEST(CheckpointBoundaryTest, ObserversBracketTheAnchorFeedback) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);

  std::vector<std::pair<std::string, int>> order;
  placer.SetAcceptedPhysicalObserver([&](int iteration) {
    order.emplace_back("accepted", iteration);
  });
  placer.SetPostFeedbackObserver([&](int iteration) {
    order.emplace_back("feedback", iteration);
  });

  ASSERT_TRUE(placer.StartPlacement());

  ASSERT_FALSE(order.empty());
  ASSERT_EQ(order.size() % 2, 0U);
  for (size_t index = 0; index < order.size(); index += 2) {
    EXPECT_EQ(order[index].first, "accepted");
    EXPECT_EQ(order[index + 1].first, "feedback");
    EXPECT_EQ(order[index].second, order[index + 1].second)
        << "the two observations at this position describe different iterations";
  }
}

TEST(CheckpointBoundaryTest, PostFeedbackObserverCanBeCleared) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);

  int fired = 0;
  placer.SetPostFeedbackObserver([&](int) { ++fired; });
  placer.SetPostFeedbackObserver(nullptr);
  ASSERT_TRUE(placer.StartPlacement());
  EXPECT_EQ(fired, 0) << "a cleared observer still fired, so the scoped guard "
                         "that clears it on an early return would not protect "
                         "a caller's dead frame";
}

TEST(CheckpointBoundaryTest, AcceptedPhysicalObserverFiresBeforeFeedback) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);

  std::vector<int> observed_iterations;
  std::vector<double> observed_hpwl;
  placer.SetAcceptedPhysicalObserver([&](int iteration) {
    observed_iterations.push_back(iteration);
    observed_hpwl.push_back(circuit.WeightedHPWL());
  });

  ASSERT_TRUE(placer.StartPlacement());

  // One observation per accepted physical upper bound, in iteration order.
  ASSERT_FALSE(observed_iterations.empty());
  for (size_t index = 1; index < observed_iterations.size(); ++index) {
    EXPECT_LT(observed_iterations[index - 1], observed_iterations[index]);
  }
  // The HPWL seen by the observer is the accepted one the placer recorded, not
  // whatever the placement becomes after feedback.
  ASSERT_EQ(observed_hpwl.size(), placer.AcceptedUpperBoundHpwls().size());
  for (size_t index = 0; index < observed_hpwl.size(); ++index) {
    EXPECT_NEAR(observed_hpwl[index], placer.AcceptedUpperBoundHpwls()[index],
                1e-6)
        << "observation " << index << " did not see the accepted placement";
  }
}

// Observation must not change the placement it observes.
TEST(CheckpointBoundaryTest, AcceptedPhysicalObservationIsNeutral) {
  EventLog plain_events;
  Circuit plain = BuildPlaceableCircuit();
  GlobalPlacer without;
  ConfigurePlacer(without, &plain, &plain_events);
  ASSERT_TRUE(without.StartPlacement());

  EventLog observed_events;
  Circuit observed = BuildPlaceableCircuit();
  GlobalPlacer with;
  ConfigurePlacer(with, &observed, &observed_events);
  int fired = 0;
  with.SetAcceptedPhysicalObserver([&](int) {
    ++fired;
    (void)observed.WeightedHPWL();  // a read, which is all an observer may do
  });
  ASSERT_TRUE(with.StartPlacement());

  EXPECT_GT(fired, 0);
  ASSERT_EQ(plain.Components().size(), observed.Components().size());
  for (size_t index = 0; index < plain.Components().size(); ++index) {
    EXPECT_DOUBLE_EQ(plain.Components()[index].LLX(),
                     observed.Components()[index].LLX()) << index;
    EXPECT_DOUBLE_EQ(plain.Components()[index].LLY(),
                     observed.Components()[index].LLY()) << index;
  }
  EXPECT_DOUBLE_EQ(plain.WeightedHPWL(), observed.WeightedHPWL());
  EXPECT_EQ(without.AcceptedUpperBoundHpwls(), with.AcceptedUpperBoundHpwls());
}

TEST(CheckpointBoundaryTest, ObserverIsClearedOnScopeExit) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);
  {
    RecordingCheckpointObserver recorder;
    ScopedCheckpointObserver installed(placer, &recorder);
    ASSERT_TRUE(placer.StartPlacement());
    EXPECT_GT(recorder.Count(), 0u);
  }
  GlobalPlacer second;
  ConfigurePlacer(second, &circuit, &events);
  EXPECT_TRUE(second.StartPlacement());
}

TEST(CheckpointBoundaryTest, ObserverIsClearedWhenScopeLeftByException) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);
  try {
    RecordingCheckpointObserver recorder;
    ScopedCheckpointObserver installed(placer, &recorder);
    throw std::runtime_error("early exit");
  } catch (const std::runtime_error &) {
  }
  // A still-installed observer would now be a pointer into a destroyed frame.
  EXPECT_TRUE(placer.StartPlacement());
}

// Observer cleanup must also survive the failure path, which returns early.
TEST(CheckpointBoundaryTest, ObserverIsClearedAfterAFailedPlacement) {
  EventLog events;
  Circuit circuit = BuildPlaceableCircuit();
  GlobalPlacer placer;
  ConfigurePlacer(placer, &circuit, &events);
  {
    SpyObserver observer(2, &events);
    observer.SetPlacer(&placer);
    observer.SetResult(TopologyMutationResult::Failed("synthetic"));
    ScopedCheckpointObserver installed(placer, &observer);
    EXPECT_FALSE(placer.StartPlacement());
  }
  GlobalPlacer second;
  ConfigurePlacer(second, &circuit, &events);
  EXPECT_TRUE(second.StartPlacement());
}

} // namespace
} // namespace dali
