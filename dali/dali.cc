/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#include "dali.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include "dali/circuit/hpwl_lower_bound.h"
#include "dali/common/act_config.h"
#include "dali/common/elapsed_time.h"
#include "dali/common/git_version.h"
#include "dali/common/helper.h"
#include "dali/common/logging.h"
#include "dali/common/phydb_helper.h"
#include "dali/common/placement_metrics.h"
#include "dali/placer/well_legalizer/rough_gridded_upper_bound_refiner.h"

namespace dali {

static std::string ConfigName(const std::string& prefix, const char* name) {
  return prefix + name;
}

static bool ShouldVisualizeWellRects(const std::string& group,
                                     const std::string& subgroup) {
  return group == "legalization" &&
         (subgroup == "well_tap" || subgroup == "end_cap");
}

static bool ConfigExists(const std::string& name) {
  return config_exists(name.c_str());
}

static void LoadBoolConfig(const std::string& name, bool* value) {
  if (ConfigExists(name)) {
    *value = config_get_int(name.c_str()) == 1;
  }
}

static void LoadIntConfig(const std::string& name, int* value) {
  if (ConfigExists(name)) {
    *value = config_get_int(name.c_str());
  }
}

static void LoadRealConfig(const std::string& name, double* value) {
  if (ConfigExists(name)) {
    *value = config_get_real(name.c_str());
  }
}

static void LoadStringConfig(const std::string& name, std::string* value) {
  if (ConfigExists(name)) {
    *value = config_get_string(name.c_str());
  }
}

static PlacementInitializerType ParseGlobalInitializer(
    const std::string& name) {
  if (name == "keep") {
    return PlacementInitializerType::kKeep;
  }
  if (name == "uniform") {
    return PlacementInitializerType::kUniform;
  }
  if (name == "gaussian") {
    return PlacementInitializerType::kGaussian;
  }
  if (name == "monte_carlo") {
    return PlacementInitializerType::kMonteCarlo;
  }
  if (name == "density_aware") {
    return PlacementInitializerType::kDensityAware;
  }
  std::cout << "Ignore unknown global_initializer: " << name << "\n";
  return PlacementInitializerType::kUniform;
}

static GlobalAnchorSchedule ParseGlobalAnchorSchedule(const std::string& name) {
  if (name == "dali") {
    return GlobalAnchorSchedule::kDali;
  }
  if (name == "simpl") {
    return GlobalAnchorSchedule::kSimpl;
  }
  std::cout << "Ignore unknown global_anchor_schedule: " << name << "\n";
  return GlobalAnchorSchedule::kDali;
}

static GlobalRefinementFeedbackMode ParseGlobalRefinementFeedbackMode(
    const std::string& name) {
  if (name == "full") {
    return GlobalRefinementFeedbackMode::kFull;
  }
  if (name == "x_only") {
    return GlobalRefinementFeedbackMode::kXOnly;
  }
  if (name == "y_only") {
    return GlobalRefinementFeedbackMode::kYOnly;
  }
  if (name == "y_row_scale") {
    return GlobalRefinementFeedbackMode::kYRowScale;
  }
  if (name == "y_row_hpwl") {
    return GlobalRefinementFeedbackMode::kYRowHpwl;
  }
  if (name == "y_row_transactional") {
    return GlobalRefinementFeedbackMode::kYRowTransactional;
  }
  if (name == "y_row_transactional_positive") {
    return GlobalRefinementFeedbackMode::kYRowTransactionalPositive;
  }
  if (name == "y_row_transactional_consistent") {
    return GlobalRefinementFeedbackMode::kYRowTransactionalConsistent;
  }
  if (name == "y_row_transactional_coherent") {
    return GlobalRefinementFeedbackMode::kYRowTransactionalCoherent;
  }
  if (name == "none") {
    return GlobalRefinementFeedbackMode::kNone;
  }
  std::cout << "Ignore unknown gridded_legalization_feedback: " << name << "\n";
  return GlobalRefinementFeedbackMode::kFull;
}

static GlobalGridSchedule ParseGlobalGridSchedule(const std::string& name) {
  if (name == "dali") {
    return GlobalGridSchedule::kDali;
  }
  if (name == "simpl") {
    return GlobalGridSchedule::kSimpl;
  }
  std::cout << "Ignore unknown global_grid_schedule: " << name << "\n";
  return GlobalGridSchedule::kDali;
}

static GlobalLalExpansionMode ParseGlobalLalExpansionMode(
    const std::string& name) {
  if (name == "symmetric") {
    return GlobalLalExpansionMode::kSymmetric;
  }
  if (name == "best_neighbor") {
    return GlobalLalExpansionMode::kBestNeighbor;
  }
  std::cout << "Ignore unknown global_lal_expansion: " << name << "\n";
  return GlobalLalExpansionMode::kSymmetric;
}

static GlobalLalHotspotMode ParseGlobalLalHotspotMode(const std::string& name) {
  if (name == "area") {
    return GlobalLalHotspotMode::kComponentArea;
  }
  if (name == "overflow") {
    return GlobalLalHotspotMode::kOverflow;
  }
  if (name == "overflow_ratio") {
    return GlobalLalHotspotMode::kOverflowRatio;
  }
  std::cout << "Ignore unknown global_lal_hotspot: " << name << "\n";
  return GlobalLalHotspotMode::kComponentArea;
}

static GlobalLalMacroBoundaryMode ParseGlobalLalMacroBoundaryMode(
    const std::string& name) {
  if (name == "off") {
    return GlobalLalMacroBoundaryMode::kOff;
  }
  if (name == "balanced") {
    return GlobalLalMacroBoundaryMode::kBalanced;
  }
  if (name == "preferred") {
    return GlobalLalMacroBoundaryMode::kPreferred;
  }
  std::cout << "Ignore unknown global_lal_macro_boundary: " << name << "\n";
  return GlobalLalMacroBoundaryMode::kOff;
}

static StandardCellLegalizerCostMode ParseStandardCellLegalizerCostMode(
    const std::string& name) {
  if (name == "displacement") {
    return StandardCellLegalizerCostMode::kDisplacement;
  }
  if (name == "hpwl") {
    return StandardCellLegalizerCostMode::kHpwl;
  }
  std::cout << "Ignore unknown standard_cell_legalizer_cost: " << name << "\n";
  return StandardCellLegalizerCostMode::kDisplacement;
}

Dali::Dali(phydb::PhyDB* phy_db_ptr, const std::string& severity_level,
           const std::string& log_file_name) {
  phy_db_ptr_ = phy_db_ptr;
  severity_level_ = StrToLoggingLevel(severity_level);
  log_file_name_ = log_file_name;
  LoadParamsFromConfig();
  InitLogging(log_file_name_, severity_level_, disable_log_prefix_);
}

Dali::Dali(phydb::PhyDB* phy_db_ptr, severity severity_level,
           const std::string& log_file_name) {
  phy_db_ptr_ = phy_db_ptr;
  severity_level_ = severity_level;
  log_file_name_ = log_file_name;
  LoadParamsFromConfig();
  InitLogging(log_file_name_, severity_level_, disable_log_prefix_);
}

void Dali::SetGuiSnapshotSinkFactory(SnapshotSinkFactory factory) {
  gui_snapshot_sink_factory_ = std::move(factory);
}

void Dali::ShowParamsList() {
  LOG(info)
      << "Dali runtime parameters:\n"
      << "  log_file_name: " << log_file_name_ << "\n"
      << "  disable_log_prefix: " << disable_log_prefix_ << "\n"
      << "  num_threads: " << num_threads_ << "\n"
      << "  well_legalization_mode: "
      << static_cast<int>(well_legalization_mode_) << "\n"
      << "  disable_global_place: " << disable_global_place_ << "\n"
      << "  disable_legalization: " << disable_legalization_ << "\n"
      << "  disable_detailed_place: " << disable_detailed_place_ << "\n"
      << "  disable_io_place: " << disable_io_place_ << "\n"
      << "  target_density: " << target_density_ << "\n"
      << "  net_ignore_threshold: " << net_ignore_threshold_ << "\n"
      << "  io_metal_layer: " << io_metal_layer_ << "\n"
      << "  disable_welltap: " << disable_welltap_ << "\n"
      << "  well_tap_pattern: " << WellTapPatternName(well_tap_pattern_) << "\n"
      << "  disable_cell_flip: " << disable_cell_flip_ << "\n"
      << "  max_row_width: " << max_row_width_ << "\n"
      << "  enable_adaptive_stripe_boundaries: "
      << enable_adaptive_stripe_boundaries_ << "\n"
      << "  is_standard_cell: " << is_standard_cell_ << "\n"
      << "  enable_filler_cell: " << enable_filler_cell_ << "\n"
      << "  enable_end_cap_cell: " << enable_end_cap_cell_ << "\n"
      << "  enable_gridded_global_capacity: " << enable_gridded_global_capacity_
      << "\n"
      << "  enable_gridded_upper_bound_refiner: "
      << enable_gridded_upper_bound_refiner_ << "\n"
      << "  enable_gridded_upper_bound_balancing: "
      << enable_gridded_upper_bound_balancing_ << "\n"
      << "  enable_gridded_evacuated_component_feedback: "
      << enable_gridded_evacuated_component_feedback_ << "\n"
      << "  disable_gridded_feedback_rollback: "
      << disable_gridded_feedback_rollback_ << "\n"
      << "  enable_gridded_legalization_pressure: "
      << enable_gridded_legalization_pressure_ << "\n"
      << "  gridded_legalization_feedback_mode: "
      << static_cast<int>(gridded_legalization_feedback_mode_) << "\n"
      << "  enable_gridded_stripe_balancing: "
      << enable_gridded_stripe_balancing_ << "\n"
      << "  enable_banded_stripe_assignment: "
      << enable_banded_stripe_assignment_ << "\n"
      << "  banded_stripe_assignment_bands: " << banded_stripe_assignment_bands_
      << "\n"
      << "  banded_stripe_assignment_min_hpwl_gain: "
      << banded_stripe_assignment_min_hpwl_gain_ << "\n"
      << "  enable_gridded_local_reorder: " << enable_gridded_local_reorder_
      << "\n"
      << "  enable_gridded_detailed_placement: "
      << enable_gridded_detailed_placement_ << "\n"
      << "  enable_gridded_detailed_relocation: "
      << enable_gridded_detailed_relocation_ << "\n"
      << "  enable_gridded_assignment_batch: "
      << enable_gridded_assignment_batch_ << "\n"
      << "  enable_gridded_exhaustive_insertion: "
      << enable_gridded_exhaustive_insertion_ << "\n"
      << "  gridded_detailed_max_candidate_rows: "
      << gridded_detailed_max_candidate_rows_ << "\n"
      << "  gridded_detailed_max_rounds: " << gridded_detailed_max_rounds_
      << "\n"
      << "  gridded_detailed_min_relative_improvement: "
      << gridded_detailed_min_relative_improvement_ << "\n"
      << "  disable_gridded_vertical_swap: " << disable_gridded_vertical_swap_
      << "\n"
      << "  enable_gridded_row_y_optimization: "
      << enable_gridded_row_y_optimization_ << "\n"
      << "  enable_vertical_hpwl_row_assignment: "
      << enable_vertical_hpwl_row_assignment_ << "\n"
      << "  enable_vertical_hpwl_row_assignment_preview: "
      << enable_vertical_hpwl_row_assignment_preview_ << "\n"
      << "  enable_vertical_hpwl_row_assignment_local_closure: "
      << enable_vertical_hpwl_row_assignment_local_closure_ << "\n"
      << "  vertical_hpwl_row_assignment_closure_windows: "
      << vertical_hpwl_row_assignment_closure_windows_ << "\n"
      << "  enable_ortools_row_optimization: "
      << enable_ortools_row_optimization_ << "\n"
      << "  analyze_exact_gridded_legalization: "
      << analyze_exact_gridded_legalization_ << "\n"
      << "  analyze_exact_adjacent_rows: " << analyze_exact_adjacent_rows_
      << "\n"
      << "  analyze_exact_row_geometry: " << analyze_exact_row_geometry_ << "\n"
      << "  exact_gridded_window_components: "
      << exact_gridded_window_components_ << "\n"
      << "  exact_gridded_max_windows: " << exact_gridded_max_windows_ << "\n"
      << "  exact_gridded_window_time: " << exact_gridded_window_time_ << "\n"
      << "  exact_gridded_max_row_changes: " << exact_gridded_max_row_changes_
      << "\n"
      << "  solve_exact_gridded_legalization: "
      << solve_exact_gridded_legalization_ << "\n"
      << "  exact_gridded_solve_time: " << exact_gridded_solve_time_ << "\n"
      << "  exact_gridded_row_radius: " << exact_gridded_row_radius_ << "\n"
      << "  exact_gridded_use_solution_hint: "
      << exact_gridded_use_solution_hint_ << "\n"
      << "  exact_gridded_log_search_progress: "
      << exact_gridded_log_search_progress_ << "\n"
      << "  enable_exact_gridded_stripe_optimization: "
      << enable_exact_gridded_stripe_optimization_ << "\n"
      << "  exact_gridded_stripe_time: " << exact_gridded_stripe_time_ << "\n"
      << "  exact_gridded_stripe_total_time: "
      << exact_gridded_stripe_total_time_ << "\n"
      << "  exact_gridded_stripe_sweeps: " << exact_gridded_stripe_sweeps_
      << "\n"
      << "  exact_gridded_stripe_components: "
      << exact_gridded_stripe_components_ << "\n"
      << "  exact_gridded_stripe_row_radius: "
      << exact_gridded_stripe_row_radius_ << "\n"
      << "  exact_gridded_stripe_displacement_weight: "
      << exact_gridded_stripe_displacement_weight_ << "\n"
      << "  exact_gridded_stripe_fixed_row_prepass: "
      << exact_gridded_stripe_fixed_row_prepass_ << "\n"
      << "  exact_gridded_stripe_before_detailed: "
      << exact_gridded_stripe_before_detailed_ << "\n"
      << "  exact_gridded_stripe_local_closure: "
      << exact_gridded_stripe_local_closure_ << "\n"
      << "  enable_exact_gridded_boundary_optimization: "
      << enable_exact_gridded_boundary_optimization_ << "\n"
      << "  exact_gridded_boundary_before_detailed: "
      << exact_gridded_boundary_before_detailed_ << "\n"
      << "  exact_gridded_boundary_local_closure: "
      << exact_gridded_boundary_local_closure_ << "\n"
      << "  exact_gridded_boundary_time: " << exact_gridded_boundary_time_
      << "\n"
      << "  exact_gridded_boundary_total_time: "
      << exact_gridded_boundary_total_time_ << "\n"
      << "  exact_gridded_boundary_components: "
      << exact_gridded_boundary_components_ << "\n"
      << "  exact_gridded_boundary_max_changes: "
      << exact_gridded_boundary_max_changes_ << "\n"
      << "  enable_shrink_off_grid_die_area: "
      << enable_shrink_off_grid_die_area_ << "\n"
      << "  global_initializer: " << static_cast<int>(global_initializer_)
      << "\n"
      << "  global_anchor_schedule: "
      << static_cast<int>(global_anchor_schedule_) << "\n"
      << "  global_grid_schedule: " << static_cast<int>(global_grid_schedule_)
      << "\n"
      << "  global_lal_expansion: "
      << static_cast<int>(global_lal_expansion_mode_) << "\n"
      << "  global_lal_hotspot: " << static_cast<int>(global_lal_hotspot_mode_)
      << "\n"
      << "  global_lal_affine_weight: " << global_lal_affine_weight_ << "\n"
      << "  global_lal_macro_boundary: "
      << static_cast<int>(global_lal_macro_boundary_mode_) << "\n"
      << "  global_min_iterations: " << global_min_iterations_ << "\n"
      << "  global_max_iterations: " << global_max_iterations_ << "\n"
      << "  standard_cell_legalizer_cost: "
      << static_cast<int>(standard_cell_legalizer_cost_mode_) << "\n"
      << "  detailed_max_rounds: " << detailed_max_rounds_ << "\n"
      << "  detailed_max_move_candidates: " << detailed_max_move_candidates_
      << "\n"
      << "  output_name: " << output_name_ << "\n"
      << "  gui_debug: " << gui_debug_ << "\n"
      << "  gui_pause: " << gui_pause_ << "\n"
      << "  debug_placement_region_scale: " << debug_placement_region_scale_
      << "\n";
}

void Dali::LoadParamsFromConfig() {
  LoadStringConfig(ConfigName(prefix_, "log_file_name"), &log_file_name_);

  bool disable_log_prefix = disable_log_prefix_;
  LoadBoolConfig(ConfigName(prefix_, "disable_log_prefix"),
                 &disable_log_prefix);
  SetLogPrefix(disable_log_prefix);

  std::string param_name = ConfigName(prefix_, "num_threads");
  if (ConfigExists(param_name)) {
    SetNumThreads(config_get_int(param_name.c_str()));
  }

  param_name = ConfigName(prefix_, "well_legalization_mode");
  if (ConfigExists(param_name)) {
    std::string model_name = config_get_string(param_name.c_str());
    if (model_name == "scavenge") {
      well_legalization_mode_ = WellPartitionMode::kScavenge;
    } else if (model_name == "strict") {
      well_legalization_mode_ = WellPartitionMode::kStrict;
    } else {
      std::cout << "Ignore unknown well_legalization_mode: " << model_name
                << "\n";
    }
  }

  LoadBoolConfig(ConfigName(prefix_, "disable_global_place"),
                 &disable_global_place_);
  LoadBoolConfig(ConfigName(prefix_, "disable_legalization"),
                 &disable_legalization_);
  LoadBoolConfig(ConfigName(prefix_, "disable_detailed_place"),
                 &disable_detailed_place_);
  LoadBoolConfig(ConfigName(prefix_, "disable_io_place"), &disable_io_place_);
  LoadRealConfig(ConfigName(prefix_, "target_density"), &target_density_);
  LoadIntConfig(ConfigName(prefix_, "net_ignore_threshold"),
                &net_ignore_threshold_);
  DaliExpects(net_ignore_threshold_ >= 100 && net_ignore_threshold_ <= 1000,
              "net_ignore_threshold must be in [100, 1000]");
  LoadIntConfig(ConfigName(prefix_, "io_metal_layer"), &io_metal_layer_);
  LoadBoolConfig(ConfigName(prefix_, "disable_welltap"), &disable_welltap_);
  param_name = ConfigName(prefix_, "well_tap_pattern");
  if (ConfigExists(param_name)) {
    well_tap_pattern_ = ParseWellTapPattern(config_get_string(param_name.c_str()));
  }
  DaliExpects(
      disable_welltap_ || IsWellTapPatternSupported(well_tap_pattern_),
      "well_tap_pattern '" + WellTapPatternName(well_tap_pattern_) +
          "' is not yet supported end-to-end. Supported patterns: " +
          SupportedWellTapPatternList() + ".");
  LoadBoolConfig(ConfigName(prefix_, "disable_cell_flip"), &disable_cell_flip_);
  LoadRealConfig(ConfigName(prefix_, "max_row_width"), &max_row_width_);
  LoadBoolConfig(ConfigName(prefix_, "enable_adaptive_stripe_boundaries"),
                 &enable_adaptive_stripe_boundaries_);
  LoadBoolConfig(ConfigName(prefix_, "is_standard_cell"), &is_standard_cell_);
  LoadBoolConfig(ConfigName(prefix_, "enable_filler_cell"),
                 &enable_filler_cell_);
  LoadBoolConfig(ConfigName(prefix_, "enable_end_cap_cell"),
                 &enable_end_cap_cell_);
  LoadBoolConfig(ConfigName(prefix_, "enable_gridded_global_capacity"),
                 &enable_gridded_global_capacity_);
  LoadBoolConfig(ConfigName(prefix_, "enable_gridded_upper_bound_refiner"),
                 &enable_gridded_upper_bound_refiner_);
  LoadBoolConfig(ConfigName(prefix_, "enable_gridded_upper_bound_balancing"),
                 &enable_gridded_upper_bound_balancing_);
  LoadBoolConfig(
      ConfigName(prefix_, "enable_gridded_evacuated_component_feedback"),
      &enable_gridded_evacuated_component_feedback_);
  LoadBoolConfig(ConfigName(prefix_, "disable_gridded_feedback_rollback"),
                 &disable_gridded_feedback_rollback_);
  LoadBoolConfig(ConfigName(prefix_, "enable_gridded_legalization_pressure"),
                 &enable_gridded_legalization_pressure_);
  bool disable_gridded_legalization_feedback = false;
  LoadBoolConfig(ConfigName(prefix_, "disable_gridded_legalization_feedback"),
                 &disable_gridded_legalization_feedback);
  if (disable_gridded_legalization_feedback) {
    gridded_legalization_feedback_mode_ = GlobalRefinementFeedbackMode::kNone;
  }
  param_name = ConfigName(prefix_, "gridded_legalization_feedback");
  if (ConfigExists(param_name)) {
    gridded_legalization_feedback_mode_ = ParseGlobalRefinementFeedbackMode(
        config_get_string(param_name.c_str()));
  }
  LoadBoolConfig(ConfigName(prefix_, "enable_gridded_stripe_balancing"),
                 &enable_gridded_stripe_balancing_);
  LoadBoolConfig(ConfigName(prefix_, "enable_banded_stripe_assignment"),
                 &enable_banded_stripe_assignment_);
  LoadIntConfig(ConfigName(prefix_, "banded_stripe_assignment_bands"),
                &banded_stripe_assignment_bands_);
  DaliExpects(banded_stripe_assignment_bands_ >= 1 &&
                  banded_stripe_assignment_bands_ <= 1024,
              "banded_stripe_assignment_bands must be in [1, 1024]");
  LoadRealConfig(ConfigName(prefix_, "banded_stripe_assignment_min_hpwl_gain"),
                 &banded_stripe_assignment_min_hpwl_gain_);
  DaliExpects(banded_stripe_assignment_min_hpwl_gain_ >= 0.0,
              "banded_stripe_assignment_min_hpwl_gain must be non-negative");
  LoadBoolConfig(ConfigName(prefix_, "enable_gridded_local_reorder"),
                 &enable_gridded_local_reorder_);
  LoadBoolConfig(ConfigName(prefix_, "enable_gridded_detailed_placement"),
                 &enable_gridded_detailed_placement_);
  LoadBoolConfig(ConfigName(prefix_, "enable_gridded_detailed_relocation"),
                 &enable_gridded_detailed_relocation_);
  LoadBoolConfig(ConfigName(prefix_, "enable_gridded_assignment_batch"),
                 &enable_gridded_assignment_batch_);
  LoadBoolConfig(ConfigName(prefix_, "enable_gridded_exhaustive_insertion"),
                 &enable_gridded_exhaustive_insertion_);
  LoadIntConfig(ConfigName(prefix_, "gridded_detailed_max_candidate_rows"),
                &gridded_detailed_max_candidate_rows_);
  DaliExpects(gridded_detailed_max_candidate_rows_ >= 1 &&
                  gridded_detailed_max_candidate_rows_ <= 32,
              "gridded_detailed_max_candidate_rows must be in [1, 32]");
  LoadIntConfig(ConfigName(prefix_, "gridded_detailed_max_rounds"),
                &gridded_detailed_max_rounds_);
  DaliExpects(gridded_detailed_max_rounds_ >= 0,
              "gridded_detailed_max_rounds must be non-negative");
  LoadRealConfig(
      ConfigName(prefix_, "gridded_detailed_min_relative_improvement"),
      &gridded_detailed_min_relative_improvement_);
  DaliExpects(gridded_detailed_min_relative_improvement_ >= 0.0 &&
                  gridded_detailed_min_relative_improvement_ <= 1.0,
              "gridded_detailed_min_relative_improvement must be in [0, 1]");
  LoadBoolConfig(ConfigName(prefix_, "disable_gridded_vertical_swap"),
                 &disable_gridded_vertical_swap_);
  LoadBoolConfig(ConfigName(prefix_, "enable_gridded_row_y_optimization"),
                 &enable_gridded_row_y_optimization_);
  LoadBoolConfig(ConfigName(prefix_, "enable_vertical_hpwl_row_assignment"),
                 &enable_vertical_hpwl_row_assignment_);
  LoadBoolConfig(
      ConfigName(prefix_, "enable_vertical_hpwl_row_assignment_preview"),
      &enable_vertical_hpwl_row_assignment_preview_);
  LoadBoolConfig(
      ConfigName(prefix_, "enable_vertical_hpwl_row_assignment_local_closure"),
      &enable_vertical_hpwl_row_assignment_local_closure_);
  LoadIntConfig(
      ConfigName(prefix_, "vertical_hpwl_row_assignment_closure_windows"),
      &vertical_hpwl_row_assignment_closure_windows_);
  DaliExpects(vertical_hpwl_row_assignment_closure_windows_ > 0,
              "vertical_hpwl_row_assignment_closure_windows must be positive");
  LoadBoolConfig(ConfigName(prefix_, "enable_ortools_row_optimization"),
                 &enable_ortools_row_optimization_);
  LoadBoolConfig(ConfigName(prefix_, "analyze_exact_gridded_legalization"),
                 &analyze_exact_gridded_legalization_);
  LoadBoolConfig(ConfigName(prefix_, "analyze_exact_adjacent_rows"),
                 &analyze_exact_adjacent_rows_);
  LoadBoolConfig(ConfigName(prefix_, "analyze_exact_row_geometry"),
                 &analyze_exact_row_geometry_);
  if (analyze_exact_adjacent_rows_ || analyze_exact_row_geometry_) {
    analyze_exact_gridded_legalization_ = true;
  }
  DaliExpects(!(analyze_exact_adjacent_rows_ && analyze_exact_row_geometry_),
              "exact row-assignment and row-geometry analyses are mutually "
              "exclusive");
  LoadIntConfig(ConfigName(prefix_, "exact_gridded_window_components"),
                &exact_gridded_window_components_);
  DaliExpects(exact_gridded_window_components_ > 0,
              "exact_gridded_window_components must be positive");
  LoadIntConfig(ConfigName(prefix_, "exact_gridded_max_windows"),
                &exact_gridded_max_windows_);
  DaliExpects(exact_gridded_max_windows_ > 0,
              "exact_gridded_max_windows must be positive");
  LoadRealConfig(ConfigName(prefix_, "exact_gridded_window_time"),
                 &exact_gridded_window_time_);
  DaliExpects(exact_gridded_window_time_ > 0.0,
              "exact_gridded_window_time must be positive");
  LoadIntConfig(ConfigName(prefix_, "exact_gridded_max_row_changes"),
                &exact_gridded_max_row_changes_);
  DaliExpects(exact_gridded_max_row_changes_ >= -1,
              "exact_gridded_max_row_changes must be at least negative one");
  LoadBoolConfig(ConfigName(prefix_, "solve_exact_gridded_legalization"),
                 &solve_exact_gridded_legalization_);
  LoadRealConfig(ConfigName(prefix_, "exact_gridded_solve_time"),
                 &exact_gridded_solve_time_);
  DaliExpects(exact_gridded_solve_time_ > 0.0,
              "exact_gridded_solve_time must be positive");
  LoadIntConfig(ConfigName(prefix_, "exact_gridded_row_radius"),
                &exact_gridded_row_radius_);
  DaliExpects(exact_gridded_row_radius_ >= 0,
              "exact_gridded_row_radius must be non-negative");
  LoadBoolConfig(ConfigName(prefix_, "exact_gridded_use_solution_hint"),
                 &exact_gridded_use_solution_hint_);
  LoadBoolConfig(ConfigName(prefix_, "exact_gridded_log_search_progress"),
                 &exact_gridded_log_search_progress_);
  LoadBoolConfig(
      ConfigName(prefix_, "enable_exact_gridded_stripe_optimization"),
      &enable_exact_gridded_stripe_optimization_);
  LoadRealConfig(ConfigName(prefix_, "exact_gridded_stripe_time"),
                 &exact_gridded_stripe_time_);
  DaliExpects(exact_gridded_stripe_time_ > 0.0,
              "exact_gridded_stripe_time must be positive");
  LoadRealConfig(ConfigName(prefix_, "exact_gridded_stripe_total_time"),
                 &exact_gridded_stripe_total_time_);
  DaliExpects(exact_gridded_stripe_total_time_ > 0.0,
              "exact_gridded_stripe_total_time must be positive");
  LoadIntConfig(ConfigName(prefix_, "exact_gridded_stripe_sweeps"),
                &exact_gridded_stripe_sweeps_);
  DaliExpects(exact_gridded_stripe_sweeps_ > 0,
              "exact_gridded_stripe_sweeps must be positive");
  LoadIntConfig(ConfigName(prefix_, "exact_gridded_stripe_components"),
                &exact_gridded_stripe_components_);
  DaliExpects(exact_gridded_stripe_components_ >= 0,
              "exact_gridded_stripe_components must be non-negative");
  LoadIntConfig(ConfigName(prefix_, "exact_gridded_stripe_row_radius"),
                &exact_gridded_stripe_row_radius_);
  DaliExpects(exact_gridded_stripe_row_radius_ >= 0,
              "exact_gridded_stripe_row_radius must be non-negative");
  LoadRealConfig(
      ConfigName(prefix_, "exact_gridded_stripe_displacement_weight"),
      &exact_gridded_stripe_displacement_weight_);
  DaliExpects(std::isfinite(exact_gridded_stripe_displacement_weight_) &&
                  exact_gridded_stripe_displacement_weight_ >= 0.0,
              "exact_gridded_stripe_displacement_weight must be non-negative");
  LoadBoolConfig(ConfigName(prefix_, "exact_gridded_stripe_fixed_row_prepass"),
                 &exact_gridded_stripe_fixed_row_prepass_);
  LoadBoolConfig(ConfigName(prefix_, "exact_gridded_stripe_before_detailed"),
                 &exact_gridded_stripe_before_detailed_);
  LoadBoolConfig(ConfigName(prefix_, "exact_gridded_stripe_local_closure"),
                 &exact_gridded_stripe_local_closure_);
  LoadBoolConfig(
      ConfigName(prefix_, "enable_exact_gridded_boundary_optimization"),
      &enable_exact_gridded_boundary_optimization_);
  LoadBoolConfig(ConfigName(prefix_, "exact_gridded_boundary_before_detailed"),
                 &exact_gridded_boundary_before_detailed_);
  LoadBoolConfig(ConfigName(prefix_, "exact_gridded_boundary_local_closure"),
                 &exact_gridded_boundary_local_closure_);
  LoadRealConfig(ConfigName(prefix_, "exact_gridded_boundary_time"),
                 &exact_gridded_boundary_time_);
  DaliExpects(exact_gridded_boundary_time_ > 0.0,
              "exact_gridded_boundary_time must be positive");
  LoadRealConfig(ConfigName(prefix_, "exact_gridded_boundary_total_time"),
                 &exact_gridded_boundary_total_time_);
  DaliExpects(exact_gridded_boundary_total_time_ > 0.0,
              "exact_gridded_boundary_total_time must be positive");
  LoadIntConfig(ConfigName(prefix_, "exact_gridded_boundary_components"),
                &exact_gridded_boundary_components_);
  DaliExpects(exact_gridded_boundary_components_ > 0,
              "exact_gridded_boundary_components must be positive");
  LoadIntConfig(ConfigName(prefix_, "exact_gridded_boundary_max_changes"),
                &exact_gridded_boundary_max_changes_);
  DaliExpects(exact_gridded_boundary_max_changes_ >= -1,
              "exact_gridded_boundary_max_changes must be at least -1");
  LoadBoolConfig(ConfigName(prefix_, "enable_shrink_off_grid_die_area"),
                 &enable_shrink_off_grid_die_area_);
  param_name = ConfigName(prefix_, "global_initializer");
  if (ConfigExists(param_name)) {
    global_initializer_ =
        ParseGlobalInitializer(config_get_string(param_name.c_str()));
  }
  param_name = ConfigName(prefix_, "global_anchor_schedule");
  if (ConfigExists(param_name)) {
    global_anchor_schedule_ =
        ParseGlobalAnchorSchedule(config_get_string(param_name.c_str()));
  }
  param_name = ConfigName(prefix_, "global_grid_schedule");
  if (ConfigExists(param_name)) {
    global_grid_schedule_ =
        ParseGlobalGridSchedule(config_get_string(param_name.c_str()));
  }
  param_name = ConfigName(prefix_, "global_lal_expansion");
  if (ConfigExists(param_name)) {
    global_lal_expansion_mode_ =
        ParseGlobalLalExpansionMode(config_get_string(param_name.c_str()));
  }
  param_name = ConfigName(prefix_, "global_lal_hotspot");
  if (ConfigExists(param_name)) {
    global_lal_hotspot_mode_ =
        ParseGlobalLalHotspotMode(config_get_string(param_name.c_str()));
  }
  LoadRealConfig(ConfigName(prefix_, "global_lal_affine_weight"),
                 &global_lal_affine_weight_);
  DaliExpects(
      global_lal_affine_weight_ >= 0.0 && global_lal_affine_weight_ <= 1.0,
      "global_lal_affine_weight must be in [0, 1]");
  param_name = ConfigName(prefix_, "global_lal_macro_boundary");
  if (ConfigExists(param_name)) {
    global_lal_macro_boundary_mode_ =
        ParseGlobalLalMacroBoundaryMode(config_get_string(param_name.c_str()));
  }
  LoadIntConfig(ConfigName(prefix_, "global_min_iterations"),
                &global_min_iterations_);
  DaliExpects(global_min_iterations_ >= 0,
              "global_min_iterations must be non-negative");
  LoadIntConfig(ConfigName(prefix_, "global_max_iterations"),
                &global_max_iterations_);
  DaliExpects(global_max_iterations_ >= 0,
              "global_max_iterations must be non-negative");
  param_name = ConfigName(prefix_, "standard_cell_legalizer_cost");
  if (ConfigExists(param_name)) {
    standard_cell_legalizer_cost_mode_ = ParseStandardCellLegalizerCostMode(
        config_get_string(param_name.c_str()));
  }
  LoadIntConfig(ConfigName(prefix_, "detailed_max_rounds"),
                &detailed_max_rounds_);
  DaliExpects(detailed_max_rounds_ >= 0,
              "detailed_max_rounds must be non-negative");
  LoadIntConfig(ConfigName(prefix_, "detailed_max_move_candidates"),
                &detailed_max_move_candidates_);
  DaliExpects(detailed_max_move_candidates_ >= 0,
              "detailed_max_move_candidates must be non-negative");
  LoadStringConfig(ConfigName(prefix_, "output_name"), &output_name_);
  LoadBoolConfig(ConfigName(prefix_, "gui_debug"), &gui_debug_);
  LoadStringConfig(ConfigName(prefix_, "gui_pause"), &gui_pause_);
  LoadRealConfig(ConfigName(prefix_, "debug_placement_region_scale"),
                 &debug_placement_region_scale_);
  DaliExpects(debug_placement_region_scale_ >= 1.0,
              "debug_placement_region_scale must be at least 1");
}

void Dali::SetLogPrefix(bool disable_log_prefix) {
  disable_log_prefix_ = disable_log_prefix;
}

void Dali::SetNumThreads(int num_threads) {
  DaliExpects(num_threads >= 1, "Number of threads must be positive");
  num_threads_ = num_threads;
}

Circuit& Dali::GetCircuit() { return circuit_; }

phydb::PhyDB* Dali::GetPhyDBPtr() { return phy_db_ptr_; }

Dali::RuntimeOptions Dali::GetRuntimeOptions() const {
  return RuntimeOptions{
      log_file_name_,
      disable_log_prefix_,
      num_threads_,
      well_legalization_mode_,
      disable_global_place_,
      disable_legalization_,
      disable_detailed_place_,
      disable_io_place_,
      target_density_,
      net_ignore_threshold_,
      io_metal_layer_,
      disable_welltap_,
      well_tap_pattern_,
      disable_cell_flip_,
      max_row_width_,
      enable_adaptive_stripe_boundaries_,
      is_standard_cell_,
      enable_filler_cell_,
      enable_end_cap_cell_,
      enable_gridded_global_capacity_,
      enable_gridded_upper_bound_refiner_,
      enable_gridded_upper_bound_balancing_,
      enable_gridded_evacuated_component_feedback_,
      disable_gridded_feedback_rollback_,
      enable_gridded_legalization_pressure_,
      gridded_legalization_feedback_mode_,
      enable_gridded_stripe_balancing_,
      enable_banded_stripe_assignment_,
      banded_stripe_assignment_bands_,
      banded_stripe_assignment_min_hpwl_gain_,
      enable_gridded_local_reorder_,
      enable_gridded_detailed_placement_,
      enable_gridded_detailed_relocation_,
      enable_gridded_assignment_batch_,
      enable_gridded_exhaustive_insertion_,
      gridded_detailed_max_candidate_rows_,
      gridded_detailed_max_rounds_,
      gridded_detailed_min_relative_improvement_,
      disable_gridded_vertical_swap_,
      enable_gridded_row_y_optimization_,
      enable_vertical_hpwl_row_assignment_,
      enable_vertical_hpwl_row_assignment_preview_,
      enable_vertical_hpwl_row_assignment_local_closure_,
      vertical_hpwl_row_assignment_closure_windows_,
      enable_ortools_row_optimization_,
      analyze_exact_gridded_legalization_,
      analyze_exact_adjacent_rows_,
      analyze_exact_row_geometry_,
      exact_gridded_window_components_,
      exact_gridded_max_windows_,
      exact_gridded_window_time_,
      exact_gridded_max_row_changes_,
      solve_exact_gridded_legalization_,
      exact_gridded_solve_time_,
      exact_gridded_row_radius_,
      exact_gridded_use_solution_hint_,
      exact_gridded_log_search_progress_,
      enable_exact_gridded_stripe_optimization_,
      exact_gridded_stripe_time_,
      exact_gridded_stripe_total_time_,
      exact_gridded_stripe_sweeps_,
      exact_gridded_stripe_components_,
      exact_gridded_stripe_row_radius_,
      exact_gridded_stripe_displacement_weight_,
      exact_gridded_stripe_fixed_row_prepass_,
      exact_gridded_stripe_before_detailed_,
      exact_gridded_stripe_local_closure_,
      enable_exact_gridded_boundary_optimization_,
      exact_gridded_boundary_before_detailed_,
      exact_gridded_boundary_local_closure_,
      exact_gridded_boundary_time_,
      exact_gridded_boundary_total_time_,
      exact_gridded_boundary_components_,
      exact_gridded_boundary_max_changes_,
      enable_shrink_off_grid_die_area_,
      global_initializer_,
      global_anchor_schedule_,
      global_grid_schedule_,
      global_lal_expansion_mode_,
      global_lal_hotspot_mode_,
      global_lal_affine_weight_,
      global_lal_macro_boundary_mode_,
      global_min_iterations_,
      global_max_iterations_,
      standard_cell_legalizer_cost_mode_,
      detailed_max_rounds_,
      detailed_max_move_candidates_,
      output_name_,
      gui_debug_,
      gui_pause_,
      debug_placement_region_scale_,
  };
}

bool Dali::SetIoPlacerGlobalMetalLayer(std::string const& layer_name) {
  InitializeCircuitFromPhyDBIfNeeded();
  DaliExpects(io_placer_ != nullptr, "Please initialize I/O placer first");
  bool is_metal_name = circuit_.IsMetalLayerExisting(layer_name);
  if (is_metal_name) {
    MetalLayer* metal_layer = circuit_.GetMetalLayerPtr(layer_name);
    return io_placer_->SetGlobalMetalLayer(metal_layer->Id());
  }
  return false;
}

bool Dali::ConfigIoPlacer() { return true; }

bool Dali::RunIoPinAutoPlacement() {
  InitializeCircuitFromPhyDBIfNeeded();
  DaliExpects(io_placer_ != nullptr, "Please initialize I/O placer first");
  return io_placer_->RunAutoPlacement();
}

void Dali::ReportIoPlacementUsage() {
  LOG(info)
      << "\033[0;36m"
      << "Usage: place-io (followed by one of the options below)\n"
      << "  -h/--help\n"
      << "      print out the IO placer usage\n"
      << "  -a/--add <pin_name> <net_name> <direction> <use>\n"
      << "      add an IOPIN\n"
      << "  -p/--place <pin_name> <metal_name> <lx> <ly> <ux> <uy> <x> <y> "
         "<orientation>\n"
      << "      manually place an IOPIN\n"
      << "  -c/--config (use -h to see more usage)\n"
      << "      set parameters for automatic IOPIN placement\n"
      << "  -ap/--auto-place\n"
      << "      automatically place all unplaced IOPINs, which is also the "
         "default option"
      << "\033[0m\n";
}

bool Dali::IoPinPlacement(int argc, char** argv) {
  if (argc < 2) {
    ReportIoPlacementUsage();
    return false;
  }

  std::string option_str(argv[1]);
  InstantiateIoPlacer();
  if (option_str == "-h" or option_str == "--help") {
    ReportIoPlacementUsage();
    return true;
  }

  // remove "place-io" and option flag before calling each function
  if (option_str == "-c" or option_str == "--config") {
    return io_placer_->ConfigCmd(argc - 2, argv + 2);
  } else if (option_str == "-ap" or option_str == "--auto-place") {
    return io_placer_->AutoPlaceCmd(argc - 2, argv + 2);
  } else {
    LOG(warning) << "IoPlace flag not specified, use --auto-place by default\n";
    return io_placer_->AutoPlaceCmd(argc - 1, argv + 1);
  }
}

bool Dali::ShouldPerformTimingDrivenPlacement() {
  return phy_db_ptr_->GetTimingApi().ReadyForTimingDriven();
}

void Dali::InitializeRCEstimator() {
  rc_estimator = std::make_unique<StarPiModelEstimator>(phy_db_ptr_);
}

#if PHYDB_USE_GALOIS
void Dali::FetchSlacks() {
  phydb::ActPhyDBTimingAPI& timing_api = phy_db_ptr_->GetTimingApi();
  std::cout << "Number of timing constraints: "
            << timing_api.GetNumConstraints() << "\n";
  for (int i = 0; i < (int)timing_api.GetNumConstraints(); ++i) {
    double slack = timing_api.GetSlack(i);
    std::cout << "Slack for timing constraint " << i << " " << slack << "\n";
    if (slack < 0) {
      phydb::PhydbPath fast_path;
      timing_api.GetFastWitness(i, fast_path);
      std::cout << "Fast path size: " << fast_path.edges.size() << "\n";
      phydb::PhydbPath slow_path;
      timing_api.GetSlowWitness(i, fast_path);
      std::cout << "Fast path size: " << slow_path.edges.size() << "\n";
    }
  }
}

void Dali::InitializeTimingDrivenPlacement() {
  phy_db_ptr_->CreatePhydbActAdaptor();
  phy_db_ptr_->AddNetsAndCompPinsToSpefManager();
  InitializeRCEstimator();
}

void Dali::UpdateRCs() { rc_estimator->PushNetRCToManager(); }

void Dali::PerformTimingAnalysis() {
  phydb::ActPhyDBTimingAPI& timing_api = phy_db_ptr_->GetTimingApi();
  timing_api.UpdateTimingIncremental();
}

void Dali::UpdateNetWeights() { FetchSlacks(); }

void Dali::ReportPerformance() {
  if (!phy_db_ptr_->GetTimingApi().ReadyForTimingDriven()) return;
}

bool Dali::TimingDrivenPlacement(double density, int number_of_threads) {
  bool is_success = true;
  InitializeTimingDrivenPlacement();
  for (int i = 0; i < max_td_place_num_; ++i) {
    GlobalPlace(density, number_of_threads);
    is_success = UnifiedLegalization();
    UpdateRCs();
    PerformTimingAnalysis();
    UpdateNetWeights();
  }
  ReportPerformance();
  return is_success;
}

#endif

void Dali::ApplyPlacementOverrides(double density, int number_of_threads) {
  if (density > 0) {
    DaliExpects(density <= 1, "Target density must be in the range (0, 1]");
    target_density_ = density;
  }
  if (number_of_threads >= 1) {
    num_threads_ = number_of_threads;
  }
}

void Dali::InitializeMainPlacementCircuit() {
  circuit_.SetEnableShrinkOffGridDieArea(enable_shrink_off_grid_die_area_);
  circuit_.InitializeFromPhyDB(phy_db_ptr_);
  ApplyDebugPlacementRegionScale();
  is_circuit_initialized_ = true;
  circuit_.ReportBriefSummary();
  ClearPlacementMetrics();
  RecordPlacementHpwlMetrics("input", circuit_);
  RecordPlacementLowerBounds();
  InitializeVisualizationSnapshots();
  WriteVisualizationSnapshot("input", "Input", "input");
}

void Dali::RecordPlacementLowerBounds() {
  HpwlLowerBound fixed_terminal = ComputeFixedTerminalHpwlLowerBound(circuit_);
  HpwlLowerBound placement_box = ComputePlacementBoxHpwlLowerBound(circuit_);

  RecordPlacementMetric("lower_bound.fixed_terminal", fixed_terminal.Total());
  RecordPlacementMetric("lower_bound.fixed_terminal.x", fixed_terminal.x);
  RecordPlacementMetric("lower_bound.fixed_terminal.y", fixed_terminal.y);
  RecordPlacementMetric("lower_bound.placement_box", placement_box.Total());
  RecordPlacementMetric("lower_bound.placement_box.x", placement_box.x);
  RecordPlacementMetric("lower_bound.placement_box.y", placement_box.y);

  LOG(info) << "HPWL lower bounds:\n"
            << "  fixed-terminal bound : " << fixed_terminal.Total() << "um\n"
            << "  placement-box bound  : " << placement_box.Total() << "um\n";
}

void Dali::ApplyDebugPlacementRegionScale() {
  if (debug_placement_region_scale_ == 1.0) return;

  const int original_left = circuit_.RegionLLX();
  const int original_right = circuit_.RegionURX();
  const int original_bottom = circuit_.RegionLLY();
  const int original_top = circuit_.RegionURY();
  circuit_.ExpandPlacementRegion(debug_placement_region_scale_);
  LOG(warning) << "Debug placement-region expansion enabled:\n"
               << "  scale factor      : " << debug_placement_region_scale_
               << "\n"
               << "  original boundary : [" << original_left << ", "
               << original_bottom << "] - [" << original_right << ", "
               << original_top << "]\n"
               << "  expanded boundary : [" << circuit_.RegionLLX() << ", "
               << circuit_.RegionLLY() << "] - [" << circuit_.RegionURX()
               << ", " << circuit_.RegionURY() << "]\n";
}

void Dali::ResolveTargetDensity() {
  if (target_density_ == -1) {
    double default_density = 0.7;
    if (HasMovableComponents()) {
      target_density_ = std::max(circuit_.WhiteSpaceUsage(), default_density);
    } else {
      target_density_ = default_density;
    }
    LOG(info) << "Target density not provided, set it to default value: "
              << target_density_ << "\n";
  }
}

bool Dali::HasMovableComponents() const {
  return circuit_.TotalMovableComponentCnt() > 0;
}

bool Dali::HasNets() const { return !circuit_.Nets().empty(); }

bool Dali::ShouldRunGlobalPlacement() const {
  return !disable_global_place_ && HasMovableComponents() && HasNets();
}

bool Dali::ShouldRunMovableCellLegalization() const {
  return HasMovableComponents();
}

bool Dali::RunGlobalPlacementStage() {
  ElapsedTime stage_timer;
  stage_timer.RecordStartTime();
  gb_placer_.SetCircuit(&circuit_);
  gb_placer_.SetNumThreads(num_threads_);
  gb_placer_.SetSnapshotCallback(
      [this](const std::string& id, const std::string& label,
             const std::string& subgroup, int iteration) {
        WriteVisualizationSnapshot("global_placement." + id, label,
                                   "global_placement", subgroup, iteration);
      });
  if (disable_global_place_) {
    LOG(info) << "Skip global placement: disabled by configuration\n";
  } else if (!HasMovableComponents()) {
    LOG(info) << "Skip global placement: no movable components\n";
  } else if (!HasNets()) {
    LOG(info) << "Skip global placement: no nets to optimize\n";
  } else if (ShouldRunGlobalPlacement()) {
    gb_placer_.SetPlacementDensity(target_density_);
    gb_placer_.SetNetIgnoreThreshold(net_ignore_threshold_);
    gb_placer_.SetInitializerType(global_initializer_);
    gb_placer_.SetAnchorSchedule(global_anchor_schedule_);
    gb_placer_.SetGridSchedule(global_grid_schedule_);
    gb_placer_.SetLalExpansionMode(global_lal_expansion_mode_);
    gb_placer_.SetLalHotspotMode(global_lal_hotspot_mode_);
    gb_placer_.SetLalAffineScalingWeight(global_lal_affine_weight_);
    gb_placer_.SetLalMacroBoundaryMode(global_lal_macro_boundary_mode_);
    gb_placer_.SetMinIteration(global_min_iterations_);
    gb_placer_.SetMaxIteration(global_max_iterations_);
    const bool needs_gridded_legalizer =
        !is_standard_cell_ && (enable_gridded_global_capacity_ ||
                               enable_gridded_upper_bound_refiner_);
    if (needs_gridded_legalizer) {
      ConfigureWellLegalizer();
    }
    std::shared_ptr<PlacementCapacityModel> capacity_model;
    if (is_standard_cell_ || !enable_gridded_global_capacity_) {
      capacity_model = std::make_shared<AreaCapacityModel>();
    } else {
      LOG(info) << "  Enable experimental gridded global capacity model\n";
      GriddedCapacityConfig capacity_config =
          well_legalizer_.BuildGriddedCapacityConfig(target_density_);
      double demand_normalization =
          well_legalizer_.EstimateGriddedDemandNormalization(capacity_config);
      capacity_model = std::make_shared<GriddedPlacementCapacityModel>(
          capacity_config, demand_normalization);
    }
    if (enable_gridded_legalization_pressure_) {
      if (is_standard_cell_) {
        LOG(warning) << "Ignore gridded legalization pressure for "
                        "standard-cell placement\n";
      } else if (!enable_gridded_upper_bound_refiner_) {
        LOG(warning) << "Ignore gridded legalization pressure without the "
                        "upper-bound refiner\n";
      } else {
        LOG(info) << "  Enable gridded legalization pressure in LAL capacity\n";
        capacity_model =
            std::make_shared<LegalizationPressureCapacityModel>(capacity_model);
      }
    }
    gb_placer_.SetCapacityModel(std::move(capacity_model));
    if (enable_gridded_upper_bound_refiner_) {
      if (is_standard_cell_) {
        LOG(warning) << "Ignore gridded upper-bound refiner for standard-cell "
                        "placement\n";
      } else {
        LOG(info) << "  Enable rough gridded upper-bound refinement on every "
                     "global-placement iteration\n";
        gb_placer_.SetUpperBoundRefiner(
            std::make_unique<RoughGriddedUpperBoundRefiner>(
                &well_legalizer_, enable_gridded_upper_bound_balancing_,
                enable_gridded_evacuated_component_feedback_,
                !disable_gridded_feedback_rollback_),
            0, 1);
        gb_placer_.SetRefinementFeedbackMode(
            gridded_legalization_feedback_mode_);
      }
    }
    if (!gb_placer_.StartPlacement()) {
      LOG(error) << "Global placement failed\n";
      return false;
    }
  }
  WriteVisualizationSnapshot("global_placement.final", "After Global Placement",
                             "global_placement");
  stage_timer.RecordEndTime();
  RecordPlacementMetric("time.global_placement.wall_s",
                        stage_timer.GetWallTime());
  RecordPlacementMetric("time.global_placement.cpu_s",
                        stage_timer.GetCpuTime());
  return true;
}

bool Dali::RunStandardCellLegalization() {
  if (!ShouldRunMovableCellLegalization()) {
    LOG(info) << "Skip standard-cell legalization: no movable components\n";
    return true;
  }
  WriteVisualizationSnapshot("legalization.start", "Before Legalization",
                             "legalization");
  FlushVisualizationEvents();
  Placer* legalizer_for_detailed_placement = &standard_cell_legalizer_;
  standard_cell_legalizer_.CopyPlacementContextFrom(&gb_placer_);
  standard_cell_legalizer_.SetDisableCellFlip(disable_cell_flip_);
  standard_cell_legalizer_.SetCostMode(standard_cell_legalizer_cost_mode_);
  ElapsedTime legalization_timer;
  legalization_timer.RecordStartTime();
  if (!standard_cell_legalizer_.StartPlacement()) {
    LOG(warning) << "Standard-cell legalizer failed; trying "
                    "ExtendedTetrisLegalizer baseline\n";
    legalizer_for_detailed_placement = &legalizer_;
    legalizer_.CopyPlacementContextFrom(&gb_placer_);
    legalizer_.disable_cell_flip_ = disable_cell_flip_;
    if (!legalizer_.StartPlacement()) {
      LOG(error) << "Standard-cell legalization failed\n";
      return false;
    }
  }
  legalization_timer.RecordEndTime();
  RecordPlacementHpwlMetrics("legalization", circuit_);
  RecordPlacementMetric("time.legalization.wall_s",
                        legalization_timer.GetWallTime());
  RecordPlacementMetric("time.legalization.cpu_s",
                        legalization_timer.GetCpuTime());
  detailed_placer_.CopyPlacementContextFrom(legalizer_for_detailed_placement);
  if (!RunDetailedPlacement()) {
    return false;
  }
  return true;
}

bool Dali::RunDetailedPlacement() {
  if (disable_detailed_place_) {
    LOG(info) << "Skip detailed placement: disabled by configuration\n";
    return true;
  }
  detailed_placer_.SetSnapshotCallback(
      [this](const std::string& id, const std::string& label,
             const std::string& subgroup, int iteration) {
        WriteVisualizationSnapshot("detailed_placement." + id, label,
                                   "detailed_placement", subgroup, iteration);
        FlushVisualizationEvents();
      });
  detailed_placer_.SetMaxOptimizationRounds(detailed_max_rounds_);
  detailed_placer_.SetMaxMoveCandidatesPerRound(detailed_max_move_candidates_);
  detailed_placer_.SetNetIgnoreThreshold(net_ignore_threshold_);
  WriteVisualizationSnapshot("detailed_placement.start",
                             "Before Detailed Placement", "detailed_placement");
  FlushVisualizationEvents();
  ElapsedTime stage_timer;
  stage_timer.RecordStartTime();
  if (!detailed_placer_.StartPlacement()) {
    LOG(error) << "Detailed placement failed\n";
    return false;
  }
  stage_timer.RecordEndTime();
  RecordPlacementHpwlMetrics("detailed_placement", circuit_);
  RecordPlacementMetric("time.detailed_placement.wall_s",
                        stage_timer.GetWallTime());
  RecordPlacementMetric("time.detailed_placement.cpu_s",
                        stage_timer.GetCpuTime());
  WriteVisualizationSnapshot("detailed_placement.final",
                             "After Detailed Placement", "detailed_placement");
  return true;
}

void Dali::ConfigureWellLegalizer() {
  well_legalizer_.CopyPlacementContextFrom(&gb_placer_);
  well_legalizer_.disable_welltap_ = disable_welltap_;
  well_legalizer_.SetWellTapPattern(well_tap_pattern_);
  well_legalizer_.disable_cell_flip_ = disable_cell_flip_;
  well_legalizer_.enable_end_cap_cell_ = enable_end_cap_cell_;
  well_legalizer_.SetMaxRowWidth(max_row_width_);
  well_legalizer_.SetEnableAdaptiveStripeBoundaries(
      enable_adaptive_stripe_boundaries_);
  well_legalizer_.SetStripePartitionMode(
      static_cast<int>(well_legalization_mode_));
  well_legalizer_.SetEnableStripeBalancing(enable_gridded_stripe_balancing_);
  well_legalizer_.SetBandedStripeAssignment(
      enable_banded_stripe_assignment_, banded_stripe_assignment_bands_,
      banded_stripe_assignment_min_hpwl_gain_);
  well_legalizer_.SetEnableLocalReorder(enable_gridded_local_reorder_);
  well_legalizer_.SetEnableDetailedPlacement(
      enable_gridded_detailed_placement_);
  well_legalizer_.SetEnableDetailedRelocation(
      enable_gridded_detailed_relocation_);
  well_legalizer_.SetEnableDetailedMoveBatching(
      enable_gridded_assignment_batch_);
  well_legalizer_.SetEnableDetailedExhaustiveInsertion(
      enable_gridded_exhaustive_insertion_);
  well_legalizer_.SetDetailedPlacementMaxCandidateRows(
      gridded_detailed_max_candidate_rows_);
  well_legalizer_.SetDetailedPlacementConvergence(
      gridded_detailed_max_rounds_, gridded_detailed_min_relative_improvement_);
  well_legalizer_.SetEnableDetailedVerticalSwap(
      !disable_gridded_vertical_swap_);
  well_legalizer_.SetDetailedPlacementNetIgnoreThreshold(net_ignore_threshold_);
  well_legalizer_.SetEnableRowLocationOptimization(
      enable_gridded_row_y_optimization_);
  well_legalizer_.SetVerticalHpwlRowAssignment(
      enable_vertical_hpwl_row_assignment_, net_ignore_threshold_);
  well_legalizer_.SetVerticalHpwlRowAssignmentPreview(
      enable_vertical_hpwl_row_assignment_preview_);
  well_legalizer_.SetVerticalHpwlRowAssignmentLocalClosure(
      enable_vertical_hpwl_row_assignment_local_closure_,
      vertical_hpwl_row_assignment_closure_windows_);
  well_legalizer_.SetEnableOrToolsRowOptimization(
      enable_ortools_row_optimization_, net_ignore_threshold_);
  well_legalizer_.SetExactLegalizationAnalysis(
      analyze_exact_gridded_legalization_, analyze_exact_adjacent_rows_,
      analyze_exact_row_geometry_, exact_gridded_window_components_,
      exact_gridded_max_windows_, exact_gridded_window_time_,
      exact_gridded_max_row_changes_, net_ignore_threshold_);
  well_legalizer_.SetWholeDesignExactLegalization(
      solve_exact_gridded_legalization_, exact_gridded_solve_time_,
      num_threads_, exact_gridded_row_radius_, exact_gridded_use_solution_hint_,
      exact_gridded_log_search_progress_, net_ignore_threshold_);
  well_legalizer_.SetExactStripeOptimization(
      enable_exact_gridded_stripe_optimization_, exact_gridded_stripe_time_,
      exact_gridded_stripe_total_time_, exact_gridded_stripe_sweeps_,
      exact_gridded_stripe_components_, num_threads_,
      exact_gridded_stripe_row_radius_, exact_gridded_max_row_changes_,
      exact_gridded_stripe_displacement_weight_,
      exact_gridded_stripe_fixed_row_prepass_,
      exact_gridded_stripe_before_detailed_,
      exact_gridded_stripe_local_closure_, exact_gridded_use_solution_hint_,
      net_ignore_threshold_);
  well_legalizer_.SetExactBoundaryOptimization(
      enable_exact_gridded_boundary_optimization_, exact_gridded_boundary_time_,
      exact_gridded_boundary_total_time_, exact_gridded_boundary_components_,
      exact_gridded_boundary_max_changes_, num_threads_,
      exact_gridded_boundary_before_detailed_,
      exact_gridded_boundary_local_closure_, exact_gridded_use_solution_hint_,
      net_ignore_threshold_);
  well_legalizer_.SetSnapshotCallback(
      [this](const std::string& id, const std::string& label,
             const std::string& group, const std::string& subgroup,
             int iteration) {
        std::vector<PlacementWellRect> well_rects;
        if (ShouldVisualizeWellRects(group, subgroup)) {
          well_rects = well_legalizer_.CollectWellVisualizationRects();
        }
        WriteVisualizationSnapshot(group + "." + id, label, group, subgroup,
                                   iteration, std::move(well_rects));
        FlushVisualizationEvents();
      });
}

void Dali::RunFixedOnlyWellCompletion() {
  LOG(info) << "Skip movable-cell well legalization: no movable components\n";
  well_legalizer_.InitializeWellLegalizer();
  well_legalizer_.RunPhysicalCompletionStages();
}

bool Dali::RunWellLegalization() {
  ConfigureWellLegalizer();
  bool has_movable_components = ShouldRunMovableCellLegalization();
  WriteVisualizationSnapshot("legalization.start", "Before Legalization",
                             "legalization");
  FlushVisualizationEvents();
  if (has_movable_components) {
    if (!well_legalizer_.StartPlacement()) {
      LOG(error) << "Well legalization failed\n";
      return false;
    }
  } else {
    RunFixedOnlyWellCompletion();
  }
  well_legalizer_.EmitDEFWellFile(output_name_, 1);
  return true;
}

bool Dali::RunLegalizationStage() {
  if (!disable_legalization_) {
    if (is_standard_cell_) {
      if (!RunStandardCellLegalization()) {
        return false;
      }
    } else if (!RunWellLegalization()) {
      return false;
    }
  }
  std::vector<PlacementWellRect> final_well_rects;
  if (!is_standard_cell_ && !disable_legalization_) {
    final_well_rects = well_legalizer_.CollectWellVisualizationRects();
  }
  WriteVisualizationSnapshot("legalization.final", "After Legalization",
                             "legalization", "", -1,
                             std::move(final_well_rects));
  return true;
}

bool Dali::RunCorePlacementStages() {
  return RunGlobalPlacementStage() && RunLegalizationStage();
}

bool Dali::RunFillerCellPlacement() {
  if (!enable_filler_cell_) {
    return true;
  }
  filler_cell_placer_.CopyPlacementContextFrom(&gb_placer_);
  filler_cell_placer_.phy_db_ptr_ = phy_db_ptr_;
  filler_cell_placer_.CreateFillerMacros(2);
  if (!filler_cell_placer_.StartPlacement()) {
    LOG(error) << "Filler-cell placement failed\n";
    return false;
  }
  return true;
}

bool Dali::RunIoPinPlacementStage() {
  if (disable_io_place_) {
    return true;
  }
  auto io_placer = std::make_unique<IoPlacer>(phy_db_ptr_, &circuit_);
  bool is_io_placer_config_success =
      io_placer->SetGlobalMetalLayer(io_metal_layer_);
  DaliExpects(is_io_placer_config_success,
              "Cannot successfully configure I/O placer");
  if (!io_placer->RunAutoPlacement()) {
    LOG(error) << "I/O pin placement failed\n";
    return false;
  }
  return true;
}

bool Dali::RunPostPlacementCompletionStages() {
  return RunFillerCellPlacement() && RunIoPinPlacementStage();
}

std::vector<PlacementSnapshotStage> Dali::ExpectedSnapshotStages() const {
  // Declare the stages that will actually run this configuration, in execution
  // order, so consumers (the live GUI) reserve exactly those chart slots.
  std::vector<PlacementSnapshotStage> stages;
  stages.push_back({"global_placement", "Global placement"});
  if (!disable_legalization_) {
    stages.push_back({"legalization", "Legalization"});
    // Detailed placement produces a curve only when it actually runs, which
    // differs between the standard-cell and gridded-cell flows.
    const bool has_detailed =
        is_standard_cell_
            ? !disable_detailed_place_
            : (enable_gridded_detailed_placement_ ||
               enable_gridded_local_reorder_);
    if (has_detailed) {
      stages.push_back({"detailed_placement", "Detailed placement"});
    }
  }
  return stages;
}

void Dali::InitializeVisualizationSnapshots() {
  if (!gui_debug_) {
    snapshot_sink_.reset();
    return;
  }
  if (!gui_snapshot_sink_factory_) {
    LOG(error) << "GUI debug mode requested, but this Dali executable does "
                  "not include a GUI snapshot sink. Rebuild with Qt6 "
                  "available, or configure with -DDALI_GUI=ON to require it.\n";
    snapshot_sink_.reset();
    return;
  }
  std::string design_name = circuit_.design().Name();
  if (design_name.empty() && phy_db_ptr_ != nullptr &&
      phy_db_ptr_->GetDesignPtr() != nullptr) {
    design_name = phy_db_ptr_->GetDesignPtr()->GetName();
  }
  if (design_name.empty()) {
    design_name = "unknown";
  }
  PlacementSnapshotRunMetadata run_metadata;
  run_metadata.design_name = design_name;
  run_metadata.database_microns = circuit_.DistanceMicrons();
  run_metadata.git_commit = get_git_version_short();
  run_metadata.pause_at_every_snapshot = gui_pause_ != "off";
  run_metadata.stages = ExpectedSnapshotStages();

  auto gui_sink = gui_snapshot_sink_factory_();
  gui_sink->StartRun(run_metadata);
  snapshot_sink_ = std::move(gui_sink);
}

void Dali::WriteVisualizationSnapshot(
    const std::string& id, const std::string& label, const std::string& group,
    const std::string& subgroup, int iteration,
    std::vector<PlacementWellRect> well_rects) {
  if (snapshot_sink_ == nullptr || !snapshot_sink_->IsEnabled()) {
    return;
  }
  PlacementSnapshotMetadata metadata;
  metadata.id = id;
  metadata.label = label;
  metadata.group = group;
  metadata.subgroup = subgroup;
  metadata.iteration = iteration;
  metadata.well_rects = std::move(well_rects);
  snapshot_sink_->PublishSnapshot(&circuit_, metadata);
}

void Dali::FlushVisualizationEvents() {
  if (snapshot_sink_ == nullptr || !snapshot_sink_->IsEnabled()) {
    return;
  }
  snapshot_sink_->FlushEvents();
}

void Dali::FinishVisualizationSnapshots() {
  if (snapshot_sink_ == nullptr || !snapshot_sink_->IsEnabled()) {
    return;
  }
  snapshot_sink_->FinishRun();
}

bool Dali::StartPlacement(double density, int number_of_threads) {
  ApplyPlacementOverrides(density, number_of_threads);
  InitializeMainPlacementCircuit();
  ResolveTargetDensity();

  if (!RunCorePlacementStages() || !RunPostPlacementCompletionStages()) {
    return false;
  }

  LOG(debug) << "dali git commit: " << get_git_version_short() << "\n";
  RecordPlacementHpwlMetrics("final", circuit_);
  std::vector<PlacementWellRect> final_well_rects;
  if (!is_standard_cell_ && !disable_legalization_) {
    final_well_rects = well_legalizer_.CollectWellVisualizationRects();
  }
  WriteVisualizationSnapshot("final", "Final", "final", "", -1,
                             std::move(final_well_rects));
  FinishVisualizationSnapshots();

  return true;
}

void Dali::AddWellTaps(phydb::Macro* cell, double cell_interval_microns,
                       bool checkerboard_enabled) {
  StandardRowWellTapInserter tap_inserter(phy_db_ptr_);
  tap_inserter.LoadRows();
  tap_inserter.MarkFixedComponentSites();
  tap_inserter.SetTapMacro(cell);
  tap_inserter.SetMaxTapInterval(cell_interval_microns);
  tap_inserter.SetCheckerboardEnabled(checkerboard_enabled);
  tap_inserter.InsertTaps();
  tap_inserter.ExportToPhyDB();
}

bool Dali::AddWellTaps(int argc, char** argv) {
  phydb::Macro* cell = nullptr;
  double cell_interval_microns = -1;
  bool checkerboard_enabled = false;
  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    if (arg == "-cell" && i < argc) {
      std::string macro_name = std::string(argv[i++]);
      cell = phy_db_ptr_->GetMacroPtr(macro_name);
      if (cell == nullptr) {
        std::cout << "Cannot find well-tap cell: " << macro_name << "\n";
        return false;
      }
      if (cell->GetClass() != phydb::MacroClass::CORE_WELLTAP) {
        std::cout << "Given cell is not well-tap cell\n";
        return false;
      }
    } else if (arg == "-interval" && i < argc) {
      std::string cell_interval_str = std::string(argv[i++]);
      try {
        cell_interval_microns = std::stod(cell_interval_str);
      } catch (...) {
        std::cout << "Invalid well-tap cell interval\n";
        return false;
      }
    } else if (arg == "-checker_board") {
      checkerboard_enabled = true;
    } else {
      std::cout << "Unknown flag\n";
      std::cout << arg << "\n";
      return false;
    }
  }

