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
#include "dali/command/dali_command_processor.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "dali/common/act_config.h"
#include "dali/dali.h"

using testing::Test;

class DaliCommandProcessorTest : public Test {
 protected:
  void SetUp() override { config_clear(); }
  void TearDown() override { config_clear(); }
};

TEST_F(DaliCommandProcessorTest, TokenizesQuotesEscapesAndComments) {
  std::vector<std::string> arguments;
  std::string error;

  EXPECT_TRUE(dali::DaliCommandProcessor::TokenizeCommandLine(
      R"(place-io -group "Metal 4" left data\ 0 'data 1' # explanation)",
      &arguments, &error));
  EXPECT_EQ(arguments,
            (std::vector<std::string>{"place-io", "-group", "Metal 4", "left",
                                      "data 0", "data 1"}));

  EXPECT_TRUE(dali::DaliCommandProcessor::TokenizeCommandLine(
      R"(set label "")", &arguments, &error));
  EXPECT_EQ(arguments, (std::vector<std::string>{"set", "label", ""}));
}

TEST_F(DaliCommandProcessorTest, RejectsMalformedCommandLines) {
  std::vector<std::string> arguments;
  std::string error;

  EXPECT_FALSE(dali::DaliCommandProcessor::TokenizeCommandLine(
      R"(set target_density "0.7)", &arguments, &error));
  EXPECT_EQ(error, "unterminated quoted argument");

  EXPECT_FALSE(dali::DaliCommandProcessor::TokenizeCommandLine(
      "set target_density 0.7\\", &arguments, &error));
  EXPECT_EQ(error, "line ends with an incomplete escape");
}

TEST_F(DaliCommandProcessorTest, AppliesTypedRuntimeSettings) {
  dali::Dali placer(nullptr, dali::severity::info);

  EXPECT_TRUE(placer.ExecuteCommand({"set", "target_density", "0.73"}));
  EXPECT_TRUE(placer.ExecuteCommand({"set", "timing_period_target", "29000"}));
  EXPECT_TRUE(placer.ExecuteCommand({"dali:set", "num_threads", "6"}));
  EXPECT_TRUE(placer.ExecuteCommand({"set", "disable_io_place", "true"}));
  EXPECT_TRUE(
      placer.ExecuteCommand({"set", "global_initializer", "density_aware"}));
  EXPECT_TRUE(placer.ExecuteCommand({"set", "output_name", "placed.def"}));
  EXPECT_TRUE(placer.ExecuteCommand({"set", "well_emit_mode", "0"}));

  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();
  EXPECT_DOUBLE_EQ(options.target_density, 0.73);
  EXPECT_DOUBLE_EQ(options.timing_period_target, 29000.0);
  EXPECT_EQ(options.num_threads, 6);
  EXPECT_TRUE(options.disable_io_place);
  EXPECT_EQ(options.global_initializer,
            dali::PlacementInitializerType::kDensityAware);
  EXPECT_EQ(options.well_emit_mode, 0);
  EXPECT_EQ(std::filesystem::path(options.output_name).filename(), "placed");

  EXPECT_FALSE(placer.ExecuteCommand({"set", "target_density", "1.1"}));
  EXPECT_FALSE(placer.ExecuteCommand({"set", "timing_period_target", "0"}));
  EXPECT_FALSE(placer.ExecuteCommand({"set", "num_threads", "many"}));
  EXPECT_FALSE(placer.ExecuteCommand({"set", "unknown_option", "1"}));
  EXPECT_FALSE(placer.ExecuteCommand({"set", "well_emit_mode", "3"}));
  placer.Close();
}

TEST_F(DaliCommandProcessorTest, ResponseTableKeepsOneOrderedAnchor) {
  dali::Dali placer(nullptr, dali::severity::info);

  EXPECT_TRUE(placer.ExecuteCommand(
      {"delay-line-response", "dl3", "7", "10", "460.0"}));
  EXPECT_TRUE(placer.ExecuteCommand(
      {"delay-line-response", "dl3", "7", "12", "760.0"}));
  EXPECT_FALSE(placer.ExecuteCommand(
      {"delay-line-response", "dl3", "8", "14", "900.0"}));
  EXPECT_FALSE(placer.ExecuteCommand(
      {"delay-line-response", "dl3", "7", "12", "900.0"}));
  EXPECT_FALSE(placer.ExecuteCommand(
      {"delay-line-response", "dl3", "7", "14", "700.0"}));
  placer.Close();
}

// The checkpoint schedule is set from the recipe, never from the environment,
// so a run is reproducible from the file that describes it. Each field is
// validated rather than clamped: a schedule silently corrected to something
// legal would place a checkpoint somewhere nobody asked for.
// The RC estimator captures the minimum routing layer when it is built, and
// anything that measures timing before the recipe runs builds one. Without
// propagation the setting is silently ignored for the rest of the run and every
// net is charged to the default layer -- `li` on sky130, at roughly fifty times
// met1's sheet resistance. That cost two flows a 7 ns disagreement about the
// same design at the same size, one reported as characterization evidence.
TEST_F(DaliCommandProcessorTest, RoutingLayerReachesAnAlreadyBuiltEstimator) {
  dali::Dali placer(nullptr, dali::severity::info);
  // Build the estimator first, as an early timing capture would.
  placer.InitializeRCEstimatorForTesting();
  ASSERT_NE(placer.RcEstimatorForTesting(), nullptr);
  EXPECT_EQ(placer.RcEstimatorForTesting()->MinRoutingLayer(), 0);

  EXPECT_TRUE(placer.ExecuteCommand({"set", "rc_min_routing_layer", "1"}));
  EXPECT_EQ(placer.RcEstimatorForTesting()->MinRoutingLayer(), 1)
      << "the setting did not reach the estimator that already existed";
  EXPECT_EQ(placer.EffectiveRcRoutingLayer(), 1);
  EXPECT_TRUE(placer.VerifyRcConfigurationIsEffective("test"));
  placer.Close();
}

// The other orderings. The bug was invisible under the one the oracle flow
// happens to use, so covering only that ordering would be covering the case
// that already worked.
TEST_F(DaliCommandProcessorTest, RoutingLayerSetBeforeAnyEstimatorExists) {
  dali::Dali placer(nullptr, dali::severity::info);
  EXPECT_EQ(placer.EffectiveRcRoutingLayer(), -1);
  EXPECT_TRUE(placer.ExecuteCommand({"set", "rc_min_routing_layer", "1"}));
  placer.InitializeRCEstimatorForTesting();
  EXPECT_EQ(placer.EffectiveRcRoutingLayer(), 1);
  EXPECT_TRUE(placer.VerifyRcConfigurationIsEffective("test"));
  placer.Close();
}

TEST_F(DaliCommandProcessorTest, RepeatedAndChangedRoutingLayersStayEffective) {
  dali::Dali placer(nullptr, dali::severity::info);
  placer.InitializeRCEstimatorForTesting();
  EXPECT_TRUE(placer.ExecuteCommand({"set", "rc_min_routing_layer", "1"}));
  EXPECT_TRUE(placer.ExecuteCommand({"set", "rc_min_routing_layer", "1"}));
  EXPECT_EQ(placer.EffectiveRcRoutingLayer(), 1);
  // 1 -> 2 -> back to 0: a propagation that only ever raised the layer, or only
  // ever fired once, would pass the first of these and fail the rest.
  EXPECT_TRUE(placer.ExecuteCommand({"set", "rc_min_routing_layer", "2"}));
  EXPECT_EQ(placer.EffectiveRcRoutingLayer(), 2);
  EXPECT_TRUE(placer.ExecuteCommand({"set", "rc_min_routing_layer", "0"}));
  EXPECT_EQ(placer.EffectiveRcRoutingLayer(), 0);
  EXPECT_TRUE(placer.VerifyRcConfigurationIsEffective("test"));
  placer.Close();
}

TEST_F(DaliCommandProcessorTest, ANegativeRoutingLayerIsRejected) {
  dali::Dali placer(nullptr, dali::severity::info);
  placer.InitializeRCEstimatorForTesting();
  EXPECT_TRUE(placer.ExecuteCommand({"set", "rc_min_routing_layer", "1"}));
  EXPECT_FALSE(placer.ExecuteCommand({"set", "rc_min_routing_layer", "-1"}));
  EXPECT_EQ(placer.EffectiveRcRoutingLayer(), 1)
      << "a rejected value must not have reached the estimator";
  EXPECT_TRUE(placer.VerifyRcConfigurationIsEffective("test"));
  placer.Close();
}

// The guard itself, driven from a drifted state that can only be produced by
// reaching around the setter. If propagation is ever removed, this is what
// turns a silently wrong slack into a failed capture.
TEST_F(DaliCommandProcessorTest, DriftBetweenConfiguredAndEffectiveLayerFails) {
  dali::Dali placer(nullptr, dali::severity::info);
  placer.InitializeRCEstimatorForTesting();
  EXPECT_TRUE(placer.ExecuteCommand({"set", "rc_min_routing_layer", "1"}));
  ASSERT_TRUE(placer.VerifyRcConfigurationIsEffective("test"));

  placer.RcEstimatorForTesting()->SetMinRoutingLayer(0);
  EXPECT_EQ(placer.EffectiveRcRoutingLayer(), 0);
  EXPECT_FALSE(placer.VerifyRcConfigurationIsEffective("test"))
      << "a capture whose estimator disagrees with its configuration must fail";
  placer.Close();
}

// The stage boundary is a different mechanism from the in-loop checkpoint and
// has to be asked for separately. Amendment R disqualified in-loop timing, so a
// recipe that enabled the old checkpoint must not silently acquire the new one.
TEST_F(DaliCommandProcessorTest, StageBoundarySizingIsOffUntilAskedFor) {
  dali::Dali placer(nullptr, dali::severity::info);
  // Off by default: the operation is a no-op that reports success.
  EXPECT_TRUE(placer.RunStageBoundaryTopologySizing());
  EXPECT_TRUE(placer.RunStageBoundaryTopologySizing())
      << "a disabled boundary must not consume the one permitted attempt";

  EXPECT_TRUE(placer.ExecuteCommand(
      {"set", "topology_stage_boundary_sizing", "true"}));
  EXPECT_FALSE(placer.ExecuteCommand(
      {"set", "topology_stage_boundary_sizing", "perhaps"}));
  placer.Close();
}

TEST_F(DaliCommandProcessorTest, ValidatesAdaptiveSizingControls) {
  dali::Dali placer(nullptr, dali::severity::info);
  EXPECT_TRUE(
      placer.ExecuteCommand({"set", "topology_adaptive_sizing", "true"}));
  EXPECT_TRUE(placer.ExecuteCommand(
      {"set", "topology_adaptive_probe_pairs", "2"}));
  EXPECT_TRUE(placer.ExecuteCommand(
      {"set", "topology_adaptive_max_step_pairs", "8"}));
  EXPECT_TRUE(placer.ExecuteCommand(
      {"set", "topology_adaptive_max_epochs", "4"}));
  EXPECT_TRUE(placer.ExecuteCommand(
      {"set", "topology_adaptive_max_nonpositive_probes", "1"}));
  EXPECT_TRUE(placer.ExecuteCommand(
      {"set", "timing_observe_global_iterations", "false"}));

  EXPECT_FALSE(
      placer.ExecuteCommand({"set", "topology_adaptive_sizing", "perhaps"}));
  EXPECT_FALSE(placer.ExecuteCommand(
      {"set", "topology_adaptive_probe_pairs", "0"}));
  EXPECT_FALSE(placer.ExecuteCommand(
      {"set", "topology_adaptive_max_step_pairs", "0"}));
  EXPECT_FALSE(placer.ExecuteCommand(
      {"set", "topology_adaptive_max_epochs", "0"}));
  EXPECT_FALSE(placer.ExecuteCommand(
      {"set", "topology_adaptive_max_nonpositive_probes", "-1"}));
  EXPECT_FALSE(placer.ExecuteCommand(
      {"set", "timing_observe_global_iterations", "sometimes"}));
  placer.Close();
}

// Control 10: one run changes one site at most once. A second request is a
// candidate search, which is the thing this architecture is meant not to be.
TEST_F(DaliCommandProcessorTest, ASecondStageBoundaryMutationIsRejected) {
  dali::Dali placer(nullptr, dali::severity::info);
  ASSERT_TRUE(placer.ExecuteCommand(
      {"set", "topology_stage_boundary_sizing", "true"}));

  // No sizing is configured, so the first attempt is a legitimate no-change --
  // and it still spends the single attempt.
  EXPECT_TRUE(placer.RunStageBoundaryTopologySizing());
  EXPECT_FALSE(placer.RunStageBoundaryTopologySizing())
      << "the second topology change of a run must be refused";
  placer.Close();
}

// Controls 3 and 4: a site the evidence cannot support must fail before the
// host is reached. A host that is called at all has already been told to change
// an authoritative netlist.
TEST_F(DaliCommandProcessorTest, StageBoundaryFailsBeforeCallingTheHost) {
  struct CountingHost : public dali::TopologyCheckpointHost {
    int calls = 0;
    dali::TopologyMutationResult ApplyTopologyChange(
        const dali::TopologyCheckpointContext &,
        const dali::TopologyChangeBatch &) override {
      ++calls;
      return dali::TopologyMutationResult::NoChange();
    }
  } host;

  dali::Dali placer(nullptr, dali::severity::info);
  ASSERT_TRUE(placer.ExecuteCommand(
      {"set", "topology_stage_boundary_sizing", "true"}));
  placer.SetTopologyCheckpointHost(&host);
  // A characterization makes sizing configured, so the decision is attempted;
  // with no registered delay line there is no evidence to decide on.
  placer.SetDelayLineCharacterization("dl5", 128.0830, 95, 105);

  EXPECT_FALSE(placer.RunStageBoundaryTopologySizing());
  EXPECT_EQ(host.calls, 0)
      << "the host was asked to change a netlist on evidence Dali had already "
         "rejected";
  placer.Close();
}

TEST_F(DaliCommandProcessorTest, AcceptsAValidTopologyCheckpointSchedule) {
  dali::Dali placer(nullptr, dali::severity::info);
  EXPECT_TRUE(
      placer.ExecuteCommand({"set", "topology_checkpoint_warmup", "12"}));
  EXPECT_TRUE(
      placer.ExecuteCommand({"set", "topology_checkpoint_interval", "1"}));
  EXPECT_TRUE(placer.ExecuteCommand({"set", "topology_checkpoint_max", "1"}));
  // Zero checkpoints is the default and an explicit, meaningful choice.
  EXPECT_TRUE(placer.ExecuteCommand({"set", "topology_checkpoint_max", "0"}));
  // Warm-up may legitimately be the very first iteration.
  EXPECT_TRUE(
      placer.ExecuteCommand({"set", "topology_checkpoint_warmup", "0"}));
  placer.Close();
}

TEST_F(DaliCommandProcessorTest, RejectsAnInvalidTopologyCheckpointSchedule) {
  dali::Dali placer(nullptr, dali::severity::info);
  EXPECT_FALSE(
      placer.ExecuteCommand({"set", "topology_checkpoint_warmup", "-1"}));
  EXPECT_FALSE(
      placer.ExecuteCommand({"set", "topology_checkpoint_warmup", "soon"}));
  // An interval of zero would mean two checkpoints on one iteration.
  EXPECT_FALSE(
      placer.ExecuteCommand({"set", "topology_checkpoint_interval", "0"}));
  EXPECT_FALSE(
      placer.ExecuteCommand({"set", "topology_checkpoint_interval", "-2"}));
  EXPECT_FALSE(placer.ExecuteCommand({"set", "topology_checkpoint_max", "-1"}));
  EXPECT_FALSE(
      placer.ExecuteCommand({"set", "topology_checkpoint_max", "lots"}));
  placer.Close();
}

TEST_F(DaliCommandProcessorTest, RunsCommandFileWithContinuation) {
  const std::filesystem::path script_path =
      std::filesystem::temp_directory_path() /
      "dali_command_processor_test.dali";
  {
    std::ofstream script(script_path);
    script << "# dali-script 1\n"
           << "set target_density \\\n"
           << "  0.68\n"
           << "set num_threads 3\n"
           << "set disable_detailed_place on\n"
           << "show settings\n";
  }

  dali::Dali placer(nullptr, dali::severity::info);
  EXPECT_TRUE(placer.RunCommandFile(script_path.string()));
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();
  EXPECT_DOUBLE_EQ(options.target_density, 0.68);
  EXPECT_EQ(options.num_threads, 3);
  EXPECT_TRUE(options.disable_detailed_place);
  placer.Close();

  std::filesystem::remove(script_path);
}

TEST_F(DaliCommandProcessorTest, ResolvesNestedSourcesRelativeToTheirRecipe) {
  const std::filesystem::path test_directory =
      std::filesystem::temp_directory_path() / "dali_relative_source_test";
  const std::filesystem::path nested_directory = test_directory / "nested";
  std::filesystem::create_directories(nested_directory);
  const std::filesystem::path parent_script = test_directory / "flow.dali";
  const std::filesystem::path child_script = nested_directory / "settings.dali";
  {
    std::ofstream child(child_script);
    child << "set target_density 0.71\n";
  }
  {
    std::ofstream parent(parent_script);
    parent << "source \"nested/settings.dali\"\n";
  }

  dali::Dali placer(nullptr, dali::severity::info);
  EXPECT_TRUE(placer.RunCommandFile(parent_script.string()));
  EXPECT_DOUBLE_EQ(placer.GetRuntimeOptions().target_density, 0.71);
  placer.Close();

  std::filesystem::remove_all(test_directory);
}

TEST_F(DaliCommandProcessorTest, StopsCommandFileAtFirstFailure) {
  const std::filesystem::path script_path =
      std::filesystem::temp_directory_path() /
      "dali_command_processor_failure_test.dali";
  {
    std::ofstream script(script_path);
    script << "set target_density 0.72\n"
           << "not-a-command\n"
           << "set num_threads 9\n";
  }

  dali::Dali placer(nullptr, dali::severity::info);
  EXPECT_FALSE(placer.RunCommandFile(script_path.string()));
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();
  EXPECT_DOUBLE_EQ(options.target_density, 0.72);
  EXPECT_EQ(options.num_threads, 1);
  placer.Close();

  std::filesystem::remove(script_path);
}

TEST_F(DaliCommandProcessorTest, TimingReportRequiresAttachedTimingHost) {
  dali::Dali placer(nullptr, dali::severity::info);

  EXPECT_FALSE(placer.ExecuteCommand({"timing-report", "unexpected"}));
  EXPECT_FALSE(placer.ExecuteCommand({"timing-report"}));
  EXPECT_FALSE(placer.ExecuteCommand({"timing-check", "unexpected"}));
  EXPECT_FALSE(placer.ExecuteCommand({"timing-check"}));
  EXPECT_FALSE(placer.ExecuteCommand({"runtime-report", "unexpected"}));
  EXPECT_TRUE(placer.ExecuteCommand({"runtime-report"}));
  EXPECT_FALSE(placer.ExecuteCommand({"write-timing-repair-plan"}));
  EXPECT_FALSE(placer.ExecuteCommand({"weight-critical-cycle", "2"}));
  EXPECT_FALSE(placer.ExecuteCommand({"weight-critical-cycle", "0"}));
  placer.Close();
}

TEST_F(DaliCommandProcessorTest,
       InteractiveModeContinuesAfterErrorsAndRecordsHistory) {
  std::istringstream input(
      "set target_density \\\n"
      "  0.66\n"
      "not-a-command\n"
      "set num_threads 7\n"
      "history\n"
      "quit\n"
      "set num_threads 9\n");
  std::ostringstream output;
  dali::Dali placer(nullptr, dali::severity::info);
  dali::DaliCommandProcessor processor(&placer);

  EXPECT_TRUE(processor.RunInteractive(input, output, false));
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();
  EXPECT_DOUBLE_EQ(options.target_density, 0.66);
  EXPECT_EQ(options.num_threads, 7);
  EXPECT_NE(output.str().find("Dali interactive mode"), std::string::npos);
  EXPECT_NE(output.str().find("set num_threads 7"), std::string::npos);
  EXPECT_EQ(output.str().find("set num_threads 9"), std::string::npos);
  placer.Close();
}

TEST_F(DaliCommandProcessorTest, InteractiveModeCanSourceCommandFile) {
  const std::filesystem::path script_path =
      std::filesystem::temp_directory_path() /
      "dali_interactive_source_test.dali";
  {
    std::ofstream script(script_path);
    script << "set target_density 0.74\n"
           << "set num_threads 5\n";
  }
  std::istringstream input("source \"" + script_path.string() + "\"\nexit\n");
  std::ostringstream output;
  dali::Dali placer(nullptr, dali::severity::info);
  dali::DaliCommandProcessor processor(&placer);

  EXPECT_TRUE(processor.RunInteractive(input, output, false));
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();
  EXPECT_DOUBLE_EQ(options.target_density, 0.74);
  EXPECT_EQ(options.num_threads, 5);
  placer.Close();
  std::filesystem::remove(script_path);
}

TEST_F(DaliCommandProcessorTest,
       InteractiveModeRejectsIncompleteFinalContinuation) {
  std::istringstream input("set target_density \\");
  std::ostringstream output;
  dali::Dali placer(nullptr, dali::severity::info);
  dali::DaliCommandProcessor processor(&placer);

  EXPECT_FALSE(processor.RunInteractive(input, output, false));
  placer.Close();
}

TEST_F(DaliCommandProcessorTest, InteractiveModeWaitsWithoutBlockingGuiPump) {
  std::istringstream input("quit\n");
  std::ostringstream output;
  dali::Dali placer(nullptr, dali::severity::info);
  dali::DaliCommandProcessor processor(&placer);
  int wait_count = 0;

  EXPECT_TRUE(processor.RunInteractive(input, output, true,
                                       [&wait_count]() { ++wait_count; }));
  EXPECT_EQ(wait_count, 1);
  placer.Close();
}
