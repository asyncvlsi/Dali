#include "dali/timing/timing_driven_placement_controller.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace dali {

namespace timing_driven_placement_controller_test {

TimingDrivenPlacementCandidate Candidate(int value) {
  TimingDrivenPlacementCandidate candidate;
  candidate.delay_parameters = {{"site", value}};
  return candidate;
}

TimingDrivenPlacementMeasurement
Measurement(const TimingDrivenPlacementCandidate &candidate, double period,
            std::initializer_list<double> slacks,
            std::string artifact = "artifact") {
  TimingDrivenPlacementMeasurement measurement;
  measurement.delay_parameters = candidate.delay_parameters;
  measurement.replacement_processes = candidate.replacement_processes;
  measurement.period_ps = period;
  measurement.wns_ps = *std::min_element(slacks.begin(), slacks.end());
  measurement.tns_ps = 0.0;
  measurement.constraint_count = static_cast<int>(slacks.size());
  int id = 0;
  for (const double slack : slacks) {
    measurement.constraints.push_back({id++, "site", slack});
  }
  measurement.placement_hpwl_um = 10.0;
  measurement.placement_legal = true;
  measurement.overlap_count = 0;
  measurement.artifact_id = std::move(artifact);
  return measurement;
}

struct HostTrial {
  bool begin_ok = true;
  std::optional<TimingDrivenPlacementMeasurement> measurement;
  bool commit_ok = true;
  bool rollback_ok = true;
};

class RecordingHost final : public TimingDrivenFlowHost {
public:
  explicit RecordingHost(std::vector<HostTrial> trials)
      : trials_(std::move(trials)) {}

  bool BeginTrial(const TimingDrivenPlacementCandidate &candidate,
                  const std::string &anchor) override {
    calls.push_back(
        "Begin:" + std::to_string(candidate.delay_parameters.at("site")) + ":" +
        anchor);
    current_trial_ = next_trial_++;
    return trials_.at(current_trial_).begin_ok;
  }

  std::optional<TimingDrivenPlacementMeasurement>
  RunPlacementAndTiming() override {
    calls.push_back("Run");
    return trials_.at(current_trial_).measurement;
  }

  bool CommitTrial() override {
    calls.push_back("Commit");
    return trials_.at(current_trial_).commit_ok;
  }

  bool RollbackTrial() override {
    calls.push_back("Rollback");
    return trials_.at(current_trial_).rollback_ok;
  }

  std::vector<std::string> calls;

private:
  std::vector<HostTrial> trials_;
  std::size_t next_trial_ = 0;
  std::size_t current_trial_ = 0;
};

TimingDrivenPlacementControllerConfig Config(TimingDrivenBaselineMode mode);

class RecordingObserver final : public TimingDrivenPlacementEventObserver {
public:
  void OnTimingDrivenPlacementEvent(
      const TimingDrivenPlacementEvent &event) override {
    events.push_back(event);
  }

  std::vector<TimingDrivenPlacementEvent> events;
};

class ThrowingObserver final : public TimingDrivenPlacementEventObserver {
public:
  void OnTimingDrivenPlacementEvent(
      const TimingDrivenPlacementEvent &event) override {
    static_cast<void>(event);
    ++calls;
    throw std::runtime_error("observer failure");
  }

  int calls = 0;
};

TimingDrivenPlacementControllerConfig MetadataConfig() {
  TimingDrivenPlacementControllerConfig config =
      Config(TimingDrivenBaselineMode::kTrackBestInfeasible);
  config.require_measurement_metadata = true;
  config.require_lifecycle_invariants = true;
  config.initial_generation_id = "generation-0";
  config.expected_die_grid = {0, 0, 100, 100, 1, 1, true};
  config.expected_timing_use_rc = true;
  config.expected_rc_min_routing_layer = 1;
  return config;
}

TimingDrivenPlacementMeasurement MetadataMeasurement(
    const TimingDrivenPlacementCandidate &candidate, double period,
    std::initializer_list<double> slacks, const std::string &artifact,
    const std::string &artifact_parent, const std::string &generation,
    const std::string &generation_parent) {
  TimingDrivenPlacementMeasurement measurement =
      Measurement(candidate, period, slacks, artifact);
  measurement.topology_identity = "topology-" + artifact;
  measurement.static_inventory_digest = "static-A";
  measurement.io_inventory_digest = "io-A";
  measurement.constraint_id_digest = "ids:0,1,";
  measurement.die_grid = {0, 0, 100, 100, 1, 1, true};
  measurement.timing_use_rc = true;
  measurement.rc_min_routing_layer = 1;
  measurement.artifact_parent_id = artifact_parent;
  measurement.generation_id = generation;
  measurement.generation_parent_id = generation_parent;
  return measurement;
}

class SequencePolicy final : public TimingDrivenCandidatePolicy {
public:
  explicit SequencePolicy(
      std::vector<TimingDrivenPlacementCandidate> candidates)
      : candidates_(std::move(candidates)) {}