  if (cell == nullptr) {
    std::cout << "Well-tap cell is required\n";
    return false;
  }
  if (cell_interval_microns <= 0) {
    std::cout << "Well-tap cell interval must be positive\n";
    return false;
  }

  AddWellTaps(cell, cell_interval_microns, checkerboard_enabled);
  return true;
}

/**
 * Perform global placement using a given target density.
 *
 * @param density: target density, reasonable range (0, 1].
 * @param num_threads: number of threads.
 * @return void
 */
bool Dali::GlobalPlace(double density, int num_threads) {
  gb_placer_.SetNumThreads(num_threads);
  gb_placer_.SetCircuit(&circuit_);
  gb_placer_.SetBoundaryFromCircuit();
  gb_placer_.SetPlacementDensity(density);
  return gb_placer_.StartPlacement();
}

/**
 * Perform unified legalization for a gridded cell design.
 * Unified legalization includes removing component overlaps and
 * fixing all N/P well design rule violations.
 *
 * N/P well rectangles, NP/PP rectangles, and clusters will
 * be generated in this step.
 *
 * @param density, target density, reasonable range (0, 1].
 * @return void
 */
bool Dali::UnifiedLegalization() {
  well_legalizer_.CopyPlacementContextFrom(&gb_placer_);
  well_legalizer_.SetStripePartitionMode(int(WellPartitionMode::kScavenge));
  return well_legalizer_.StartPlacement();
}

