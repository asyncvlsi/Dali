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
// clang-format off
#include <stdio.h>
#include <common/config.h>
// clang-format on

#include "dali/application/dali_command_line.h"

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <vector>

namespace {

class DaliCommandLineTest : public ::testing::Test {
 protected:
  void SetUp() override { config_clear(); }

  static std::vector<char*> MakeArgv(std::vector<std::string>* args) {
    std::vector<char*> argv;
    argv.reserve(args->size());
    for (std::string& arg : *args) {
      argv.push_back(arg.data());
    }
    return argv;
  }

  bool Parse(std::vector<std::string> args,
             dali::DaliCommandLineOptions* options) {
    std::vector<char*> argv = MakeArgv(&args);
    std::ostringstream errors;
    return dali::ParseDaliCommandLine(static_cast<int>(argv.size()),
                                      argv.data(), options, errors);
  }
};

TEST_F(DaliCommandLineTest, ParsesRequiredInputsAndKeepsDefaults) {
  dali::DaliCommandLineOptions options;
  EXPECT_TRUE(
      Parse({"dali", "-lef", "input.lef", "-def", "input.def"}, &options));

  EXPECT_EQ(options.lef_file_name, "input.lef");
  EXPECT_EQ(options.def_file_name, "input.def");
  EXPECT_EQ(options.output_name, "dali_out");
  EXPECT_EQ(options.metrics_file_name, "dali_metrics.json");
  EXPECT_EQ(options.verbose_level, dali::severity::info);
}

TEST_F(DaliCommandLineTest, ParsesRuntimeConfigOptions) {
  dali::DaliCommandLineOptions options;
  EXPECT_TRUE(
      Parse({"dali", "-lef", "input.lef", "-def", "input.def", "-output_name",
             "placed", "-metrics_file", "metrics.json", "-target_density",
             "0.72", "-num_threads", "8", "-io_metal_layer", "3",
             "-well_legalization_mode", "scavenge", "-disable_io_place"},
            &options));

  EXPECT_EQ(options.output_name, "placed");
  EXPECT_EQ(options.metrics_file_name, "metrics.json");
  EXPECT_DOUBLE_EQ(config_get_real("dali.target_density"), 0.72);
  EXPECT_EQ(config_get_int("dali.num_threads"), 8);
  EXPECT_EQ(config_get_int("dali.io_metal_layer"), 2);
  EXPECT_STREQ(config_get_string("dali.well_legalization_mode"), "scavenge");
  EXPECT_EQ(config_get_int("dali.disable_io_place"), 1);
}

TEST_F(DaliCommandLineTest, RejectsMissingRequiredInputs) {
  dali::DaliCommandLineOptions missing_def_options;
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef"}, &missing_def_options));

  dali::DaliCommandLineOptions missing_lef_options;
  EXPECT_FALSE(Parse({"dali", "-def", "input.def"}, &missing_lef_options));
}

TEST_F(DaliCommandLineTest, RejectsPartialNumericTokens) {
  dali::DaliCommandLineOptions options;
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def",
                      "-target_density", "0.7abc"},
                     &options));
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def",
                      "-num_threads", "4abc"},
                     &options));
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def", "-grid",
                      "0.2", "0.3abc"},
                     &options));
}

TEST_F(DaliCommandLineTest, RejectsOutOfRangeOptions) {
  dali::DaliCommandLineOptions options;
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def",
                      "-target_density", "1.1"},
                     &options));
  EXPECT_FALSE(Parse(
      {"dali", "-lef", "input.lef", "-def", "input.def", "-num_threads", "0"},
      &options));
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def",
                      "-io_metal_layer", "0"},
                     &options));
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def",
                      "-well_legalization_mode", "loose"},
                     &options));
}

}  // namespace