  std::optional<TimingDrivenPlacementCandidate> NextCandidate(
      TimingDrivenPlacementCandidate current_candidate,
      std::optional<TimingDrivenPlacementMeasurement> current_measurement,
      std::vector<TimingDrivenTrialRecord> history) override {
    current_candidates.push_back(std::move(current_candidate));
    current_measurements.push_back(std::move(current_measurement));
    history_sizes.push_back(history.size());
    if (next_ == candidates_.size()) {
      return std::nullopt;
    }
    return candidates_[next_++];
  }

  std::vector<TimingDrivenPlacementCandidate> current_candidates;
  std::vector<std::optional<TimingDrivenPlacementMeasurement>>
      current_measurements;
  std::vector<std::size_t> history_sizes;

private:
  std::vector<TimingDrivenPlacementCandidate> candidates_;
  std::size_t next_ = 0;
};

TimingDrivenPlacementControllerConfig
Config(TimingDrivenBaselineMode mode =
           TimingDrivenBaselineMode::kRequireFeasible) {
  TimingDrivenPlacementControllerConfig config;
  config.initial_candidate = Candidate(0);
  config.expected_constraint_count = 2;
  config.required_slack_margin_ps = 25.0;
  config.max_trials = 8;
  config.no_improvement_limit = 1;
  config.minimum_period_improvement_ps = 0.0;
  config.baseline_mode = mode;
  return config;
}

std::unique_ptr<SequencePolicy>
Policy(std::vector<TimingDrivenPlacementCandidate> candidates) {
  return std::make_unique<SequencePolicy>(std::move(candidates));
}

} // namespace timing_driven_placement_controller_test

using timing_driven_placement_controller_test::Candidate;
using timing_driven_placement_controller_test::Config;
using timing_driven_placement_controller_test::HostTrial;
using timing_driven_placement_controller_test::Measurement;
using timing_driven_placement_controller_test::MetadataConfig;
using timing_driven_placement_controller_test::MetadataMeasurement;
using timing_driven_placement_controller_test::Policy;
using timing_driven_placement_controller_test::RecordingHost;
using timing_driven_placement_controller_test::RecordingObserver;
using timing_driven_placement_controller_test::ThrowingObserver;

TEST(TimingDrivenPlacementControllerTest, CommitsImprovementAndExhaustsPolicy) {
  const auto candidate = Candidate(1);
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base"), true,
       true},
      {true, Measurement(candidate, 90.0, {30.0, 31.0}, "improved"), true,
       true},
  });
  auto policy = Policy({candidate});
  TimingDrivenPlacementController controller(Config(), std::move(policy));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.terminal_status, TimingDrivenTerminalStatus::kConverged);
  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kPolicyExhausted);
  EXPECT_EQ(result.convergence_reason,
            TimingDrivenConvergenceReason::kPolicyExhausted);
  ASSERT_TRUE(result.accepted_measurement.has_value());
  EXPECT_DOUBLE_EQ(result.accepted_measurement->period_ps, 90.0);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit",
                                      "Begin:1:base", "Run", "Commit"}));
}

TEST(TimingDrivenPlacementControllerTest,
     RejectedTrialRollsBackAndConvergesByNoImprovement) {
  const auto candidate = Candidate(1);
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base"), true,
       true},
      {true, Measurement(candidate, 110.0, {30.0, 31.0}, "rejected"), true,
       true},
  });
  TimingDrivenPlacementController controller(Config(), Policy({candidate}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.terminal_status, TimingDrivenTerminalStatus::kConverged);
  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kNoImprovement);
  EXPECT_EQ(result.convergence_reason,
            TimingDrivenConvergenceReason::kNoImprovement);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit",
                                      "Begin:1:base", "Run", "Rollback"}));
  ASSERT_EQ(result.history.size(), 2U);
  EXPECT_EQ(result.history.back().decision,
            TimingDrivenTrialDecision::kRejected);
}

