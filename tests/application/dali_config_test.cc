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
#include <gtest/gtest.h>

#include "dali/common/act_config.h"
#include "dali/dali.h"

using testing::Test;

class DaliConfigTest : public Test {
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
  EXPECT_EQ(options.well_legalization_mode, dali::WellPartitionMode::kStrict);
  EXPECT_FALSE(options.disable_global_place);
  EXPECT_FALSE(options.disable_legalization);
  EXPECT_FALSE(options.disable_detailed_place);
  EXPECT_FALSE(options.disable_io_place);
  EXPECT_DOUBLE_EQ(options.target_density, -1);
  EXPECT_EQ(options.net_ignore_threshold, 100);
  EXPECT_EQ(options.io_metal_layer, 0);
  EXPECT_FALSE(options.export_well_cluster_matlab);
  EXPECT_FALSE(options.disable_welltap);
  EXPECT_FALSE(options.disable_cell_flip);
  EXPECT_DOUBLE_EQ(options.max_row_width, 0);
  EXPECT_FALSE(options.enable_adaptive_stripe_boundaries);
  EXPECT_FALSE(options.is_standard_cell);
  EXPECT_FALSE(options.enable_filler_cell);
  EXPECT_FALSE(options.enable_end_cap_cell);
  EXPECT_FALSE(options.enable_gridded_local_reorder);
  EXPECT_FALSE(options.enable_gridded_detailed_placement);
  EXPECT_FALSE(options.enable_gridded_detailed_relocation);
  EXPECT_FALSE(options.enable_gridded_assignment_batch);
  EXPECT_EQ(options.gridded_detailed_max_candidate_rows, 4);
  EXPECT_EQ(options.gridded_detailed_max_rounds, 6);
  EXPECT_DOUBLE_EQ(options.gridded_detailed_min_relative_improvement, 0.005);
  EXPECT_FALSE(options.disable_gridded_vertical_swap);
  EXPECT_EQ(options.gridded_legalization_feedback_mode,
            dali::GlobalRefinementFeedbackMode::kYRowTransactionalConsistent);
  EXPECT_FALSE(options.enable_gridded_upper_bound_balancing);
  EXPECT_FALSE(options.enable_gridded_legalization_pressure);
  EXPECT_FALSE(options.enable_gridded_row_y_optimization);
  EXPECT_FALSE(options.enable_ortools_row_optimization);
  EXPECT_FALSE(options.analyze_exact_gridded_legalization);
  EXPECT_FALSE(options.analyze_exact_adjacent_rows);
  EXPECT_FALSE(options.analyze_exact_row_geometry);
  EXPECT_EQ(options.exact_gridded_window_components, 48);
  EXPECT_EQ(options.exact_gridded_max_windows, 24);
  EXPECT_DOUBLE_EQ(options.exact_gridded_window_time, 0.25);
  EXPECT_EQ(options.exact_gridded_max_row_changes, -1);
  EXPECT_FALSE(options.solve_exact_gridded_legalization);
  EXPECT_DOUBLE_EQ(options.exact_gridded_solve_time, 3600.0);
  EXPECT_EQ(options.exact_gridded_row_radius, 0);
  EXPECT_TRUE(options.exact_gridded_use_solution_hint);
  EXPECT_FALSE(options.exact_gridded_log_search_progress);
  EXPECT_FALSE(options.enable_exact_gridded_stripe_optimization);
  EXPECT_DOUBLE_EQ(options.exact_gridded_stripe_time, 5.0);
  EXPECT_DOUBLE_EQ(options.exact_gridded_stripe_total_time, 120.0);
  EXPECT_EQ(options.exact_gridded_stripe_sweeps, 2);
  EXPECT_EQ(options.exact_gridded_stripe_components, 0);
  EXPECT_EQ(options.exact_gridded_stripe_row_radius, 0);
  EXPECT_DOUBLE_EQ(options.exact_gridded_stripe_displacement_weight, 0.0);
  EXPECT_FALSE(options.exact_gridded_stripe_fixed_row_prepass);
  EXPECT_FALSE(options.exact_gridded_stripe_before_detailed);
  EXPECT_FALSE(options.enable_exact_gridded_boundary_optimization);
  EXPECT_DOUBLE_EQ(options.exact_gridded_boundary_time, 0.1);
  EXPECT_DOUBLE_EQ(options.exact_gridded_boundary_total_time, 120.0);
  EXPECT_EQ(options.exact_gridded_boundary_components, 64);
  EXPECT_EQ(options.exact_gridded_boundary_max_changes, 4);
  EXPECT_FALSE(options.enable_shrink_off_grid_die_area);
  EXPECT_EQ(options.global_initializer,
            dali::PlacementInitializerType::kUniform);
  EXPECT_EQ(options.global_lal_hotspot_mode,
            dali::GlobalLalHotspotMode::kComponentArea);
  EXPECT_DOUBLE_EQ(options.global_lal_affine_weight, 0.65);
  EXPECT_EQ(options.global_min_iterations, 10);
  EXPECT_EQ(options.standard_cell_legalizer_cost_mode,
            dali::StandardCellLegalizerCostMode::kDisplacement);
  EXPECT_EQ(options.detailed_max_rounds, 1);
  EXPECT_EQ(options.detailed_max_move_candidates, 1000);
  EXPECT_FALSE(options.save_intermediate_result);
  EXPECT_EQ(options.output_name, "dali_out");
  EXPECT_EQ(options.visualization_dir, "");
  EXPECT_FALSE(options.gui_debug);
  EXPECT_EQ(options.gui_pause, "every_snapshot");