/**
 * Perform detailed placement using external placers.
 *
 * @param engine, the full path or name of external placer. If name is given,
 *        this placer is required to be in the search path.
 * @param load_dp_result, a boolean variable, if true, then load detailed
 *        placement back to PhyDB database.
 * @return void
 */
void Dali::ExternalDetailedPlaceAndLegalize(std::string const& engine,
                                            bool load_dp_result) {
  // create a script for detailed placement and legalization
  LOG(info) << "Creating detailed placement and legalization script...\n";
  std::string dp_script_name = "dali_" + engine + ".cmd";
  std::string legal_def_file =
      CreateDetailedPlacementAndLegalizationScript(engine, dp_script_name);

  // system call
  LOG(info) << "System call...\n";
  std::string command = engine + " < " + dp_script_name;
  int res = std::system(command.c_str());
  LOG(info) << engine << " return code: " << res << "\n";

  if (load_dp_result) {
    phy_db_ptr_->OverrideComponentLocsFromDef(legal_def_file);
    LOG(info) << "New placement loaded back to PhyDB\n";
  }
}

void Dali::ExportToPhyDB() {
  // 1. COMPONENTS
  ExportComponentsToPhyDB();
  // 2. IOPINs
  ExportIoPinsToPhyDB();
  if (well_legalizer_.ckt_ptr_ != nullptr) {
    // 3. MiniRows
    ExportMiniRowsToPhyDB();
    // 4. NP/PP and Well
    ExportPpNpToPhyDB();
    ExportWellToPhyDB();
  }
}

