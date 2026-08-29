/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 *******************************************************************************/

#include "dali/placer/well_legalizer/gridded_placement_validator.h"
#include "dali/timing/timing_driven_candidate_policy.h"
#include "dali/timing/timing_driven_placement_config.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace dali {
namespace timing_driven_placement_config_test {

TimingDrivenPlacementCandidate Candidate(int value) {
  TimingDrivenPlacementCandidate candidate;
  candidate.delay_parameters = {{"dl0", value}, {"dl1", value + 1}};
  candidate.replacement_processes = {
      {"dl0", "plain_delay<" + std::to_string(value) + ">"},
      {"dl1", "plain_delay<" + std::to_string(value + 1) + ">"},
  };
  return candidate;
}

TimingDrivenPlacementConfig Config() {
  TimingDrivenPlacementConfig config;
  config.policy.delay_site_ids = {"dl0", "dl1"};
  config.policy.controller.initial_candidate = Candidate(7);
  config.policy.controller.expected_constraint_count = 512;
  config.policy.controller.required_slack_margin_ps = 25.0;
  config.policy.controller.max_trials = 4;
  config.policy.controller.no_improvement_limit = 2;
  config.policy.controller.minimum_period_improvement_ps = 0.0;
  config.policy.controller.feasibility_merit_tolerance_ps = 1e-6;
  config.policy.controller.initial_anchor = "seed";
  config.policy.controller.baseline_mode =
      TimingDrivenBaselineMode::kTrackBestInfeasible;
  config.policy.controller.require_replacement_map = true;
  config.policy.controller.expected_delay_site_ids =
      config.policy.delay_site_ids;
  config.policy.controller.require_measurement_metadata = true;
  config.policy.candidate_sequence = {Candidate(9), Candidate(7)};
  config.lifecycle.liberty_path = "/tmp/reference.lib";
  config.lifecycle.tech_config_path = "/tmp/tech.rules";
  config.lifecycle.placement_recipe_path = "/tmp/placement.dali";
  config.lifecycle.timing_use_rc = true;
  config.lifecycle.rc_min_routing_layer = 1;
  config.lifecycle.target_density = 0.70;
  config.lifecycle.num_threads = 1;
  config.lifecycle.is_standard_cell = false;
  config.lifecycle.well_emit_mode = 0;
  config.lifecycle.enable_well_taps = true;
  config.lifecycle.fixed_die_grid = {0, 0, 100000, 100000, 600, 300, true};
  return config;
}

std::filesystem::path TempPath(const std::string &suffix) {
  return std::filesystem::temp_directory_path() /
         ("dali_timing_driven_" + suffix + ".tdp");
}

bool WriteText(const std::filesystem::path &path, const std::string &text) {
  std::ofstream output(path);
  output << text;
  return static_cast<bool>(output);
}

std::string SerializedConfig() {
  const std::filesystem::path path = TempPath("serialized");
  std::string error;
  EXPECT_TRUE(WriteTimingDrivenPlacementConfig(path.string(), Config(), &error))
      << error;
  std::ifstream input(path);
  std::string text((std::istreambuf_iterator<char>(input)), {});
  std::filesystem::remove(path);
  return text;
}

bool LoadsText(const std::string &suffix, const std::string &text) {
  const std::filesystem::path path = TempPath(suffix);
  EXPECT_TRUE(WriteText(path, text));
  TimingDrivenPlacementConfig output;
  std::string error;
  const bool loaded =
      LoadTimingDrivenPlacementConfig(path.string(), &output, &error);
  std::filesystem::remove(path);
  return loaded;
}

std::string RemoveLineWithPrefix(std::string text, const std::string &prefix) {
  const std::size_t start = text.find(prefix);
  if (start == std::string::npos)
    return text;
  const std::size_t end = text.find('\n', start);
  text.erase(start,
             end == std::string::npos ? text.size() - start : end + 1 - start);
  return text;
}

TimingDrivenPlacementMeasurement
Measurement(const TimingDrivenPlacementCandidate &candidate, double period,
            std::initializer_list<double> slacks, const std::string &artifact) {
  TimingDrivenPlacementMeasurement measurement;
  measurement.delay_parameters = candidate.delay_parameters;
  measurement.replacement_processes = candidate.replacement_processes;
  measurement.period_ps = period;
  measurement.wns_ps = *std::min_element(slacks.begin(), slacks.end());
  measurement.tns_ps = 0.0;
  measurement.constraint_count = static_cast<int>(slacks.size());
  int id = 0;
  for (double slack : slacks)
    measurement.constraints.push_back({id++, "site", slack});
  measurement.placement_hpwl_um = 1.0;
  measurement.placement_legal = true;
  measurement.overlap_count = 0;
  measurement.artifact_id = artifact;
  return measurement;
}

class EventHost final : public TimingDrivenFlowHost {
public:
  explicit EventHost(std::vector<TimingDrivenPlacementMeasurement> measurements)
      : measurements_(std::move(measurements)) {}

