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

#include <gtest/gtest.h>

#include "dali/dali.h"

namespace {

class DaliConfigTest : public ::testing::Test {
 protected:
  void SetUp() override { config_clear(); }
  void TearDown() override { config_clear(); }
};

TEST_F(DaliConfigTest, KeepsDefaultRuntimeOptionsWhenConfigIsEmpty) {
  dali::Dali placer(nullptr, dali::severity::info);
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();

  EXPECT_EQ(options.log_file_name, "");
  EXPECT_FALSE(options.disable_log_prefix);
  EXPECT_EQ(options.num_threads, 1);
  EXPECT_EQ(options.well_legalization_mode, dali::DefaultPartitionMode::STRICT);
  EXPECT_FALSE(options.disable_global_place);
  EXPECT_FALSE(options.disable_legalization);
  EXPECT_FALSE(options.disable_io_place);
  EXPECT_DOUBLE_EQ(options.target_density, -1);
  EXPECT_EQ(options.io_metal_layer, 0);
  EXPECT_FALSE(options.export_well_cluster_matlab);
  EXPECT_FALSE(options.disable_welltap);
  EXPECT_FALSE(options.disable_cell_flip);
  EXPECT_DOUBLE_EQ(options.max_row_width, 0);
  EXPECT_FALSE(options.is_standard_cell);
  EXPECT_FALSE(options.enable_filler_cell);
  EXPECT_FALSE(options.enable_end_cap_cell);
  EXPECT_FALSE(options.enable_shrink_off_grid_die_area);
  EXPECT_EQ(options.output_name, "dali_out");

  placer.Close();
}

TEST_F(DaliConfigTest, LoadsRuntimeOptionsFromActConfig) {
  config_set_string("dali.log_file_name", "dali_test.log");
  config_set_int("dali.disable_log_prefix", 1);
  config_set_int("dali.num_threads", 4);
  config_set_string("dali.well_legalization_mode", "scavenge");
  config_set_int("dali.disable_global_place", 1);
  config_set_int("dali.disable_legalization", 1);
  config_set_int("dali.disable_io_place", 1);
  config_set_real("dali.target_density", 0.71);
  config_set_int("dali.io_metal_layer", 2);
  config_set_int("dali.export_well_cluster_matlab", 1);
  config_set_int("dali.disable_welltap", 1);
  config_set_int("dali.disable_cell_flip", 1);
  config_set_real("dali.max_row_width", 42.5);
  config_set_int("dali.is_standard_cell", 1);
  config_set_int("dali.enable_filler_cell", 1);
  config_set_int("dali.enable_end_cap_cell", 1);
  config_set_int("dali.enable_shrink_off_grid_die_area", 1);
  config_set_string("dali.output_name", "placed");

  dali::Dali placer(nullptr, dali::severity::info);
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();

  EXPECT_EQ(options.log_file_name, "dali_test.log");
  EXPECT_TRUE(options.disable_log_prefix);
  EXPECT_EQ(options.num_threads, 4);
  EXPECT_EQ(options.well_legalization_mode,
            dali::DefaultPartitionMode::SCAVENGE);
  EXPECT_TRUE(options.disable_global_place);
  EXPECT_TRUE(options.disable_legalization);
  EXPECT_TRUE(options.disable_io_place);
  EXPECT_DOUBLE_EQ(options.target_density, 0.71);
  EXPECT_EQ(options.io_metal_layer, 2);
  EXPECT_TRUE(options.export_well_cluster_matlab);
  EXPECT_TRUE(options.disable_welltap);
  EXPECT_TRUE(options.disable_cell_flip);
  EXPECT_DOUBLE_EQ(options.max_row_width, 42.5);
  EXPECT_TRUE(options.is_standard_cell);
  EXPECT_TRUE(options.enable_filler_cell);
  EXPECT_TRUE(options.enable_end_cap_cell);
  EXPECT_TRUE(options.enable_shrink_off_grid_die_area);
  EXPECT_EQ(options.output_name, "placed");

  placer.Close();
}

TEST_F(DaliConfigTest, IgnoresUnknownWellLegalizationMode) {
  config_set_string("dali.well_legalization_mode", "unknown");

  dali::Dali placer(nullptr, dali::severity::info);
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();

  EXPECT_EQ(options.well_legalization_mode, dali::DefaultPartitionMode::STRICT);

  placer.Close();
}

}  // namespace
