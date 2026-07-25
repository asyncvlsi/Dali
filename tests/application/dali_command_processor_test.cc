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
  EXPECT_TRUE(placer.ExecuteCommand({"dali:set", "num_threads", "6"}));
  EXPECT_TRUE(placer.ExecuteCommand({"set", "disable_io_place", "true"}));
  EXPECT_TRUE(
      placer.ExecuteCommand({"set", "global_initializer", "density_aware"}));

  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();
  EXPECT_DOUBLE_EQ(options.target_density, 0.73);
  EXPECT_EQ(options.num_threads, 6);
  EXPECT_TRUE(options.disable_io_place);
  EXPECT_EQ(options.global_initializer,
            dali::PlacementInitializerType::kDensityAware);

  EXPECT_FALSE(placer.ExecuteCommand({"set", "target_density", "1.1"}));
  EXPECT_FALSE(placer.ExecuteCommand({"set", "num_threads", "many"}));
  EXPECT_FALSE(placer.ExecuteCommand({"set", "unknown_option", "1"}));
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