void Dali::Close() { CloseLogging(); }

void Dali::MaybeExportToLEF(std::string const& input_lef_file_full_name,
                            std::string const& output_lef_name) {
  if (!enable_end_cap_cell_) {
    return;
  }
  circuit_.SaveLefFile(input_lef_file_full_name, output_lef_name);
}

void Dali::ExportToDEF(std::string const& input_def_file_full_name,
                       std::string const& output_def_name) {
  circuit_.SaveDefFile(output_def_name, "", input_def_file_full_name, 1, 1, 2,
                       1);
  circuit_.SaveDefFile(output_def_name, "_io", input_def_file_full_name, 1, 1,
                       1, 1);
  circuit_.SaveDefFile(output_def_name, "_filling", input_def_file_full_name, 1,
                       4, 2, 0);
  circuit_.SaveDefFileComponent(output_def_name + "_comp.def",
                                input_def_file_full_name);
  circuit_.InitNetFanoutHistogram();
  circuit_.ReportNetFanoutHistogram();
  circuit_.ReportHPWLHistogramLinear();
  circuit_.ReportHPWLHistogramLogarithm();
}

void Dali::InstantiateIoPlacer() {
  InitializeCircuitFromPhyDBIfNeeded();
  if (!io_placer_) {
    io_placer_ = std::make_unique<IoPlacer>(phy_db_ptr_, &circuit_);
  }
}

