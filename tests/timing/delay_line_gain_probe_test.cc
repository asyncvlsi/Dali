/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/timing/delay_line_gain_probe.h"

#include "dali/dali.h"

#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace dali {

class DelayLinePlacementRestoreTest : public testing::Test {
protected:
  DelayLinePlacementRestoreTest() : placer_(nullptr, severity::info) {
    Circuit &circuit = placer_.GetCircuit();
    circuit.SetManufacturingGrid(1);
    circuit.SetUnitsDistanceMicrons(1);
    circuit.SetGridValue(1, 1);
    circuit.SetDieArea(0, 0, 100, 100);
    circuit.ReserveSpaceForDesignImp(4, 0, 3);
    circuit.AddMacro("cell", 2, 2);
    Macro *macro = circuit.GetMacroPtr("cell");
    circuit.AddMacroPin(macro, "in", true);
    circuit.AddMacroPin(macro, "out", false);
    circuit.AddComponent("dl0_0", "cell", 10, 10, PLACED);
    circuit.AddComponent("dl0_1", "cell", 20, 20, UNPLACED);
    circuit.AddComponent("sink", "cell", 30, 30, FIXED);
    circuit.AddNet("dl0_n0", 2);
    circuit.AddComponentPinToNet("dl0_0", "out", "dl0_n0");
    circuit.AddComponentPinToNet("dl0_1", "in", "dl0_n0");
    circuit.AddNet("dl0_out", 2);
    circuit.AddComponentPinToNet("dl0_1", "out", "dl0_out");
    circuit.AddComponentPinToNet("sink", "in", "dl0_out");
  }

  std::vector<ComponentPlacement> Snapshot() {
    return placer_.SnapshotDelayLinePlacement("dl0");
  }

  void Restore(const std::vector<ComponentPlacement> &placements) {
    placer_.RestoreDelayLinePlacement(placements);
  }

  Dali placer_;
};

TEST_F(DelayLinePlacementRestoreTest, RestoresCoordinatesAndPlacementStatus) {
  const std::vector<ComponentPlacement> before = Snapshot();
  ASSERT_EQ(before.size(), 2U);

  Circuit &circuit = placer_.GetCircuit();
  for (Component &component : circuit.Components()) {
    if (component.Name().compare(0, 4, "dl0_") != 0)
      continue;
    component.SetLLX(component.LLX() + 7);
    component.SetLLY(component.LLY() + 9);
    component.SetPlacementStatus(FIXED);
  }
  Restore(before);

  const std::vector<ComponentPlacement> after = Snapshot();
  ASSERT_EQ(after.size(), before.size());
  for (std::size_t index = 0; index < before.size(); ++index) {
    EXPECT_EQ(after[index].component_id, before[index].component_id);
    EXPECT_DOUBLE_EQ(after[index].llx, before[index].llx);
    EXPECT_DOUBLE_EQ(after[index].lly, before[index].lly);
    EXPECT_EQ(after[index].status, before[index].status);
  }
}

namespace {

/**
 * A circuit stand-in whose slack depends on separation and on a placement term
 * the probe is not allowed to move.
 *
 * `placement_offset_ps` is the contamination: on the real fixture a spreading
 * step moved every constraint by ~175 ps between global-placement iterations
 * while no separation changed. A probe takes both its samples without the
 * offset changing, so the offset cancels; the old cross-iteration arithmetic
 * differenced two samples taken either side of a change to it, and did not.
 */
class FakeCircuit {
public:
  static constexpr double kTrueGainPsPerRow = 10.0;

  struct Site {
    int separation = 0;
    double base_slack = 0.0;
    std::string identity = "root|slow|fast";
    int attributed_constraints = 1;
    bool attribution_unique = true;
  };

  std::map<std::string, Site> sites;
  double placement_offset_ps = 0.0;
  bool refresh_succeeds = true;
  bool restore_enabled = true;
  std::vector<std::string> apply_order;
  std::vector<std::string> measured_order;
  int refresh_calls = 0;