TEST(TimingDrivenPlacementControllerTest,
     BeginFailureStillRollsBackExactlyOnce) {
  RecordingHost host({{false, std::nullopt, true, true}});
  TimingDrivenPlacementController controller(Config(), Policy({}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kHostFailure);
  EXPECT_EQ(host.calls, (std::vector<std::string>{"Begin:0:seed", "Rollback"}));
}

TEST(TimingDrivenPlacementControllerTest, TimingFailureRollsBack) {
  RecordingHost host({{true, std::nullopt, true, true}});
  TimingDrivenPlacementController controller(Config(), Policy({}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kHostFailure);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Rollback"}));
}

TEST(TimingDrivenPlacementControllerTest, MalformedMeasurementRollsBack) {
  auto malformed = Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base");
  malformed.constraint_count = 1;
  RecordingHost host({{true, malformed, true, true}});
  TimingDrivenPlacementController controller(Config(), Policy({}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kValidationFailure);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Rollback"}));
}

TEST(TimingDrivenPlacementControllerTest, RejectsNonFiniteMeasurementFields) {
  const auto valid = Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base");
  const std::vector<std::function<void(TimingDrivenPlacementMeasurement &)>>
      mutations = {
          [](auto &measurement) { measurement.period_ps = 0.0; },
          [](auto &measurement) { measurement.wns_ps = NAN; },
          [](auto &measurement) { measurement.tns_ps = INFINITY; },
          [](auto &measurement) { measurement.placement_hpwl_um = NAN; },
          [](auto &measurement) { measurement.constraints[0].slack_ps = NAN; },
      };

  for (const auto &mutate : mutations) {
    auto malformed = valid;
    mutate(malformed);
    RecordingHost host({{true, malformed, true, true}});
    TimingDrivenPlacementController controller(Config(), Policy({}));
    const auto result = controller.Run(host);
    EXPECT_EQ(result.termination_reason,
              TimingDrivenTerminationReason::kValidationFailure);
    EXPECT_EQ(host.calls,
              (std::vector<std::string>{"Begin:0:seed", "Run", "Rollback"}));
  }
}

TEST(TimingDrivenPlacementControllerTest,
     RejectsParameterArtifactAndDuplicateIdErrors) {
  const auto valid = Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base");
  const std::vector<std::function<void(TimingDrivenPlacementMeasurement &)>>
      mutations = {
          [](auto &measurement) { measurement.delay_parameters["site"] = 7; },
          [](auto &measurement) { measurement.artifact_id.clear(); },
          [](auto &measurement) {
            measurement.constraints[1].constraint_id =
                measurement.constraints[0].constraint_id;
          },
      };

  for (const auto &mutate : mutations) {
    auto malformed = valid;
    mutate(malformed);
    RecordingHost host({{true, malformed, true, true}});
    TimingDrivenPlacementController controller(Config(), Policy({}));
    const auto result = controller.Run(host);
    EXPECT_EQ(result.termination_reason,
              TimingDrivenTerminationReason::kValidationFailure);
    EXPECT_EQ(host.calls,
              (std::vector<std::string>{"Begin:0:seed", "Run", "Rollback"}));
  }
}

TEST(TimingDrivenPlacementControllerTest,
     RejectsIllegalAndOverlappingMeasurements) {
  const auto valid = Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base");
  for (const int failure_case : {0, 1}) {
    auto malformed = valid;
    malformed.placement_legal = failure_case != 0;
    malformed.overlap_count = failure_case == 1 ? 1 : 0;
    RecordingHost host({{true, malformed, true, true}});
    TimingDrivenPlacementController controller(Config(), Policy({}));
    const auto result = controller.Run(host);
    EXPECT_EQ(result.termination_reason,
              TimingDrivenTerminationReason::kValidationFailure);
    EXPECT_EQ(host.calls,
              (std::vector<std::string>{"Begin:0:seed", "Run", "Rollback"}));
  }
}

TEST(TimingDrivenPlacementControllerTest, CommitFailureRollsBackOnceAndStops) {
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base"), false,
       true},
  });
  TimingDrivenPlacementController controller(Config(), Policy({}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kCommitFailure);
  EXPECT_EQ(host.calls, (std::vector<std::string>{"Begin:0:seed", "Run",
                                                  "Commit", "Rollback"}));
}

TEST(TimingDrivenPlacementControllerTest, RollbackFailureIsTerminal) {
  const auto candidate = Candidate(1);
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base"), true,
       true},
      {true, Measurement(candidate, 110.0, {30.0, 31.0}, "rejected"), true,
       false},
  });
  TimingDrivenPlacementController controller(Config(),
                                             Policy({candidate, Candidate(2)}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kRollbackFailure);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit",
                                      "Begin:1:base", "Run", "Rollback"}));
}

TEST(TimingDrivenPlacementControllerTest,
     TracksBestInfeasibleByWnsBeforePeriod) {
  const auto candidate = Candidate(1);
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {20.0, 20.0}, "base"), true,
       true},
      {true, Measurement(candidate, 90.0, {9.0, 20.0}, "worse"), true, true},
  });
  TimingDrivenPlacementController controller(
      Config(TimingDrivenBaselineMode::kTrackBestInfeasible),
      Policy({candidate}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.terminal_status, TimingDrivenTerminalStatus::kFailed);
  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kNoFeasibleSolution);
  EXPECT_EQ(result.convergence_reason,
            TimingDrivenConvergenceReason::kNoImprovement);
  EXPECT_EQ(result.history[1].decision, TimingDrivenTrialDecision::kRejected);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit",
                                      "Begin:1:base", "Run", "Rollback"}));
  ASSERT_TRUE(result.provisional_measurement.has_value());
  EXPECT_EQ(result.provisional_measurement->artifact_id, "base");
}