  bool BeginTrial(const TimingDrivenPlacementCandidate &candidate,
                  const std::string &anchor) override {
    calls.push_back(
        "begin:" + std::to_string(candidate.delay_parameters.at("site")) + ":" +
        anchor);
    current_ = next_++;
    return true;
  }

  std::optional<TimingDrivenPlacementMeasurement>
  RunPlacementAndTiming() override {
    calls.push_back("measure");
    return measurements_.at(current_);
  }

  bool CommitTrial() override {
    calls.push_back("commit");
    return true;
  }

  bool RollbackTrial() override {
    calls.push_back("rollback");
    return true;
  }

  std::vector<std::string> calls;

private:
  std::vector<TimingDrivenPlacementMeasurement> measurements_;
  std::size_t next_ = 0;
  std::size_t current_ = 0;
};

class EventCollector final : public TimingDrivenPlacementEventObserver {
public:
  void OnTimingDrivenPlacementEvent(
      const TimingDrivenPlacementEvent &event) override {
    events.push_back(event);
  }

  std::vector<TimingDrivenPlacementEvent> events;
};

} // namespace timing_driven_placement_config_test

using timing_driven_placement_config_test::Candidate;
using timing_driven_placement_config_test::Config;
using timing_driven_placement_config_test::EventCollector;
using timing_driven_placement_config_test::EventHost;
using timing_driven_placement_config_test::LoadsText;
using timing_driven_placement_config_test::Measurement;
using timing_driven_placement_config_test::RemoveLineWithPrefix;
using timing_driven_placement_config_test::SerializedConfig;
using timing_driven_placement_config_test::TempPath;
using timing_driven_placement_config_test::WriteText;

TEST(TimingDrivenPlacementConfigTest, RoundTripPreservesBothConfigDomains) {
  const std::filesystem::path path = TempPath("round_trip");
  const auto input = Config();
  std::string error;
  ASSERT_TRUE(WriteTimingDrivenPlacementConfig(path.string(), input, &error))
      << error;

  TimingDrivenPlacementConfig output;
  ASSERT_TRUE(LoadTimingDrivenPlacementConfig(path.string(), &output, &error))
      << error;
  EXPECT_EQ(output.policy.delay_site_ids, input.policy.delay_site_ids);
  EXPECT_EQ(output.policy.candidate_sequence.size(), 2U);
  EXPECT_EQ(output.policy.controller.initial_candidate.delay_parameters,
            input.policy.controller.initial_candidate.delay_parameters);
  EXPECT_EQ(output.policy.controller.initial_candidate.replacement_processes,
            input.policy.controller.initial_candidate.replacement_processes);
  EXPECT_EQ(output.policy.controller.initial_generation_id,
            input.policy.controller.initial_generation_id);
  EXPECT_EQ(output.lifecycle.liberty_path, input.lifecycle.liberty_path);
  EXPECT_EQ(output.lifecycle.tech_config_path,
            input.lifecycle.tech_config_path);
  EXPECT_EQ(output.lifecycle.placement_recipe_path,
            input.lifecycle.placement_recipe_path);
  EXPECT_TRUE(output.lifecycle.timing_use_rc);
  EXPECT_TRUE(output.lifecycle.fixed_die_grid.valid);
  EXPECT_DOUBLE_EQ(output.lifecycle.fixed_die_grid.grid_y, 300.0);
  EXPECT_TRUE(output.policy.controller.require_lifecycle_invariants);
  EXPECT_TRUE(output.policy.controller.expected_timing_use_rc);
  EXPECT_EQ(output.policy.controller.expected_rc_min_routing_layer, 1);
  EXPECT_DOUBLE_EQ(output.policy.controller.expected_die_grid.die_urx,
                   output.lifecycle.fixed_die_grid.die_urx);
  EXPECT_DOUBLE_EQ(output.policy.controller.expected_die_grid.grid_x,
                   output.lifecycle.fixed_die_grid.grid_x);
  std::filesystem::remove(path);
}