void Dali::InitializeCircuitFromPhyDBIfNeeded() {
  if (is_circuit_initialized_) {
    return;
  }
  circuit_.SetEnableShrinkOffGridDieArea(enable_shrink_off_grid_die_area_);
  circuit_.InitializeFromPhyDB(phy_db_ptr_);
  is_circuit_initialized_ = true;
}

/**
 * Create a script for detailed placement and legalization.
 *
 * @param engine, the full path or name of an external placer. If name is given,
 *        this placer is required to be in the search path.
 * @param script_name, the name of this detailed placement and legalization
 * script.
 * @return the DEF file name generated by the external placer.
 */
std::string Dali::CreateDetailedPlacementAndLegalizationScript(
    std::string const& engine, std::string const& script_name) {
  // create script for innovus
  DaliExpects(engine == "innovus", "Only support Innovus now");
  std::ofstream ost(script_name);
  ost << "loadLefFile " << phy_db_ptr_->GetTechPtr()->GetLefName() << "\n";
  std::string input_def_file = phy_db_ptr_->GetDesignPtr()->GetDefName();
  std::string tmp_def_out =
      "dali_global_" + phy_db_ptr_->GetDesignPtr()->GetName();
  ExportToDEF(input_def_file, tmp_def_out);
  ost << "loadDefFile " << tmp_def_out << ".def\n";
  ost << "refinePlace\n";
  std::string out_def = "dali_global_innovus_refine_" +
                        phy_db_ptr_->GetDesignPtr()->GetName() + ".def";
  ost << "defOut " << out_def << "\n";

  // check if the engine can be found
  DaliExpects(IsExecutableExisting(engine), "Cannot find the given engine");
  return out_def;
}