  /**
   * Component ids are per site, so a restore reaches exactly the site it was
   * snapshotted from. A fake that restored every site from one snapshot would
   * hide a coordinator that mixed them up.
   */
  int BaseId(const std::string &site) const {
    int index = 0;
    for (const auto &[name, state] : sites) {
      (void)state;
      if (name == site)
        return 10 * (index + 1);
      ++index;
    }
    return 0;
  }

  std::vector<ComponentPlacement> Snapshot(const std::string &site) const {
    // A fixed anchor plus one movable cell whose y carries the separation, so a
    // shape that moved cells changes the digest and a restore returns it.
    const int base = BaseId(site);
    const Site &state = sites.at(site);
    return {{base, 0.0, 0.0, 1},
            {base + 1, 0.0, static_cast<double>(state.separation), 2}};
  }

  bool ApplyShape(const std::string &site, int separation) {
    apply_order.push_back(site);
    sites[site].separation = separation;
    return true;
  }

  bool RefreshTiming() {
    ++refresh_calls;
    return refresh_succeeds;
  }

  ProbeMeasurement Measure(const std::string &site) {
    measured_order.push_back(site);
    const Site &state = sites.at(site);
    ProbeMeasurement measurement;
    measurement.slack = state.base_slack +
                        kTrueGainPsPerRow * state.separation +
                        placement_offset_ps;
    measurement.identity = state.identity;
    measurement.attributed_constraints = state.attributed_constraints;
    measurement.attribution_unique = state.attribution_unique;
    return measurement;
  }

  void Restore(const std::vector<ComponentPlacement> &placements) {
    if (!restore_enabled)
      return;
    // The synthetic geometry encodes separation in the movable cell's y, so
    // restoring coordinates restores the separation the shape changed.
    for (const ComponentPlacement &placement : placements) {
      for (auto &[name, state] : sites) {
        if (placement.component_id != BaseId(name) + 1)
          continue;
        state.separation = static_cast<int>(placement.lly);
      }
    }
  }

  ProbeHooks Hooks() {
    ProbeHooks hooks;
    hooks.snapshot = [this](const std::string &site) { return Snapshot(site); };
    hooks.apply_shape = [this](const std::string &site, int separation) {
      return ApplyShape(site, separation);
    };
    hooks.refresh_timing = [this]() { return RefreshTiming(); };
    hooks.measure = [this](const std::string &site) { return Measure(site); };
    hooks.restore = [this](const std::vector<ComponentPlacement> &placements) {
      Restore(placements);
    };
    return hooks;
  }
};

ProbeSiteRequest Request(const std::string &site, int baseline, int trial,
                         double baseline_slack) {
  ProbeSiteRequest request;
  request.site = site;
  request.constraint_ids = {1};
  request.baseline_separation = baseline;
  request.trial_separation = trial;
  request.baseline_slack = baseline_slack;
  request.baseline_identity = "root|slow|fast";
  request.baseline_attributed_constraints = 1;
  return request;
}

IsolatedGainInput ValidInput() {
  IsolatedGainInput input;
  input.baseline_separation = 10;
  input.trial_separation = 11;
  input.baseline_slack = -100.0;
  input.trial_slack = -90.0;
  input.baseline_identity = "root|slow|fast";
  input.trial_identity = "root|slow|fast";
  input.baseline_attributed_constraints = 1;
  input.trial_attributed_constraints = 1;
  input.baseline_digest = 1234;
  input.restored_digest = 1234;
  return input;
}

/**
 * The estimator this work replaces: a slack difference over a separation
 * difference, with the two samples taken from different placements.
 *
 * Kept in the test rather than in production so the noise test can show what
 * the old arithmetic concluded from the same trajectory.
 */
double CrossIterationGain(double previous_slack, int previous_separation,
                          double slack, int separation) {
  return (slack - previous_slack) /
         static_cast<double>(separation - previous_separation);
}

} // namespace

// ---- 1. a one-row trial with a known positive gain -------------------------