TEST(TimingDrivenPlacementConfigTest, ResolvesRelativeLifecyclePaths) {
  const std::filesystem::path directory =
      std::filesystem::temp_directory_path() / "dali_timing_driven_paths";
  std::filesystem::create_directories(directory);
  const std::filesystem::path path = directory / "config.tdp";
  ASSERT_TRUE(WriteText(path, "format_version 1\n"
                              "policy.expected_constraint_count 2\n"
                              "policy.required_slack_margin_ps 25\n"
                              "policy.max_trials 2\n"
                              "policy.no_improvement_limit 1\n"
                              "policy.minimum_period_improvement_ps 0\n"
                              "policy.feasibility_merit_tolerance_ps 0.001\n"
                              "policy.baseline_mode require_feasible\n"
                              "policy.initial_anchor seed\n"
                              "policy.initial_generation_id seed-generation\n"
                              "policy.require_measurement_metadata false\n"
                              "policy.delay_site dl0\n"
                              "policy.initial.parameter dl0 7\n"
                              "policy.initial.replacement dl0 p7\n"
                              "policy.candidate 1 parameter dl0 8\n"
                              "policy.candidate 1 replacement dl0 p8\n"
                              "lifecycle.liberty_path lib/ref.lib\n"
                              "lifecycle.tech_config_path tech/rules\n"
                              "lifecycle.placement_recipe_path recipes/p.dali\n"
                              "lifecycle.timing_use_rc false\n"
                              "lifecycle.rc_min_routing_layer 0\n"
                              "lifecycle.target_density 0.7\n"
                              "lifecycle.num_threads 1\n"
                              "lifecycle.is_standard_cell false\n"
                              "lifecycle.well_emit_mode 0\n"
                              "lifecycle.enable_well_taps true\n"
                              "lifecycle.fixed_die 0 0 10 10 0.6 0.3\n"));
  TimingDrivenPlacementConfig config;
  std::string error;
  ASSERT_TRUE(LoadTimingDrivenPlacementConfig(path.string(), &config, &error))
      << error;
  EXPECT_EQ(config.lifecycle.liberty_path,
            (directory / "lib/ref.lib").lexically_normal().string());
  EXPECT_EQ(config.lifecycle.tech_config_path,
            (directory / "tech/rules").lexically_normal().string());
  std::filesystem::remove(path);
  std::filesystem::remove(directory);
}

TEST(TimingDrivenPlacementConfigTest,
     WriterResolvesRelativePathsAndSharesValidation) {
  const std::filesystem::path directory =
      std::filesystem::temp_directory_path() / "dali_timing_driven_writer";
  std::filesystem::create_directories(directory);
  const std::filesystem::path path = directory / "config.tdp";
  auto input = Config();
  input.lifecycle.liberty_path = "lib/ref.lib";
  input.lifecycle.tech_config_path = "tech/rules";
  input.lifecycle.placement_recipe_path = "recipes/p.dali";
  std::string error;
  ASSERT_TRUE(WriteTimingDrivenPlacementConfig(path.string(), input, &error))
      << error;
  TimingDrivenPlacementConfig output;
  ASSERT_TRUE(LoadTimingDrivenPlacementConfig(path.string(), &output, &error))
      << error;
  EXPECT_EQ(output.lifecycle.liberty_path,
            (directory / "lib/ref.lib").lexically_normal().string());

  input.lifecycle.target_density = 1.1;
  EXPECT_FALSE(WriteTimingDrivenPlacementConfig(path.string(), input, &error));
  input = Config();
  input.policy.controller.initial_candidate.delay_parameters["dl0"] = 0;
  EXPECT_FALSE(WriteTimingDrivenPlacementConfig(path.string(), input, &error));
  std::filesystem::remove(path);
  std::filesystem::remove(directory);
}

