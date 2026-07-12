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

using testing::Test;

class DaliCommandLineTest : public Test {
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
  EXPECT_EQ(options.visualization_dir, "");
  EXPECT_EQ(options.verbose_level, dali::severity::info);
}

TEST_F(DaliCommandLineTest, ParsesRuntimeConfigOptions) {
  dali::DaliCommandLineOptions options;
  EXPECT_TRUE(Parse({"dali",
                     "-lef",
                     "input.lef",
                     "-def",
                     "input.def",
                     "-output_name",
                     "placed",
                     "-metrics_file",
                     "metrics.json",
                     "-net_hpwl_file",
                     "net_hpwl.tsv",
                     "-target_density",
                     "0.72",
                     "-visualization_dir",
                     "dali_snapshots",
                     "-num_threads",
                     "8",
                     "-io_metal_layer",
                     "3",
                     "-well_legalization_mode",
                     "scavenge",
                     "-global_initializer",
                     "keep",
                     "-global_lal_hotspot",
                     "overflow",
                     "-global_lal_affine_weight",
                     "0.8",
                     "-global_min_iterations",
                     "25",
                     "-standard_cell_legalizer_cost",
                     "hpwl",
                     "-detailed_max_rounds",
                     "2",
                     "-detailed_max_move_candidates",
                     "500",
                     "-enable_gridded_global_capacity",
                     "-enable_gridded_upper_bound_refiner",
                     "-enable_gridded_stripe_balancing",
                     "-enable_gridded_local_reorder",
                     "-enable_gridded_row_y_optimization",
                     "-debug_placement_region_scale",
                     "1.1",
                     "-save_intermediate_result",
                     "-disable_detailed_place",
                     "-disable_io_place",
                     "-gui_debug",
                     "-gui_pause",
                     "off"},
                    &options));

  EXPECT_EQ(options.output_name, "placed");
  EXPECT_EQ(options.metrics_file_name, "metrics.json");
  EXPECT_EQ(options.net_hpwl_file_name, "net_hpwl.tsv");
  EXPECT_EQ(options.visualization_dir, "dali_snapshots");
  EXPECT_DOUBLE_EQ(config_get_real("dali.target_density"), 0.72);
  EXPECT_STREQ(config_get_string("dali.visualization_dir"), "dali_snapshots");
  EXPECT_EQ(config_get_int("dali.num_threads"), 8);
  EXPECT_EQ(config_get_int("dali.io_metal_layer"), 2);
  EXPECT_STREQ(config_get_string("dali.well_legalization_mode"), "scavenge");
  EXPECT_STREQ(config_get_string("dali.global_initializer"), "keep");
  EXPECT_STREQ(config_get_string("dali.global_lal_hotspot"), "overflow");
  EXPECT_DOUBLE_EQ(config_get_real("dali.global_lal_affine_weight"), 0.8);
  EXPECT_EQ(config_get_int("dali.global_min_iterations"), 25);
  EXPECT_STREQ(config_get_string("dali.standard_cell_legalizer_cost"), "hpwl");
  EXPECT_EQ(config_get_int("dali.detailed_max_rounds"), 2);
  EXPECT_EQ(config_get_int("dali.detailed_max_move_candidates"), 500);
  EXPECT_EQ(config_get_int("dali.enable_gridded_global_capacity"), 1);
  EXPECT_EQ(config_get_int("dali.enable_gridded_upper_bound_refiner"), 1);
  EXPECT_EQ(config_get_int("dali.enable_gridded_stripe_balancing"), 1);
  EXPECT_EQ(config_get_int("dali.enable_gridded_local_reorder"), 1);
  EXPECT_EQ(config_get_int("dali.enable_gridded_row_y_optimization"), 1);
  EXPECT_DOUBLE_EQ(config_get_real("dali.debug_placement_region_scale"), 1.1);
  EXPECT_EQ(config_get_int("dali.save_intermediate_result"), 1);
  EXPECT_EQ(config_get_int("dali.disable_detailed_place"), 1);
  EXPECT_EQ(config_get_int("dali.disable_io_place"), 1);
  EXPECT_EQ(config_get_int("dali.gui_debug"), 1);
  EXPECT_STREQ(config_get_string("dali.gui_pause"), "off");
}

TEST_F(DaliCommandLineTest, RejectsMissingRequiredInputs) {
  dali::DaliCommandLineOptions missing_def_options;
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef"}, &missing_def_options));

  dali::DaliCommandLineOptions missing_lef_options;
  EXPECT_FALSE(Parse({"dali", "-def", "input.def"}, &missing_lef_options));
}

TEST_F(DaliCommandLineTest, RejectsPlacementRegionShrinkDebugScale) {
  dali::DaliCommandLineOptions options;
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def",
                      "-debug_placement_region_scale", "0.9"},
                     &options));
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
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def",
                      "-global_initializer", "randomish"},
                     &options));
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def",
                      "-global_lal_hotspot", "largest"},
                     &options));
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def",
                      "-global_lal_affine_weight", "1.2"},
                     &options));
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def",
                      "-global_min_iterations", "-1"},
                     &options));
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def",
                      "-standard_cell_legalizer_cost", "wirelengthish"},
                     &options));
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def",
                      "-detailed_max_rounds", "-1"},
                     &options));
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def",
                      "-detailed_max_move_candidates", "-1"},
                     &options));
  EXPECT_FALSE(Parse({"dali", "-lef", "input.lef", "-def", "input.def",
                      "-gui_pause", "sometimes"},
                     &options));
}