TEST(IsolatedGainTest, MeasuresAOneRowTrialWithKnownGain) {
  FakeCircuit circuit;
  circuit.sites["dl0"] = {10, -200.0, "root|slow|fast", 1, true};

  const auto probes = RunIsolatedGainProbes(
      {Request("dl0", 10, 11, -200.0 + FakeCircuit::kTrueGainPsPerRow * 10)},
      circuit.Hooks());

  ASSERT_EQ(probes.size(), 1U);
  EXPECT_EQ(probes[0].outcome, ProbeOutcome::kAccepted);
  EXPECT_NEAR(probes[0].gain_ps_per_row, FakeCircuit::kTrueGainPsPerRow, 1e-9);
}

// ---- 2 and 17. placement noise does not reach the measurement --------------

TEST(IsolatedGainTest, UnrelatedPlacementSlackChangeDoesNotAffectTheGain) {
  FakeCircuit quiet;
  quiet.sites["dl0"] = {10, -200.0, "root|slow|fast", 1, true};
  const auto without_noise =
      RunIsolatedGainProbes({Request("dl0", 10, 11, -100.0)}, quiet.Hooks());

  FakeCircuit noisy;
  noisy.sites["dl0"] = {10, -200.0, "root|slow|fast", 1, true};
  noisy.placement_offset_ps = -200.0;
  // The baseline was taken under the same offset, because a probe measures
  // both samples without letting placement move in between.
  const auto with_noise = RunIsolatedGainProbes(
      {Request("dl0", 10, 11, -100.0 - 200.0)}, noisy.Hooks());

  ASSERT_EQ(without_noise[0].outcome, ProbeOutcome::kAccepted);
  ASSERT_EQ(with_noise[0].outcome, ProbeOutcome::kAccepted);
  EXPECT_NEAR(with_noise[0].gain_ps_per_row, without_noise[0].gain_ps_per_row,
              1e-9);
}

TEST(IsolatedGainTest, RecoversTheLocalGainThroughATwoHundredPsSpike) {
  // The measured width-1 trajectory: separation 71 -> 70 while a spreading
  // event moved slack +35.83 -> -90.09. The old arithmetic read 125.9 ps/row
  // from that pair and computed a zero step; the probe reads the true local
  // gain because both of its samples sit inside one placement.
  const double contaminated = CrossIterationGain(35.83, 71, -90.09, 70);
  EXPECT_GT(contaminated, 100.0)
      << "the cross-iteration estimate must be the inflated one";

  FakeCircuit circuit;
  circuit.sites["dl0"] = {70, -800.0, "root|slow|fast", 1, true};
  circuit.placement_offset_ps = -200.0;
  const double baseline_slack = circuit.Measure("dl0").slack;

  const auto probes = RunIsolatedGainProbes(
      {Request("dl0", 70, 71, baseline_slack)}, circuit.Hooks());

  ASSERT_EQ(probes[0].outcome, ProbeOutcome::kAccepted);
  EXPECT_NEAR(probes[0].gain_ps_per_row, FakeCircuit::kTrueGainPsPerRow, 1e-9);
  EXPECT_LT(probes[0].gain_ps_per_row, contaminated / 10.0)
      << "the isolated gain must not inherit the spike";
}

// ---- 3, 4. coordinates move only where they should, and come back ----------

TEST(IsolatedGainTest, OnlyTheProbedLineMoves) {
  FakeCircuit circuit;
  circuit.sites["dl0"] = {10, -200.0, "root|slow|fast", 1, true};
  circuit.sites["dl1"] = {5, -50.0, "root|slow|fast", 1, true};
  const int untouched_before = circuit.sites["dl1"].separation;

  RunIsolatedGainProbes({Request("dl0", 10, 11, -100.0)}, circuit.Hooks());

  EXPECT_EQ(circuit.apply_order, std::vector<std::string>{"dl0"});
  EXPECT_EQ(circuit.sites["dl1"].separation, untouched_before);
}

TEST(IsolatedGainTest, RestorationReturnsTheExactBaselineDigest) {
  FakeCircuit circuit;
  circuit.sites["dl0"] = {10, -200.0, "root|slow|fast", 1, true};

  const auto probes =
      RunIsolatedGainProbes({Request("dl0", 10, 11, -100.0)}, circuit.Hooks());

  EXPECT_EQ(probes[0].baseline_digest, probes[0].restored_digest);
  EXPECT_EQ(circuit.sites["dl0"].separation, 10);
}