  placer.Close();
}

TEST_F(DaliConfigTest, LoadsRuntimeOptionsFromActConfig) {
  config_set_string("dali.log_file_name", "dali_test.log");
  config_set_int("dali.disable_log_prefix", 1);
  config_set_int("dali.num_threads", 4);
  config_set_string("dali.well_legalization_mode", "scavenge");
  config_set_int("dali.disable_global_place", 1);
  config_set_int("dali.disable_legalization", 1);
  config_set_int("dali.disable_detailed_place", 1);
  config_set_int("dali.disable_io_place", 1);
  config_set_real("dali.target_density", 0.71);
  config_set_int("dali.net_ignore_threshold", 300);
  config_set_int("dali.io_metal_layer", 2);
  config_set_int("dali.export_well_cluster_matlab", 1);
  config_set_int("dali.disable_welltap", 1);
  config_set_int("dali.disable_cell_flip", 1);
  config_set_real("dali.max_row_width", 42.5);
  config_set_int("dali.enable_adaptive_stripe_boundaries", 1);
  config_set_int("dali.is_standard_cell", 1);
  config_set_int("dali.enable_filler_cell", 1);
  config_set_int("dali.enable_end_cap_cell", 1);
  config_set_int("dali.enable_gridded_global_capacity", 1);
  config_set_int("dali.enable_gridded_upper_bound_refiner", 1);
  config_set_int("dali.enable_gridded_upper_bound_balancing", 1);
  config_set_int("dali.disable_gridded_feedback_rollback", 1);
  config_set_int("dali.enable_gridded_legalization_pressure", 1);
  config_set_string("dali.gridded_legalization_feedback", "y_only");
  config_set_int("dali.enable_gridded_stripe_balancing", 1);
  config_set_int("dali.enable_gridded_local_reorder", 1);
  config_set_int("dali.enable_gridded_detailed_placement", 1);
  config_set_int("dali.enable_gridded_detailed_relocation", 1);
  config_set_int("dali.enable_gridded_assignment_batch", 1);
  config_set_int("dali.gridded_detailed_max_candidate_rows", 8);
  config_set_int("dali.gridded_detailed_max_rounds", 5);
  config_set_real("dali.gridded_detailed_min_relative_improvement", 0.002);
  config_set_int("dali.disable_gridded_vertical_swap", 1);
  config_set_int("dali.enable_gridded_row_y_optimization", 1);
  config_set_int("dali.enable_ortools_row_optimization", 1);
  config_set_int("dali.analyze_exact_gridded_legalization", 1);
  config_set_int("dali.analyze_exact_adjacent_rows", 1);
  config_set_int("dali.exact_gridded_window_components", 64);
  config_set_int("dali.exact_gridded_max_windows", 12);
  config_set_real("dali.exact_gridded_window_time", 0.5);
  config_set_int("dali.exact_gridded_max_row_changes", 4);
  config_set_int("dali.solve_exact_gridded_legalization", 1);
  config_set_real("dali.exact_gridded_solve_time", 7200.0);
  config_set_int("dali.exact_gridded_row_radius", 3);
  config_set_int("dali.exact_gridded_use_solution_hint", 0);
  config_set_int("dali.exact_gridded_log_search_progress", 1);
  config_set_int("dali.enable_exact_gridded_stripe_optimization", 1);
  config_set_real("dali.exact_gridded_stripe_time", 8.0);
  config_set_real("dali.exact_gridded_stripe_total_time", 90.0);
  config_set_int("dali.exact_gridded_stripe_sweeps", 3);
  config_set_int("dali.exact_gridded_stripe_components", 48);
  config_set_int("dali.exact_gridded_stripe_row_radius", 2);
  config_set_real("dali.exact_gridded_stripe_displacement_weight", 0.125);
  config_set_int("dali.exact_gridded_stripe_fixed_row_prepass", 1);
  config_set_int("dali.exact_gridded_stripe_before_detailed", 1);
  config_set_int("dali.enable_exact_gridded_boundary_optimization", 1);
  config_set_real("dali.exact_gridded_boundary_time", 0.2);
  config_set_real("dali.exact_gridded_boundary_total_time", 60.0);
  config_set_int("dali.exact_gridded_boundary_components", 32);
  config_set_int("dali.exact_gridded_boundary_max_changes", 6);
  config_set_int("dali.enable_shrink_off_grid_die_area", 1);
  config_set_string("dali.global_initializer", "keep");
  config_set_string("dali.global_lal_hotspot", "overflow_ratio");
  config_set_real("dali.global_lal_affine_weight", 0.8);
  config_set_int("dali.global_min_iterations", 25);
  config_set_string("dali.standard_cell_legalizer_cost", "hpwl");
  config_set_int("dali.detailed_max_rounds", 2);
  config_set_int("dali.detailed_max_move_candidates", 500);
  config_set_int("dali.save_intermediate_result", 1);
  config_set_string("dali.output_name", "placed");
  config_set_string("dali.visualization_dir", "dali_snapshots");
  config_set_int("dali.gui_debug", 1);
  config_set_string("dali.gui_pause", "off");
  config_set_real("dali.debug_placement_region_scale", 1.1);

  dali::Dali placer(nullptr, dali::severity::info);
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();

  EXPECT_EQ(options.log_file_name, "dali_test.log");
  EXPECT_TRUE(options.disable_log_prefix);
  EXPECT_EQ(options.num_threads, 4);
  EXPECT_EQ(options.well_legalization_mode, dali::WellPartitionMode::kScavenge);
  EXPECT_TRUE(options.disable_global_place);
  EXPECT_TRUE(options.disable_legalization);
  EXPECT_TRUE(options.disable_detailed_place);
  EXPECT_TRUE(options.disable_io_place);
  EXPECT_DOUBLE_EQ(options.target_density, 0.71);
  EXPECT_EQ(options.net_ignore_threshold, 300);
  EXPECT_EQ(options.io_metal_layer, 2);
  EXPECT_TRUE(options.export_well_cluster_matlab);
  EXPECT_TRUE(options.disable_welltap);
  EXPECT_TRUE(options.disable_cell_flip);
  EXPECT_DOUBLE_EQ(options.max_row_width, 42.5);
  EXPECT_TRUE(options.enable_adaptive_stripe_boundaries);
  EXPECT_TRUE(options.is_standard_cell);
  EXPECT_TRUE(options.enable_filler_cell);
  EXPECT_TRUE(options.enable_end_cap_cell);
  EXPECT_TRUE(options.enable_gridded_global_capacity);
  EXPECT_TRUE(options.enable_gridded_upper_bound_refiner);
  EXPECT_TRUE(options.enable_gridded_upper_bound_balancing);
  EXPECT_TRUE(options.disable_gridded_feedback_rollback);
  EXPECT_TRUE(options.enable_gridded_legalization_pressure);
  EXPECT_EQ(options.gridded_legalization_feedback_mode,
            dali::GlobalRefinementFeedbackMode::kYOnly);
  EXPECT_TRUE(options.enable_gridded_stripe_balancing);
  EXPECT_TRUE(options.enable_gridded_local_reorder);
  EXPECT_TRUE(options.enable_gridded_detailed_placement);
  EXPECT_TRUE(options.enable_gridded_detailed_relocation);
  EXPECT_TRUE(options.enable_gridded_assignment_batch);
  EXPECT_EQ(options.gridded_detailed_max_candidate_rows, 8);
  EXPECT_EQ(options.gridded_detailed_max_rounds, 5);
  EXPECT_DOUBLE_EQ(options.gridded_detailed_min_relative_improvement, 0.002);
  EXPECT_TRUE(options.disable_gridded_vertical_swap);
  EXPECT_TRUE(options.enable_gridded_row_y_optimization);
  EXPECT_TRUE(options.enable_ortools_row_optimization);
  EXPECT_TRUE(options.analyze_exact_gridded_legalization);
  EXPECT_TRUE(options.analyze_exact_adjacent_rows);
  EXPECT_FALSE(options.analyze_exact_row_geometry);
  EXPECT_EQ(options.exact_gridded_window_components, 64);
  EXPECT_EQ(options.exact_gridded_max_windows, 12);
  EXPECT_DOUBLE_EQ(options.exact_gridded_window_time, 0.5);
  EXPECT_EQ(options.exact_gridded_max_row_changes, 4);
  EXPECT_TRUE(options.solve_exact_gridded_legalization);
  EXPECT_DOUBLE_EQ(options.exact_gridded_solve_time, 7200.0);
  EXPECT_EQ(options.exact_gridded_row_radius, 3);
  EXPECT_FALSE(options.exact_gridded_use_solution_hint);
  EXPECT_TRUE(options.exact_gridded_log_search_progress);
  EXPECT_TRUE(options.enable_exact_gridded_stripe_optimization);
  EXPECT_DOUBLE_EQ(options.exact_gridded_stripe_time, 8.0);
  EXPECT_DOUBLE_EQ(options.exact_gridded_stripe_total_time, 90.0);
  EXPECT_EQ(options.exact_gridded_stripe_sweeps, 3);
  EXPECT_EQ(options.exact_gridded_stripe_components, 48);
  EXPECT_EQ(options.exact_gridded_stripe_row_radius, 2);
  EXPECT_DOUBLE_EQ(options.exact_gridded_stripe_displacement_weight, 0.125);
  EXPECT_TRUE(options.exact_gridded_stripe_fixed_row_prepass);
  EXPECT_TRUE(options.exact_gridded_stripe_before_detailed);
  EXPECT_TRUE(options.enable_exact_gridded_boundary_optimization);
  EXPECT_DOUBLE_EQ(options.exact_gridded_boundary_time, 0.2);
  EXPECT_DOUBLE_EQ(options.exact_gridded_boundary_total_time, 60.0);
  EXPECT_EQ(options.exact_gridded_boundary_components, 32);
  EXPECT_EQ(options.exact_gridded_boundary_max_changes, 6);
  EXPECT_TRUE(options.enable_shrink_off_grid_die_area);
  EXPECT_EQ(options.global_initializer, dali::PlacementInitializerType::kKeep);
  EXPECT_EQ(options.global_lal_hotspot_mode,
            dali::GlobalLalHotspotMode::kOverflowRatio);
  EXPECT_DOUBLE_EQ(options.global_lal_affine_weight, 0.8);
  EXPECT_EQ(options.global_min_iterations, 25);
  EXPECT_EQ(options.standard_cell_legalizer_cost_mode,
            dali::StandardCellLegalizerCostMode::kHpwl);
  EXPECT_EQ(options.detailed_max_rounds, 2);
  EXPECT_EQ(options.detailed_max_move_candidates, 500);
  EXPECT_TRUE(options.save_intermediate_result);
  EXPECT_EQ(options.output_name, "placed");
  EXPECT_EQ(options.visualization_dir, "dali_snapshots");
  EXPECT_TRUE(options.gui_debug);
  EXPECT_EQ(options.gui_pause, "off");
  EXPECT_DOUBLE_EQ(options.debug_placement_region_scale, 1.1);

  placer.Close();
}