TEST(TimingDrivenPlacementConfigTest, RejectsUnknownDuplicateAndMalformedKeys) {
  const std::filesystem::path base_path = TempPath("invalid");
  std::string serialized;
  const auto input = Config();
  const std::filesystem::path valid_path = TempPath("valid");
  std::string error;
  ASSERT_TRUE(
      WriteTimingDrivenPlacementConfig(valid_path.string(), input, &error));
  {
    std::ifstream source(valid_path);
    serialized.assign(std::istreambuf_iterator<char>(source), {});
  }
  const std::vector<std::string> invalid_lines = {
      "unknown.key 1\n", "policy.max_trials 9\n", "policy.delay_site dl0\n",
      "policy.candidate 1 parameter dl0\n"};
  for (std::size_t index = 0; index < invalid_lines.size(); ++index) {
    ASSERT_TRUE(WriteText(base_path, serialized + invalid_lines[index]));
    TimingDrivenPlacementConfig output;
    EXPECT_FALSE(
        LoadTimingDrivenPlacementConfig(base_path.string(), &output, &error))
        << invalid_lines[index];
  }
  std::filesystem::remove(valid_path);
  std::filesystem::remove(base_path);
}

TEST(TimingDrivenPlacementConfigTest, RejectsMissingOrUndeclaredReplacement) {
  const std::filesystem::path path = TempPath("replacement");
  const auto input = Config();
  std::string error;
  ASSERT_TRUE(WriteTimingDrivenPlacementConfig(path.string(), input, &error));
  std::ifstream source(path);
  std::string text((std::istreambuf_iterator<char>(source)), {});
  const std::string replacement =
      "policy.initial.replacement dl1 plain_delay<8>\n";
  const std::size_t position = text.find(replacement);
  ASSERT_NE(position, std::string::npos);
  text.erase(position, replacement.size());
  ASSERT_TRUE(WriteText(path, text));
  TimingDrivenPlacementConfig output;
  EXPECT_FALSE(LoadTimingDrivenPlacementConfig(path.string(), &output, &error));
  std::filesystem::remove(path);
}

TEST(TimingDrivenPlacementConfigTest, RejectsParserBoundaryCases) {
  const std::string valid = SerializedConfig();
  const std::vector<std::string> required_scalar_prefixes = {
      "format_version",
      "policy.expected_constraint_count",
      "policy.required_slack_margin_ps",
      "policy.max_trials",
      "policy.no_improvement_limit",
      "policy.minimum_period_improvement_ps",
      "policy.feasibility_merit_tolerance_ps",
      "policy.baseline_mode",
      "policy.initial_anchor",
      "policy.initial_generation_id",
      "policy.require_measurement_metadata",
      "lifecycle.liberty_path",
      "lifecycle.tech_config_path",
      "lifecycle.placement_recipe_path",
      "lifecycle.timing_use_rc",
      "lifecycle.rc_min_routing_layer",
      "lifecycle.target_density",
      "lifecycle.num_threads",
      "lifecycle.is_standard_cell",
      "lifecycle.well_emit_mode",
      "lifecycle.enable_well_taps",
      "lifecycle.fixed_die",
  };
  for (std::size_t index = 0; index < required_scalar_prefixes.size();
       ++index) {
    EXPECT_FALSE(
        LoadsText("missing_scalar_" + std::to_string(index),
                  RemoveLineWithPrefix(valid, required_scalar_prefixes[index])))
        << required_scalar_prefixes[index];
  }
  const std::vector<std::pair<std::string, std::string>> invalid = {
      {"missing delay site", RemoveLineWithPrefix(valid, "policy.delay_site")},
      {"missing initial parameter",
       RemoveLineWithPrefix(valid, "policy.initial.parameter")},
      {"missing initial replacement",
       RemoveLineWithPrefix(valid, "policy.initial.replacement")},
      {"missing candidate parameter",
       RemoveLineWithPrefix(valid, "policy.candidate 1 parameter")},
      {"missing candidate replacement",
       RemoveLineWithPrefix(valid, "policy.candidate 1 replacement")},
      {"noncontiguous candidate index",
       [&]() {
         std::string text = valid;
         const std::string from = "policy.candidate 1 ";
         std::size_t position = 0;
         while ((position = text.find(from, position)) != std::string::npos) {
           text.replace(position, from.size(), "policy.candidate 2 ");
           position += from.size();
         }
         return text;
       }()},
      {"duplicate parameter", valid + "policy.initial.parameter dl0 8\n"},
      {"duplicate replacement",
       valid + "policy.initial.replacement dl0 duplicate\n"},
      {"duplicate site", valid + "policy.delay_site dl0\n"},
      {"undeclared initial parameter",
       valid + "policy.initial.parameter dl9 8\n"},
      {"undeclared candidate parameter",
       valid + "policy.candidate 1 parameter dl9 8\n"},
      {"NaN density", RemoveLineWithPrefix(valid, "lifecycle.target_density") +
                          "lifecycle.target_density nan\n"},
      {"infinite die", RemoveLineWithPrefix(valid, "lifecycle.fixed_die") +
                           "lifecycle.fixed_die 0 0 inf 100000 600 300\n"},
      {"invalid die grid", RemoveLineWithPrefix(valid, "lifecycle.fixed_die") +
                               "lifecycle.fixed_die 0 0 100000 100000 0 300\n"},
      {"invalid boolean",
       RemoveLineWithPrefix(valid, "lifecycle.timing_use_rc") +
           "lifecycle.timing_use_rc maybe\n"},
      {"invalid well emit mode",
       RemoveLineWithPrefix(valid, "lifecycle.well_emit_mode") +
           "lifecycle.well_emit_mode 3\n"},
      {"invalid delay parameter",
       RemoveLineWithPrefix(valid, "policy.initial.parameter") +
           "policy.initial.parameter dl0 0\n"},
  };
  for (std::size_t index = 0; index < invalid.size(); ++index)
    EXPECT_FALSE(
        LoadsText("boundary_" + std::to_string(index), invalid[index].second))
        << invalid[index].first;
}