void Dali::ExportOrdinaryComponentsToPhyDB() {
  double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
  double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();
  for (auto& component : circuit_.Components()) {
    if (component.MacroPtr() == circuit_.tech().IoDummyMacroPtr()) {
      // skip dummy cells for I/O pins
      continue;
    }
    std::string comp_name = component.Name();
    int lx =
        (int)(component.LLX() * factor_x) + circuit_.design().DieAreaOffsetX();
    int ly =
        (int)(component.LLY() * factor_y) + circuit_.design().DieAreaOffsetY();
    auto place_status = phydb::PlaceStatus(component.Status());
    auto orient = phydb::CompOrient(component.Orient());

    phydb::Component* comp_ptr = phy_db_ptr_->GetComponentPtr(comp_name);
    DaliExpects(comp_ptr != nullptr,
                "No component in PhyDB with name: " << comp_name);
    comp_ptr->SetLocation(lx, ly);
    comp_ptr->SetPlacementStatus(place_status);
    comp_ptr->SetOrientation(orient);
  }
}

void Dali::ExportWellTapCellsToPhyDB() {
  double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
  double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();
  for (auto& component : circuit_.design().WellTaps()) {
    std::string comp_name = component.Name();
    std::string macro_name = component.MacroPtr()->Name();
    int lx =
        (int)(component.LLX() * factor_x) + circuit_.design().DieAreaOffsetX();
    int ly =
        (int)(component.LLY() * factor_y) + circuit_.design().DieAreaOffsetY();
    auto place_status = phydb::PlaceStatus(component.Status());
    auto orient = phydb::CompOrient(component.Orient());

    auto* phydb_macro_ptr = phy_db_ptr_->GetMacroPtr(macro_name);
    DaliExpects(phydb_macro_ptr != nullptr,
                "Cannot find " << macro_name << " in PhyDB?!");
    phy_db_ptr_->AddComponent(comp_name, phydb_macro_ptr, place_status, lx, ly,
                              orient, phydb::CompSource::USER);
  }
}