TEST_F(DaliConfigTest, EnablesExactRowGeometryAnalysis) {
  config_set_int("dali.analyze_exact_row_geometry", 1);

  dali::Dali placer(nullptr, dali::severity::info);
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();

  EXPECT_TRUE(options.analyze_exact_gridded_legalization);
  EXPECT_FALSE(options.analyze_exact_adjacent_rows);
  EXPECT_TRUE(options.analyze_exact_row_geometry);

  placer.Close();
}

TEST_F(DaliConfigTest, SupportsLegacyDisabledLegalizationFeedback) {
  config_set_int("dali.disable_gridded_legalization_feedback", 1);

  dali::Dali placer(nullptr, dali::severity::info);
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();

  EXPECT_EQ(options.gridded_legalization_feedback_mode,
            dali::GlobalRefinementFeedbackMode::kNone);

  placer.Close();
}

TEST_F(DaliConfigTest, LoadsRowScaleLegalizationFeedback) {
  config_set_string("dali.gridded_legalization_feedback", "y_row_scale");

  dali::Dali placer(nullptr, dali::severity::info);
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();

  EXPECT_EQ(options.gridded_legalization_feedback_mode,
            dali::GlobalRefinementFeedbackMode::kYRowScale);

  placer.Close();
}

TEST_F(DaliConfigTest, LoadsHpwlFilteredLegalizationFeedback) {
  config_set_string("dali.gridded_legalization_feedback", "y_row_hpwl");

  dali::Dali placer(nullptr, dali::severity::info);
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();

  EXPECT_EQ(options.gridded_legalization_feedback_mode,
            dali::GlobalRefinementFeedbackMode::kYRowHpwl);

  placer.Close();
}