TEST(TimingDrivenPlacementConfigTest, EmitsExactControllerEventSequence) {
  TimingDrivenPlacementCandidate a = Candidate(0);
  a.delay_parameters = {{"site", 0}};
  a.replacement_processes.clear();
  TimingDrivenPlacementCandidate b = Candidate(1);
  b.delay_parameters = {{"site", 1}};
  b.replacement_processes.clear();
  TimingDrivenPlacementControllerConfig config;
  config.initial_candidate = a;
  config.initial_anchor = "seed";
  config.expected_constraint_count = 2;
  config.required_slack_margin_ps = 25.0;
  config.max_trials = 4;
  config.no_improvement_limit = 2;
  config.baseline_mode = TimingDrivenBaselineMode::kTrackBestInfeasible;
  EventHost host({Measurement(a, 100.0, {0.0, 0.0}, "A"),
                  Measurement(b, 110.0, {10.0, 10.0}, "B"),
                  Measurement(a, 90.0, {0.0, 0.0}, "A2")});
  EventCollector observer;
  TimingDrivenPlacementController controller(
      config, std::make_unique<DeterministicTimingDrivenCandidatePolicy>(
                  std::vector<TimingDrivenPlacementCandidate>{b, a}));

  const auto result = controller.Run(host, &observer);

  ASSERT_EQ(result.history.size(), 3U);
  EXPECT_EQ(result.history[0].decision,
            TimingDrivenTrialDecision::kBestInfeasible);
  EXPECT_EQ(result.history[1].decision,
            TimingDrivenTrialDecision::kBestInfeasible);
  EXPECT_EQ(result.history[2].decision, TimingDrivenTrialDecision::kRejected);
  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kNoFeasibleSolution);
  EXPECT_EQ(result.convergence_reason,
            TimingDrivenConvergenceReason::kPolicyExhausted);
  ASSERT_EQ(observer.events.size(), 10U);
  EXPECT_EQ(observer.events[0].type,
            TimingDrivenPlacementEventType::kTrialBegin);
  EXPECT_EQ(observer.events[1].type, TimingDrivenPlacementEventType::kMeasured);
  EXPECT_EQ(observer.events[2].type, TimingDrivenPlacementEventType::kCommit);
  EXPECT_EQ(observer.events[3].type,
            TimingDrivenPlacementEventType::kTrialBegin);
  EXPECT_EQ(observer.events[4].type, TimingDrivenPlacementEventType::kMeasured);
  EXPECT_EQ(observer.events[5].type, TimingDrivenPlacementEventType::kCommit);
  EXPECT_EQ(observer.events[6].type,
            TimingDrivenPlacementEventType::kTrialBegin);
  EXPECT_EQ(observer.events[7].type, TimingDrivenPlacementEventType::kMeasured);
  EXPECT_EQ(observer.events[8].type, TimingDrivenPlacementEventType::kRollback);
  EXPECT_EQ(observer.events[9].type, TimingDrivenPlacementEventType::kTerminal);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"begin:0:seed", "measure", "commit",
                                      "begin:1:A", "measure", "commit",
                                      "begin:0:B", "measure", "rollback"}));
}