TEST(PlacementDigestTest, DistinguishesACoordinateThatDidNotComeBack) {
  const std::vector<ComponentPlacement> baseline = {{1, 0.0, 0.0, 2},
                                                    {2, 4.0, 8.0, 2}};
  std::vector<ComponentPlacement> drifted = baseline;
  drifted[1].lly = 8.0 + 1e-12;

  EXPECT_NE(PlacementDigest(baseline), PlacementDigest(drifted))
      << "a restore within a tolerance of the baseline is not the baseline";
}

TEST(PlacementDigestTest, IsIndependentOfSnapshotOrder) {
  const std::vector<ComponentPlacement> forward = {{1, 0.0, 0.0, 2},
                                                   {2, 4.0, 8.0, 2}};
  const std::vector<ComponentPlacement> reversed = {{2, 4.0, 8.0, 2},
                                                    {1, 0.0, 0.0, 2}};

  EXPECT_EQ(PlacementDigest(forward), PlacementDigest(reversed));
}

TEST(PlacementDigestTest, NoticesAChangedPlacementStatus) {
  const std::vector<ComponentPlacement> placed = {{1, 4.0, 8.0, 2}};
  const std::vector<ComponentPlacement> fixed = {{1, 4.0, 8.0, 3}};

  EXPECT_NE(PlacementDigest(placed), PlacementDigest(fixed));
}

// ---- 5, 6. every site probes the same baseline, in a fixed order -----------

TEST(IsolatedGainTest, EverySiteProbesTheSameBaseline) {
  FakeCircuit circuit;
  circuit.sites["dl0"] = {10, -200.0, "root|slow|fast", 1, true};
  circuit.sites["dl1"] = {10, -200.0, "root|slow|fast", 1, true};

  const auto probes = RunIsolatedGainProbes(
      {Request("dl0", 10, 11, -100.0), Request("dl1", 10, 11, -100.0)},
      circuit.Hooks());

  ASSERT_EQ(probes.size(), 2U);
  EXPECT_EQ(probes[0].outcome, ProbeOutcome::kAccepted);
  EXPECT_EQ(probes[1].outcome, ProbeOutcome::kAccepted);
  EXPECT_NEAR(probes[0].gain_ps_per_row, probes[1].gain_ps_per_row, 1e-9)
      << "site B must not measure site A's trial geometry";
  // Each site snapshots its own components, so the two digests differ by
  // construction; what must hold is that each site left its own geometry as it
  // found it before the next site was tried.
  for (const IsolatedGainProbe &probe : probes) {
    EXPECT_EQ(probe.baseline_digest, probe.restored_digest) << probe.site;
  }
  EXPECT_EQ(circuit.sites["dl0"].separation, 10);
  EXPECT_EQ(circuit.sites["dl1"].separation, 10);
}

TEST(IsolatedGainTest, ProbesInDeterministicSiteOrder) {
  FakeCircuit circuit;
  for (const char *site : {"dl2", "dl0", "dl1"}) {
    circuit.sites[site] = {10, -200.0, "root|slow|fast", 1, true};
  }

  const auto probes = RunIsolatedGainProbes({Request("dl2", 10, 11, -100.0),
                                             Request("dl0", 10, 11, -100.0),
                                             Request("dl1", 10, 11, -100.0)},
                                            circuit.Hooks());

  const std::vector<std::string> expected = {"dl0", "dl1", "dl2"};
  EXPECT_EQ(circuit.apply_order, expected);
  EXPECT_EQ(circuit.measured_order, expected);
  ASSERT_EQ(probes.size(), 3U);
  EXPECT_EQ(probes[0].site, "dl0");
  EXPECT_EQ(probes[2].site, "dl2");
}

// ---- 7-12. the refusal rules ----------------------------------------------

TEST(IsolatedGainTest, AmbiguousAttributionRefuses) {
  IsolatedGainInput input = ValidInput();
  input.attribution_unique = false;

  const IsolatedGainResult result = EvaluateIsolatedGain(input);

  EXPECT_EQ(result.outcome, ProbeOutcome::kRefusedAmbiguousAttribution);
  EXPECT_FALSE(result.usable());
  EXPECT_EQ(result.gain_ps_per_row, 0.0);
}