void Dali::ExportFillerCellsToPhyDB() {
  double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
  double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();
  for (auto& component : circuit_.design().Fillers()) {
    std::string comp_name = component.Name();
    std::string macro_name = component.MacroPtr()->Name();
    int lx =
        (int)(component.LLX() * factor_x) + circuit_.design().DieAreaOffsetX();
    int ly =
        (int)(component.LLY() * factor_y) + circuit_.design().DieAreaOffsetY();
    auto place_status = phydb::PlaceStatus(component.Status());
    auto orient = phydb::CompOrient(component.Orient());

    auto* phydb_macro_ptr = phy_db_ptr_->GetMacroPtr(macro_name);
    DaliExpects(phydb_macro_ptr != nullptr,
                "Cannot find " << macro_name << " in PhyDB?!");
    phy_db_ptr_->AddComponent(comp_name, phydb_macro_ptr, place_status, lx, ly,
                              orient, phydb::CompSource::DIST);
  }
}

void Dali::ExportComponentsToPhyDB() {
  ExportOrdinaryComponentsToPhyDB();
  ExportWellTapCellsToPhyDB();
  ExportFillerCellsToPhyDB();
}

void Dali::ExportIoPinsToPhyDB() {
  DaliExpects(!circuit_.Metals().empty(),
              "Need metal layer info to generate PIN location\n");
  for (auto& iopin : circuit_.design().IoPins()) {
    if (!iopin.IsPrePlaced() && iopin.IsPlaced()) {
      DaliExpects(iopin.LayerPtr() != nullptr,
                  "IOPIN metal layer not set? Cannot export it to PhyDB");
      std::string metal_name = iopin.LayerPtr()->Name();
      std::string iopin_name = iopin.Name();
      DaliExpects(phy_db_ptr_->IsIoPinExisting(iopin_name),
                  "IOPIN not in PhyDB? " << iopin_name);
      phydb::IOPin* phydb_iopin = phy_db_ptr_->GetIoPinPtr(iopin_name);
      auto& rect = iopin.Shape();
      int llx = circuit_.Micron2DatabaseUnit(rect.LLX());
      int lly = circuit_.Micron2DatabaseUnit(rect.LLY());
      int urx = circuit_.Micron2DatabaseUnit(rect.URX());
      int ury = circuit_.Micron2DatabaseUnit(rect.URY());
      phydb_iopin->SetShape(metal_name, llx, lly, urx, ury);

      int pin_x = iopin.FinalX();
      int pin_y = iopin.FinalY();
      phydb::CompOrient pin_orient;
      if (iopin.X() == circuit_.design().RegionLeft()) {
        pin_orient = phydb::CompOrient::E;
      } else if (iopin.X() == circuit_.design().RegionRight()) {
        pin_orient = phydb::CompOrient::W;
      } else if (iopin.Y() == circuit_.design().RegionBottom()) {
        pin_orient = phydb::CompOrient::N;
      } else {
        pin_orient = phydb::CompOrient::S;
      }

      phydb_iopin->SetPlacement(PlaceStatusDali2PhyDB(iopin.Status()), pin_x,
                                pin_y, pin_orient);
    }
  }
}