TEST(TimingDrivenPlacementControllerTest,
     ImprovedWnsCommitsEvenWithHigherPeriod) {
  const auto candidate = Candidate(1);
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {20.0, 20.0}, "base"), true,
       true},
      {true, Measurement(candidate, 110.0, {21.0, 20.0}, "better"), true, true},
  });
  TimingDrivenPlacementController controller(
      Config(TimingDrivenBaselineMode::kTrackBestInfeasible),
      Policy({candidate}));

  const auto result = controller.Run(host);

  ASSERT_TRUE(result.provisional_measurement.has_value());
  EXPECT_EQ(result.provisional_measurement->artifact_id, "better");
  EXPECT_EQ(result.history[1].decision,
            TimingDrivenTrialDecision::kBestInfeasible);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit",
                                      "Begin:1:base", "Run", "Commit"}));
}

TEST(TimingDrivenPlacementControllerTest,
     FeasibleCandidateSupersedesInfeasibleAnchor) {
  const auto candidate = Candidate(1);
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {20.0, 20.0}, "base"), true,
       true},
      {true, Measurement(candidate, 150.0, {25.0, 26.0}, "feasible"), true,
       true},
  });
  TimingDrivenPlacementController controller(
      Config(TimingDrivenBaselineMode::kTrackBestInfeasible),
      Policy({candidate}));

  const auto result = controller.Run(host);

  ASSERT_TRUE(result.accepted_measurement.has_value());
  EXPECT_EQ(result.accepted_measurement->artifact_id, "feasible");
  ASSERT_TRUE(result.best_infeasible_measurement.has_value());
  EXPECT_EQ(result.best_infeasible_measurement->artifact_id, "base");
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit",
                                      "Begin:1:base", "Run", "Commit"}));
}

TEST(TimingDrivenPlacementControllerTest,
     InfeasibleCandidateCannotReplaceFeasibleState) {
  const auto candidate = Candidate(1);
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {25.0, 26.0}, "base"), true,
       true},
      {true, Measurement(candidate, 1.0, {24.0, 100.0}, "infeasible"), true,
       true},
  });
  TimingDrivenPlacementController controller(Config(), Policy({candidate}));

  const auto result = controller.Run(host);

  ASSERT_TRUE(result.accepted_measurement.has_value());
  EXPECT_EQ(result.accepted_measurement->artifact_id, "base");
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit",
                                      "Begin:1:base", "Run", "Rollback"}));
}

TEST(TimingDrivenPlacementControllerTest,
     PeriodComparisonStartsAfterFeasibility) {
  const auto infeasible = Candidate(1);
  const auto feasible = Candidate(2);
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {20.0, 20.0}, "base"), true,
       true},
      {true, Measurement(infeasible, 200.0, {25.0, 26.0}, "feasible"), true,
       true},
      {true, Measurement(feasible, 150.0, {26.0, 27.0}, "faster"), true, true},
  });
  TimingDrivenPlacementController controller(
      Config(TimingDrivenBaselineMode::kTrackBestInfeasible),
      Policy({infeasible, feasible}));

  const auto result = controller.Run(host);

  ASSERT_TRUE(result.accepted_measurement.has_value());
  EXPECT_EQ(result.accepted_measurement->artifact_id, "faster");
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit",
                                      "Begin:1:base", "Run", "Commit",
                                      "Begin:2:feasible", "Run", "Commit"}));
}

TEST(TimingDrivenPlacementControllerTest,
     PolicyReceivesCommittedArtifactAnchorAndHistory) {
  const auto candidate = Candidate(1);
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base"), true,
       true},
      {true, Measurement(candidate, 90.0, {30.0, 31.0}, "next"), true, true},
  });
  auto policy = Policy({candidate});
  auto *policy_ptr = policy.get();
  TimingDrivenPlacementController controller(Config(), std::move(policy));

  controller.Run(host);

  ASSERT_EQ(policy_ptr->current_measurements.size(), 2U);
  EXPECT_EQ(policy_ptr->current_measurements[0]->artifact_id, "base");
  EXPECT_EQ(policy_ptr->history_sizes, (std::vector<std::size_t>{1, 2}));
  EXPECT_EQ(host.calls[3], "Begin:1:base");
}