TEST(IsolatedGainTest, MissingAttributionRefuses) {
  IsolatedGainInput input = ValidInput();
  input.trial_attributed_constraints = 0;

  EXPECT_EQ(EvaluateIsolatedGain(input).outcome,
            ProbeOutcome::kRefusedMissingAttribution);
}

TEST(IsolatedGainTest, ChangedSemanticIdentityRefuses) {
  IsolatedGainInput input = ValidInput();
  input.trial_identity = "root|other_slow|fast";

  EXPECT_EQ(EvaluateIsolatedGain(input).outcome,
            ProbeOutcome::kRefusedIdentityChanged);
}

TEST(IsolatedGainTest, NonFiniteTimingRefuses) {
  IsolatedGainInput input = ValidInput();
  input.trial_slack = std::numeric_limits<double>::quiet_NaN();
  EXPECT_EQ(EvaluateIsolatedGain(input).outcome,
            ProbeOutcome::kRefusedNonFiniteTiming);

  input.trial_slack = std::numeric_limits<double>::infinity();
  EXPECT_EQ(EvaluateIsolatedGain(input).outcome,
            ProbeOutcome::kRefusedNonFiniteTiming);
}

TEST(IsolatedGainTest, NonFiniteTimingStillRestores) {
  FakeCircuit circuit;
  circuit.sites["dl0"] = {10, std::numeric_limits<double>::quiet_NaN(),
                          "root|slow|fast", 1, true};

  const auto probes =
      RunIsolatedGainProbes({Request("dl0", 10, 11, -100.0)}, circuit.Hooks());

  EXPECT_EQ(probes[0].outcome, ProbeOutcome::kRefusedNonFiniteTiming);
  EXPECT_EQ(probes[0].baseline_digest, probes[0].restored_digest);
  EXPECT_EQ(circuit.sites["dl0"].separation, 10);
}

TEST(IsolatedGainTest, NonPositiveGainRefusesAndRestores) {
  IsolatedGainInput zero = ValidInput();
  zero.trial_slack = zero.baseline_slack;
  EXPECT_EQ(EvaluateIsolatedGain(zero).outcome,
            ProbeOutcome::kRefusedNonPositiveGain);

  IsolatedGainInput negative = ValidInput();
  negative.trial_slack = negative.baseline_slack - 40.0;
  EXPECT_EQ(EvaluateIsolatedGain(negative).outcome,
            ProbeOutcome::kRefusedNonPositiveGain);

  FakeCircuit circuit;
  circuit.sites["dl0"] = {10, -200.0, "root|slow|fast", 1, true};
  // A baseline already better than any trial makes the measured gain negative.
  const auto probes =
      RunIsolatedGainProbes({Request("dl0", 10, 11, 1e6)}, circuit.Hooks());
  EXPECT_EQ(probes[0].outcome, ProbeOutcome::kRefusedNonPositiveGain);
  EXPECT_EQ(probes[0].baseline_digest, probes[0].restored_digest);
  EXPECT_EQ(circuit.sites["dl0"].separation, 10);
}

TEST(IsolatedGainTest, AnAreaClampedNoOpIsNotAMeasurement) {
  IsolatedGainInput input = ValidInput();
  input.trial_separation = input.baseline_separation;

  const IsolatedGainResult result = EvaluateIsolatedGain(input);

  EXPECT_EQ(result.outcome, ProbeOutcome::kRefusedNoSeparationChange);
  EXPECT_EQ(result.gain_ps_per_row, 0.0)
      << "a clamp that changed nothing must not divide by zero";
}

TEST(IsolatedGainTest, AProposalOutsideTheAreaBoundRefuses) {
  IsolatedGainInput input = ValidInput();
  input.within_area_bound = false;

  EXPECT_EQ(EvaluateIsolatedGain(input).outcome,
            ProbeOutcome::kRefusedAreaBound);
}