TEST_F(DaliConfigTest, LoadsTransactionalLegalizationFeedback) {
  config_set_string("dali.gridded_legalization_feedback",
                    "y_row_transactional");

  dali::Dali placer(nullptr, dali::severity::info);
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();

  EXPECT_EQ(options.gridded_legalization_feedback_mode,
            dali::GlobalRefinementFeedbackMode::kYRowTransactional);

  placer.Close();
}

TEST_F(DaliConfigTest, LoadsPositiveTransactionalFeedback) {
  config_set_string("dali.gridded_legalization_feedback",
                    "y_row_transactional_positive");

  dali::Dali placer(nullptr, dali::severity::info);
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();

  EXPECT_EQ(options.gridded_legalization_feedback_mode,
            dali::GlobalRefinementFeedbackMode::kYRowTransactionalPositive);

  placer.Close();
}

TEST_F(DaliConfigTest, LoadsConsistentTransactionalFeedback) {
  config_set_string("dali.gridded_legalization_feedback",
                    "y_row_transactional_consistent");

  dali::Dali placer(nullptr, dali::severity::info);
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();

  EXPECT_EQ(options.gridded_legalization_feedback_mode,
            dali::GlobalRefinementFeedbackMode::kYRowTransactionalConsistent);

  placer.Close();
}

TEST_F(DaliConfigTest, LoadsCoherentTransactionalFeedback) {
  config_set_string("dali.gridded_legalization_feedback",
                    "y_row_transactional_coherent");

  dali::Dali placer(nullptr, dali::severity::info);
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();

  EXPECT_EQ(options.gridded_legalization_feedback_mode,
            dali::GlobalRefinementFeedbackMode::kYRowTransactionalCoherent);

  placer.Close();
}

TEST_F(DaliConfigTest, IgnoresUnknownWellLegalizationMode) {
  config_set_string("dali.well_legalization_mode", "unknown");

  dali::Dali placer(nullptr, dali::severity::info);
  const dali::Dali::RuntimeOptions options = placer.GetRuntimeOptions();

  EXPECT_EQ(options.well_legalization_mode, dali::WellPartitionMode::kStrict);

  placer.Close();
}