TEST(TimingDrivenPlacementControllerTest,
     PolicyExhaustionBeforeFeasibilityIsFailure) {
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {20.0, 20.0}, "base"), true,
       true},
  });
  TimingDrivenPlacementController controller(
      Config(TimingDrivenBaselineMode::kTrackBestInfeasible), Policy({}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.terminal_status, TimingDrivenTerminalStatus::kFailed);
  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kNoFeasibleSolution);
  EXPECT_EQ(result.convergence_reason,
            TimingDrivenConvergenceReason::kPolicyExhausted);
  EXPECT_FALSE(result.accepted_measurement.has_value());
  ASSERT_TRUE(result.provisional_measurement.has_value());
  EXPECT_EQ(result.provisional_measurement->artifact_id, "base");
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit"}));
}

TEST(TimingDrivenPlacementControllerTest,
     NoImprovementBeforeFeasibilityIsFailure) {
  const auto candidate = Candidate(1);
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {20.0, 20.0}, "base"), true,
       true},
      {true, Measurement(candidate, 90.0, {19.0, 20.0}, "worse"), true, true},
  });
  TimingDrivenPlacementController controller(
      Config(TimingDrivenBaselineMode::kTrackBestInfeasible),
      Policy({candidate}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.terminal_status, TimingDrivenTerminalStatus::kFailed);
  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kNoFeasibleSolution);
  EXPECT_EQ(result.convergence_reason,
            TimingDrivenConvergenceReason::kNoImprovement);
  EXPECT_FALSE(result.accepted_measurement.has_value());
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit",
                                      "Begin:1:base", "Run", "Rollback"}));
}

TEST(TimingDrivenPlacementControllerTest,
     MaxTrialsBeforeFeasibilityIsFailureWithoutPolicyCall) {
  auto config = Config(TimingDrivenBaselineMode::kTrackBestInfeasible);
  config.max_trials = 1;
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {20.0, 20.0}, "base"), true,
       true},
  });
  auto policy = Policy({Candidate(1)});
  auto *policy_ptr = policy.get();
  TimingDrivenPlacementController controller(config, std::move(policy));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.terminal_status, TimingDrivenTerminalStatus::kFailed);
  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kNoFeasibleSolution);
  EXPECT_EQ(result.convergence_reason,
            TimingDrivenConvergenceReason::kMaxTrials);
  EXPECT_TRUE(policy_ptr->history_sizes.empty());
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit"}));
}

TEST(TimingDrivenPlacementControllerTest,
     InvalidControllerConfigurationMakesNoHostCalls) {
  for (const int invalid_case : {0, 1, 2, 3, 4}) {
    auto config = Config();
    if (invalid_case == 0) {
      config.initial_candidate.delay_parameters.clear();
    } else if (invalid_case == 1) {
      config.initial_anchor.clear();
    } else if (invalid_case == 2) {
      config.expected_constraint_count = 0;
    } else if (invalid_case == 3) {
      config.max_trials = 0;
    } else {
      config.require_measurement_metadata = true;
      config.require_lifecycle_invariants = true;
      config.expected_die_grid = {0, 0, 100, 100, 1, 1, true};
      config.expected_rc_min_routing_layer = -1;
    }
    RecordingHost host({});
    TimingDrivenPlacementController controller(config, Policy({Candidate(1)}));

    const auto result = controller.Run(host);

    EXPECT_EQ(result.termination_reason,
              TimingDrivenTerminationReason::kValidationFailure);
    EXPECT_EQ(result.trials_attempted, 0);
    EXPECT_TRUE(host.calls.empty());
  }
}

TEST(TimingDrivenPlacementControllerTest, RejectsNegativePlacementHpwl) {
  auto malformed = Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base");
  malformed.placement_hpwl_um = -0.1;
  RecordingHost host({{true, malformed, true, true}});
  TimingDrivenPlacementController controller(Config(), Policy({}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kValidationFailure);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Rollback"}));
}

TEST(TimingDrivenPlacementControllerTest,
     MinimumPeriodImprovementRequiresConfiguredThreshold) {
  const auto below_threshold = Candidate(1);
  const auto above_threshold = Candidate(2);
  auto config = Config();
  config.minimum_period_improvement_ps = 1.0;
  config.no_improvement_limit = 2;
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base"), true,
       true},
      {true, Measurement(below_threshold, 99.5, {30.0, 31.0}, "small"), true,
       true},
      {true, Measurement(above_threshold, 98.0, {30.0, 31.0}, "large"), true,
       true},
  });
  TimingDrivenPlacementController controller(
      config, Policy({below_threshold, above_threshold}));

  const auto result = controller.Run(host);

  ASSERT_TRUE(result.accepted_measurement.has_value());
  EXPECT_EQ(result.accepted_measurement->artifact_id, "large");
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit",
                                      "Begin:1:base", "Run", "Rollback",
                                      "Begin:2:base", "Run", "Commit"}));
}