void Dali::ExportMiniRowsToPhyDB() {
  double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
  double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();

  int counter = 0;
  for (auto& col : well_legalizer_.col_list_) {
    for (auto& strip : col.stripe_list_) {
      std::string column_name = "column" + std::to_string(counter++);
      std::string bot_signal_;
      if (strip.is_first_row_orient_N_) {
        bot_signal_ = "GND";
      } else {
        bot_signal_ = "Vdd";
      }
      phydb::ClusterCol* p_col =
          phy_db_ptr_->AddClusterCol(column_name, bot_signal_);

      int col_lx =
          (int)(strip.LLX() * factor_x) + circuit_.design().DieAreaOffsetX();
      int col_ux =
          (int)(strip.URX() * factor_x) + circuit_.design().DieAreaOffsetX();
      p_col->SetXRange(col_lx, col_ux);

      if (strip.is_bottom_up_) {
        for (auto& cluster : strip.gridded_rows_) {
          int row_ly = (int)(cluster.LLY() * factor_y) +
                       circuit_.design().DieAreaOffsetY();
          int row_uy = (int)(cluster.URY() * factor_y) +
                       circuit_.design().DieAreaOffsetY();
          p_col->AddRow(row_ly, row_uy);
        }
      } else {
        int sz = static_cast<int>(strip.gridded_rows_.size());
        for (int j = sz - 1; j >= 0; --j) {
          auto& cluster = strip.gridded_rows_[j];
          int row_ly = (int)(cluster.LLY() * factor_y) +
                       circuit_.design().DieAreaOffsetY();
          int row_uy = (int)(cluster.URY() * factor_y) +
                       circuit_.design().DieAreaOffsetY();
          p_col->AddRow(row_ly, row_uy);
        }
      }
    }
  }
}

void Dali::ExportPpNpToPhyDB() {
  well_legalizer_.ExportPpNpToPhyDB(phy_db_ptr_);
}

void Dali::ExportWellToPhyDB() {
  well_legalizer_.ExportWellToPhyDB(phy_db_ptr_, 1);
}

}  // namespace dali