// ---- 13, 14. failure paths leave nothing behind ---------------------------

TEST(IsolatedGainTest, FailedTimingRefreshRefusesAndRestores) {
  FakeCircuit circuit;
  circuit.sites["dl0"] = {10, -200.0, "root|slow|fast", 1, true};
  circuit.refresh_succeeds = false;

  const auto probes =
      RunIsolatedGainProbes({Request("dl0", 10, 11, -100.0)}, circuit.Hooks());

  EXPECT_EQ(probes[0].outcome, ProbeOutcome::kRefusedTimingRefreshFailed);
  EXPECT_EQ(probes[0].baseline_digest, probes[0].restored_digest);
  EXPECT_EQ(circuit.sites["dl0"].separation, 10);
}

TEST(IsolatedGainTest, NoProbeStateSurvivesTheProbePhase) {
  FakeCircuit circuit;
  circuit.sites["dl0"] = {10, -200.0, "root|slow|fast", 1, true};
  circuit.sites["dl1"] = {5, -50.0, "root|slow|fast", 1, true};
  const auto before = circuit.Snapshot("dl0");

  RunIsolatedGainProbes(
      {Request("dl0", 10, 40, -100.0), Request("dl1", 5, 25, -50.0)},
      circuit.Hooks());

  EXPECT_EQ(PlacementDigest(before), PlacementDigest(circuit.Snapshot("dl0")));
  EXPECT_EQ(circuit.sites["dl0"].separation, 10);
  EXPECT_EQ(circuit.sites["dl1"].separation, 5);
}

// ---- 18, 19. the controls that prove the tests bite ------------------------

TEST(IsolatedGainTest, WithoutRestorationTheSecondSiteInheritsTheFirst) {
  // Control 18 in production form: with restoration disabled the transaction
  // is exactly the thing this design replaces, and the probe says so rather
  // than reporting a gain measured against a contaminated baseline.
  FakeCircuit circuit;
  circuit.sites["dl0"] = {10, -200.0, "root|slow|fast", 1, true};
  circuit.sites["dl1"] = {10, -200.0, "root|slow|fast", 1, true};
  circuit.restore_enabled = false;

  const auto probes = RunIsolatedGainProbes(
      {Request("dl0", 10, 40, -100.0), Request("dl1", 10, 40, -100.0)},
      circuit.Hooks());

  ASSERT_EQ(probes.size(), 2U);
  EXPECT_NE(probes[0].baseline_digest, probes[0].restored_digest);
  EXPECT_EQ(probes[0].outcome, ProbeOutcome::kRefusedRestorationMismatch);
  EXPECT_EQ(probes[1].outcome, ProbeOutcome::kRefusedRestorationMismatch);
}

TEST(IsolatedGainTest, TheOldCrossIterationArithmeticFailsTheNoiseCase) {
  // Control 19: the same trajectory through both estimators. Reintroducing the
  // old arithmetic is what this asserts against, so a future change back to it
  // fails here rather than in a benchmark six hours later.
  const double true_gain = FakeCircuit::kTrueGainPsPerRow;
  const double spike = -200.0;

  FakeCircuit circuit;
  circuit.sites["dl0"] = {70, -800.0, "root|slow|fast", 1, true};
  const double slack_before_spike = circuit.Measure("dl0").slack;
  circuit.placement_offset_ps = spike;
  const double slack_after_spike = circuit.Measure("dl0").slack;

  circuit.sites["dl0"].separation = 71;
  const double cross_iteration =
      CrossIterationGain(slack_before_spike, 70, slack_after_spike, 71);
  circuit.sites["dl0"].separation = 70;

  const auto probes = RunIsolatedGainProbes(
      {Request("dl0", 70, 71, slack_after_spike)}, circuit.Hooks());

  ASSERT_EQ(probes[0].outcome, ProbeOutcome::kAccepted);
  EXPECT_NEAR(probes[0].gain_ps_per_row, true_gain, 1e-9);
  EXPECT_GT(std::abs(cross_iteration - true_gain), 100.0)
      << "the old estimator must be wrong on this trajectory, or the test "
         "proves nothing";
}

} // namespace dali