TEST(TimingDrivenPlacementControllerTest,
     NoImprovementLimitAllowsConfiguredRejectionsAndPreservesAnchor) {
  const auto first = Candidate(1);
  const auto second = Candidate(2);
  const auto third = Candidate(3);
  auto config = Config();
  config.no_improvement_limit = 2;
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base"), true,
       true},
      {true, Measurement(first, 110.0, {30.0, 31.0}, "first"), true, true},
      {true, Measurement(second, 120.0, {30.0, 31.0}, "second"), true, true},
  });
  auto policy = Policy({first, second, third});
  auto *policy_ptr = policy.get();
  TimingDrivenPlacementController controller(config, std::move(policy));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kNoImprovement);
  EXPECT_EQ(policy_ptr->history_sizes, (std::vector<std::size_t>{1, 2}));
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit",
                                      "Begin:1:base", "Run", "Rollback",
                                      "Begin:2:base", "Run", "Rollback"}));
  ASSERT_TRUE(result.accepted_measurement.has_value());
  EXPECT_EQ(result.accepted_measurement->artifact_id, "base");
}

TEST(TimingDrivenPlacementControllerTest,
     StrictInfeasibleBaselineRollsBackWithoutAcceptedState) {
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {24.0, 30.0}, "infeasible"), true,
       true},
  });
  TimingDrivenPlacementController controller(Config(), Policy({Candidate(1)}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.terminal_status, TimingDrivenTerminalStatus::kFailed);
  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kInfeasibleBaseline);
  EXPECT_FALSE(result.accepted_candidate.has_value());
  EXPECT_FALSE(result.accepted_measurement.has_value());
  EXPECT_TRUE(result.accepted_artifact_id.empty());
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Rollback"}));
}

TEST(TimingDrivenPlacementControllerTest,
     InvalidPolicyCandidateFailsBeforeAnyHostCallForThatCandidate) {
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base"), true,
       true},
  });
  TimingDrivenPlacementController controller(
      Config(), Policy({TimingDrivenPlacementCandidate{}}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.terminal_status, TimingDrivenTerminalStatus::kFailed);
  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kValidationFailure);
  EXPECT_EQ(result.trials_attempted, 1);
  ASSERT_EQ(result.history.size(), 2U);
  EXPECT_EQ(result.history.back().trial_id, 1);
  EXPECT_EQ(result.history.back().decision, TimingDrivenTrialDecision::kFailed);
  EXPECT_NE(result.history.back().reason.find("empty"), std::string::npos);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit"}));
}

TEST(TimingDrivenPlacementControllerTest,
     RuntimeReplacementModeValidatesBothCandidateMaps) {
  auto initial = Candidate(1);
  initial.replacement_processes = {{"site", "process-1"}};
  auto invalid = Candidate(2);
  invalid.replacement_processes = {{"other", "process-2"}};
  auto config = Config();
  config.initial_candidate = initial;
  config.require_replacement_map = true;
  config.expected_delay_site_ids = {"site"};
  RecordingHost host({
      {true, Measurement(initial, 100.0, {30.0, 31.0}, "base"), true, true},
  });
  TimingDrivenPlacementController controller(config, Policy({invalid}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kValidationFailure);
  EXPECT_NE(result.history.back().reason.find("replacement"),
            std::string::npos);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:1:seed", "Run", "Commit"}));
}

TEST(TimingDrivenPlacementControllerTest,
     ObserverReportsBeginFailureAndTerminalTrial) {
  RecordingHost host({{false, std::nullopt, true, true}});
  RecordingObserver observer;
  TimingDrivenPlacementController controller(Config(), Policy({}));

  controller.Run(host, &observer);

  ASSERT_EQ(observer.events.size(), 3U);
  EXPECT_FALSE(observer.events[0].operation_succeeded);
  EXPECT_EQ(observer.events[0].type,
            TimingDrivenPlacementEventType::kTrialBegin);
  EXPECT_EQ(observer.events[2].trial_id, 0);
}

TEST(TimingDrivenPlacementControllerTest,
     ObserverReportsMeasurementFailureAndTerminalTrial) {
  RecordingHost host({{true, std::nullopt, true, true}});
  RecordingObserver observer;
  TimingDrivenPlacementController controller(Config(), Policy({}));

  controller.Run(host, &observer);

  ASSERT_EQ(observer.events.size(), 4U);
  EXPECT_FALSE(observer.events[1].operation_succeeded);
  EXPECT_EQ(observer.events[1].type, TimingDrivenPlacementEventType::kMeasured);
  EXPECT_EQ(observer.events.back().trial_id, 0);
}

TEST(TimingDrivenPlacementControllerTest,
     ObserverReportsValidationFailureAndTerminalTrial) {
  auto malformed = Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base");
  malformed.constraint_count = 1;
  RecordingHost host({{true, malformed, true, true}});
  RecordingObserver observer;
  TimingDrivenPlacementController controller(Config(), Policy({}));

  controller.Run(host, &observer);

  ASSERT_EQ(observer.events.size(), 4U);
  EXPECT_TRUE(observer.events[1].operation_succeeded);
  EXPECT_EQ(observer.events[2].type, TimingDrivenPlacementEventType::kRollback);
  EXPECT_EQ(observer.events.back().trial_id, 0);
}

TEST(TimingDrivenPlacementControllerTest,
     ObserverReportsCommitFailureAndTerminalTrial) {
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base"), false,
       true},
  });
  RecordingObserver observer;
  TimingDrivenPlacementController controller(Config(), Policy({}));

  controller.Run(host, &observer);

  ASSERT_EQ(observer.events.size(), 5U);
  EXPECT_FALSE(observer.events[2].operation_succeeded);
  EXPECT_EQ(observer.events[2].type, TimingDrivenPlacementEventType::kCommit);
  EXPECT_EQ(observer.events.back().trial_id, 0);
}