TEST(TimingDrivenPlacementConfigTest, RejectsAnyCategorizedLegalityViolation) {
  TimingDrivenPlacementCandidate candidate;
  candidate.delay_parameters = {{"site", 0}};
  TimingDrivenPlacementMeasurement measurement =
      Measurement(candidate, 100.0, {30.0, 30.0}, "A");
  measurement.legality.physical_completion_violation_count = 1;
  EventHost host({measurement});
  TimingDrivenPlacementControllerConfig config;
  config.initial_candidate = candidate;
  config.expected_constraint_count = 2;
  config.max_trials = 1;
  config.no_improvement_limit = 1;
  TimingDrivenPlacementController controller(
      config, std::make_unique<DeterministicTimingDrivenCandidatePolicy>(
                  std::vector<TimingDrivenPlacementCandidate>{}));

  const auto result = controller.Run(host);

  EXPECT_EQ(result.termination_reason,
            TimingDrivenTerminationReason::kValidationFailure);
  EXPECT_EQ(host.calls,
            (std::vector<std::string>{"begin:0:seed", "measure", "rollback"}));
}

TEST(TimingDrivenPlacementConfigTest, ConvertsEveryLegalityField) {
  GriddedPlacementLegalityReport report;
  report.movable_component_count = 1;
  report.assigned_component_count = 2;
  report.unassigned_component_count = 3;
  report.duplicate_assignment_count = 4;
  report.invalid_component_reference_count = 5;
  report.row_boundary_violation_count = 6;
  report.row_overlap_count = 7;
  report.component_boundary_violation_count = 8;
  report.component_overlap_count = 9;
  report.component_y_violation_count = 10;
  report.component_orientation_violation_count = 11;
  report.physical_completion_violation_count = 12;
  report.missing_well_tap_count = 13;
  report.well_tap_geometry_violation_count = 14;
  report.well_tap_spacing_violation_count = 15;
  report.missing_end_cap_count = 16;
  report.end_cap_geometry_violation_count = 17;
  report.end_cap_tap_overlap_count = 18;
  report.physical_component_count_violation_count = 19;
  report.well_tap_coverage_violation_count = 20;
  report.max_well_tap_coverage_gap = 21.5;

  const TimingDrivenPlacementLegalitySummary summary =
      ToTimingDrivenPlacementLegalitySummary(report);
  EXPECT_EQ(summary.movable_component_count, 1U);
  EXPECT_EQ(summary.assigned_component_count, 2U);
  EXPECT_EQ(summary.unassigned_component_count, 3U);
  EXPECT_EQ(summary.duplicate_assignment_count, 4U);
  EXPECT_EQ(summary.invalid_component_reference_count, 5U);
  EXPECT_EQ(summary.row_boundary_violation_count, 6U);
  EXPECT_EQ(summary.row_overlap_count, 7U);
  EXPECT_EQ(summary.component_boundary_violation_count, 8U);
  EXPECT_EQ(summary.component_overlap_count, 9U);
  EXPECT_EQ(summary.component_y_violation_count, 10U);
  EXPECT_EQ(summary.component_orientation_violation_count, 11U);
  EXPECT_EQ(summary.physical_completion_violation_count, 12U);
  EXPECT_EQ(summary.missing_well_tap_count, 13U);
  EXPECT_EQ(summary.well_tap_geometry_violation_count, 14U);
  EXPECT_EQ(summary.well_tap_spacing_violation_count, 15U);
  EXPECT_EQ(summary.missing_end_cap_count, 16U);
  EXPECT_EQ(summary.end_cap_geometry_violation_count, 17U);
  EXPECT_EQ(summary.end_cap_tap_overlap_count, 18U);
  EXPECT_EQ(summary.physical_component_count_violation_count, 19U);
  EXPECT_EQ(summary.well_tap_coverage_violation_count, 20U);
  EXPECT_DOUBLE_EQ(summary.max_well_tap_coverage_gap, 21.5);
}

TEST(TimingDrivenPlacementConfigTest,
     LegalitySummaryDoesNotDoubleCountPhysicalDiagnostics) {
  TimingDrivenPlacementLegalitySummary summary;
  summary.physical_completion_violation_count = 2;
  summary.missing_well_tap_count = 3;
  summary.well_tap_coverage_violation_count = 4;

  EXPECT_EQ(summary.TotalViolationCount(), 2U);
  EXPECT_FALSE(summary.IsLegal());

  summary.physical_completion_violation_count = 0;
  EXPECT_EQ(summary.TotalViolationCount(), 0U);
  EXPECT_FALSE(summary.IsLegal());
}

} // namespace dali