TEST(TimingDrivenPlacementControllerTest,
     ObserverReportsRollbackFailureAndStops) {
  const auto candidate = Candidate(1);
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base"), true,
       true},
      {true, Measurement(candidate, 110.0, {30.0, 31.0}, "rejected"), true,
       false},
  });
  RecordingObserver observer;
  TimingDrivenPlacementController controller(Config(), Policy({candidate}));

  const auto result = controller.Run(host, &observer);

  ASSERT_EQ(observer.events.size(), 7U);
  EXPECT_FALSE(observer.events[5].operation_succeeded);
  EXPECT_EQ(observer.events[5].type, TimingDrivenPlacementEventType::kRollback);
  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kRollbackFailure);
  EXPECT_EQ(observer.events.back().trial_id, 1);
}

TEST(TimingDrivenPlacementControllerTest,
     ObserverReportsInvalidPolicyCandidateWithoutHostMutation) {
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base"), true,
       true},
  });
  RecordingObserver observer;
  TimingDrivenPlacementController controller(
      Config(), Policy({TimingDrivenPlacementCandidate{}}));

  const auto result = controller.Run(host, &observer);

  ASSERT_EQ(observer.events.size(), 4U);
  EXPECT_EQ(result.history.back().trial_id, 1);
  EXPECT_EQ(observer.events.back().trial_id, 1);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit"}));
}

TEST(TimingDrivenPlacementControllerTest,
     ObserverReportsInvalidInitialConfigWithoutHostCall) {
  auto config = Config();
  config.initial_anchor.clear();
  RecordingHost host({});
  RecordingObserver observer;
  TimingDrivenPlacementController controller(config, Policy({}));

  const auto result = controller.Run(host, &observer);

  ASSERT_EQ(observer.events.size(), 1U);
  EXPECT_EQ(observer.events[0].type, TimingDrivenPlacementEventType::kTerminal);
  EXPECT_EQ(observer.events[0].trial_id, -1);
  EXPECT_EQ(result.trials_attempted, 0);
  EXPECT_TRUE(host.calls.empty());
}

TEST(TimingDrivenPlacementControllerTest,
     ObserverExceptionsAreSuppressedWithoutChangingTransactions) {
  const auto candidate = Candidate(1);
  RecordingHost host({
      {true, Measurement(Candidate(0), 100.0, {30.0, 31.0}, "base"), true,
       true},
      {true, Measurement(candidate, 90.0, {30.0, 31.0}, "improved"), true,
       true},
  });
  ThrowingObserver observer;
  TimingDrivenPlacementController controller(Config(), Policy({candidate}));

  const auto result = controller.Run(host, &observer);

  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kPolicyExhausted);
  EXPECT_EQ(observer.calls, 1);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit",
                                      "Begin:1:base", "Run", "Commit"}));
}

TEST(TimingDrivenPlacementControllerTest,
     EnforcesMetadataLifecycleStaticTopologyAndArtifactLineage) {
  const auto candidate = Candidate(1);
  RecordingHost host({
      {true,
       MetadataMeasurement(Candidate(0), 100.0, {20.0, 20.0}, "A", "seed", "A",
                           "generation-0"),
       true, true},
      {true,
       MetadataMeasurement(candidate, 110.0, {21.0, 20.0}, "B", "A", "B", "A"),
       true, true},
      {true,
       MetadataMeasurement(Candidate(0), 90.0, {19.0, 20.0}, "C", "B", "C",
                           "B"),
       true, true},
  });
  TimingDrivenPlacementController controller(MetadataConfig(),
                                             Policy({candidate, Candidate(0)}));

  const auto result = controller.Run(host);

  ASSERT_TRUE(result.provisional_measurement.has_value());
  EXPECT_EQ(result.provisional_measurement->artifact_id, "B");
  EXPECT_EQ(result.history[2].decision, TimingDrivenTrialDecision::kRejected);
  EXPECT_EQ(host.calls[6], "Begin:0:B");
}

TEST(TimingDrivenPlacementControllerTest,
     RejectsChangedStaticInventoryBeforeAcceptance) {
  const auto candidate = Candidate(1);
  auto changed =
      MetadataMeasurement(candidate, 90.0, {21.0, 20.0}, "B", "A", "B", "A");
  changed.static_inventory_digest = "changed";
  RecordingHost host({
      {true,
       MetadataMeasurement(Candidate(0), 100.0, {20.0, 20.0}, "A", "seed", "A",
                           "generation-0"),
       true, true},
      {true, changed, true, true},
  });
  TimingDrivenPlacementController controller(MetadataConfig(),
                                             Policy({candidate}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kValidationFailure);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Commit",
                                      "Begin:1:A", "Run", "Rollback"}));
}

TEST(TimingDrivenPlacementControllerTest,
     RejectsEachIndependentMetadataInvariantBeforeAcceptance) {
  struct MetadataMutation {
    const char *name;
    const char *reason;
    std::function<void(TimingDrivenPlacementMeasurement &)> apply;
  };
  const std::vector<MetadataMutation> mutations = {
      {"changed I/O digest", "I/O inventory",
       [](auto &measurement) { measurement.io_inventory_digest = "io-B"; }},
      {"same-count changed constraint ID set", "constraint id set",
       [](auto &measurement) {
         measurement.constraints[1].constraint_id = 2;
         measurement.constraint_id_digest = "ids:0,2,";
       }},
      {"inconsistent host constraint digest", "constraint id digest",
       [](auto &measurement) {
         measurement.constraint_id_digest = "ids:0,2,";
       }},
      {"die/grid mismatch", "lifecycle metadata",
       [](auto &measurement) { measurement.die_grid.die_urx = 101.0; }},
      {"timing_use_rc mismatch", "lifecycle metadata",
       [](auto &measurement) { measurement.timing_use_rc = false; }},
      {"routing-layer mismatch", "lifecycle metadata",
       [](auto &measurement) { measurement.rc_min_routing_layer = 2; }},
      {"empty topology identity", "incomplete artifact metadata",
       [](auto &measurement) { measurement.topology_identity.clear(); }},
      {"reused later artifact id", "artifact id is not unique",
       [](auto &measurement) { measurement.artifact_id = "A"; }},
      {"reused later generation id", "generation id is not unique",
       [](auto &measurement) { measurement.generation_id = "A"; }},
      {"wrong artifact parent", "artifact parent",
       [](auto &measurement) { measurement.artifact_parent_id = "seed"; }},
      {"wrong generation parent", "generation parent",
       [](auto &measurement) {
         measurement.generation_parent_id = "generation-0";
       }},
  };

  for (const MetadataMutation &mutation : mutations) {
    auto trial = MetadataMeasurement(Candidate(1), 110.0, {21.0, 20.0}, "B",
                                     "A", "B", "A");
    mutation.apply(trial);
    RecordingHost host({
        {true,
         MetadataMeasurement(Candidate(0), 100.0, {20.0, 20.0}, "A", "seed",
                             "A", "generation-0"),
         true, true},
        {true, trial, true, true},
    });
    TimingDrivenPlacementController controller(MetadataConfig(),
                                                Policy({Candidate(1)}));

    const auto result = controller.Run(host);

    EXPECT_EQ(result.termination_reason,
              TimingDrivenTerminationReason::kValidationFailure)
        << mutation.name;
    EXPECT_TRUE(result.accepted_artifact_id.empty()) << mutation.name;
    EXPECT_EQ(result.provisional_artifact_id, "A") << mutation.name;
    ASSERT_EQ(result.history.size(), 2U) << mutation.name;
    EXPECT_EQ(result.history.back().decision,
              TimingDrivenTrialDecision::kFailed)
        << mutation.name;
    EXPECT_NE(result.history.back().reason.find(mutation.reason),
              std::string::npos)
        << mutation.name << ": " << result.history.back().reason;
    EXPECT_EQ(host.calls,
              (std::vector<std::string>{"Begin:0:seed", "Run", "Commit",
                                        "Begin:1:A", "Run", "Rollback"}))
        << mutation.name;
  }
}

TEST(TimingDrivenPlacementControllerTest,
     RejectsSeedSelfLineageAndReusedGenerationRoot) {
  auto config = MetadataConfig();
  const auto self_parent =
      MetadataMeasurement(Candidate(0), 100.0, {30.0, 31.0}, "seed", "seed",
                          "generation-0", "generation-0");
  RecordingHost host({{true, self_parent, true, true}});
  TimingDrivenPlacementController controller(config, Policy({}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kValidationFailure);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"Begin:0:seed", "Run", "Rollback"}));
}

} // namespace dali
