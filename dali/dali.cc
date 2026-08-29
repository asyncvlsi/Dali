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

/**
 * @file
 * Top-level placement flow orchestration.
 *
 * `StartPlacement` resolves configuration, then runs the core stages -- global
 * placement followed by legalization -- and the post-placement completion
 * stages, filler cells and I/O pins. Which legalizer runs is the fork between
 * the two supported flows: `-is_standard_cell` takes the standard-cell
 * legalizer, otherwise a `-cell` file selects gridded well legalization, whose
 * physical completion also inserts well taps and end caps.
 *
 * Configuration reaches this class as named entries loaded from the ACT
 * configuration system rather than as constructor arguments, so each option
 * appears three times: a member with its default, a Load*Config call, and a
 * line in the reporting block. Adding an option means touching all three.
 *
 * Stages publish snapshots through PlacementSnapshotSink.
 * `ExpectedSnapshotStages` declares up front which stages this configuration
 * will actually execute, so the GUI can reserve exactly those chart slots
 * instead of discovering them.
 */
#include "dali.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <limits>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>

#include "dali/circuit/hpwl_lower_bound.h"
#include "dali/common/act_config.h"
#include "dali/common/elapsed_time.h"
#include "dali/common/git_version.h"
#include "dali/common/helper.h"
#include "dali/common/logging.h"
#include "dali/common/phydb_helper.h"
#include "dali/common/placement_metrics.h"
#include "dali/placer/well_legalizer/delay_line_reservation.h"
#include "dali/placer/well_legalizer/rough_gridded_upper_bound_refiner.h"
#include "dali/timing/delay_line_detour.h"
#include "dali/timing/timing_path_visualization.h"

namespace dali {

namespace {

/**
 * Applies Dali's checkpoint schedule and delegates only the netlist change.
 *
 * The split is the point: deciding *when* to interrupt placement is a placement
 * decision and stays here, while deciding *what* the netlist becomes belongs to
 * whoever owns the authoritative netlist. A host that could do both would own
 * the loop.
 *
 * With no host, or with max_checkpoints at its default of zero, this records
 * offers and never asks for a change, which is the behaviour every run has had
 * so far.
 */
/**
 * Carries the placer's checkpoint offers to Dali's decision, and nothing else.
 *
 * Deliberately thin. Eligibility is the coordinator's, the decision is Dali's,
 * and this only knows how to ask them in the right order and count what
 * happened. It is named for what it is because the previous shape, a
 * "HostCheckpointObserver" holding a schedule and calling a host, read as
 * though the observer were making the choice.
 */
class DaliCheckpointCoordination : public PlacementCheckpointObserver {
 public:
  DaliCheckpointCoordination(Dali *dali, CheckpointEligibilityConfig config,
                             int max_checkpoints)
      : dali_(dali), coordinator_(config), max_checkpoints_(max_checkpoints) {}

  CheckpointDecision Observe(const PlacementCheckpoint &checkpoint) override {
    ++offers_;
    if (dali_ == nullptr || max_checkpoints_ <= 0) {
      return CheckpointDecision::kContinue;
    }
    CheckpointSample sample;
    sample.iteration = checkpoint.iteration;
    sample.accepted_hpwl = checkpoint.accepted_hpwl;
    sample.hpwl_change_fraction = checkpoint.hpwl_change_fraction;
    if (!coordinator_.Offer(sample)) return CheckpointDecision::kContinue;

    LOG(info) << "  CHECKPOINT_ELIGIBLE at iteration " << checkpoint.iteration
              << " after three settled accepted physical iterations:\n";
    for (const CheckpointSample &qualifying : coordinator_.QualifyingSamples()) {
      LOG(info) << "    iteration " << qualifying.iteration << " accepted_hpwl "
                << qualifying.accepted_hpwl << " change "
                << qualifying.hpwl_change_fraction << "\n";
    }
    return CheckpointDecision::kChangeTopology;
  }

  TopologyMutationResult RequestTopologyChange(
      const PlacementCheckpoint &checkpoint,
      const TopologyCheckpointContext &context) override {
    (void)checkpoint;
    ++requests_;
    // One attempt per run, whatever it decides. Marked before the decision, so
    // a decline cannot be retried at the next settled iteration.
    coordinator_.MarkAttempted();
    if (dali_ == nullptr) return TopologyMutationResult::NoChange();
    return dali_->HandleTopologyCheckpoint(context);
  }

  size_t Count() const { return offers_; }
  size_t Requests() const { return requests_; }

 private:
  Dali *dali_ = nullptr;
  TopologyCheckpointCoordinator coordinator_;
  int max_checkpoints_ = 0;
  size_t offers_ = 0;
  size_t requests_ = 0;
};

}  // namespace

static std::string ConfigName(const std::string &prefix, const char *name) {
  return prefix + name;
}

static bool ShouldVisualizeWellRects(const std::string &group,
                                     const std::string &subgroup) {
  return group == "legalization" &&
         (subgroup == "well_tap" || subgroup == "end_cap");
}

static bool ConfigExists(const std::string &name) {
  return config_exists(name.c_str());
}

static void LoadBoolConfig(const std::string &name, bool *value) {
  if (ConfigExists(name)) {
    *value = config_get_int(name.c_str()) == 1;
  }
}

static void LoadIntConfig(const std::string &name, int *value) {
  if (ConfigExists(name)) {
    *value = config_get_int(name.c_str());
  }
}

static void LoadRealConfig(const std::string &name, double *value) {
  if (ConfigExists(name)) {
    *value = config_get_real(name.c_str());
  }
}

static void LoadStringConfig(const std::string &name, std::string *value) {
  if (ConfigExists(name)) {
    *value = config_get_string(name.c_str());
  }
}

static bool ParseCommandBool(const std::string &value, bool *result) {
  if (value == "true" || value == "on" || value == "1") {
    *result = true;
    return true;
  }
  if (value == "false" || value == "off" || value == "0") {
    *result = false;
    return true;
  }
  return false;
}

static bool ParseCommandInt(const std::string &value, int *result) {
  try {
    std::size_t parsed_length = 0;
    *result = std::stoi(value, &parsed_length);
    return parsed_length == value.size();
  } catch (...) {
    return false;
  }
}

static bool ParseCommandDouble(const std::string &value, double *result) {
  try {
    std::size_t parsed_length = 0;
    *result = std::stod(value, &parsed_length);
    return parsed_length == value.size() && std::isfinite(*result);
  } catch (...) {
    return false;
  }
}

static PlacementInitializerType
ParseGlobalInitializer(const std::string &name) {
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

static GlobalRefinementFeedbackMode
ParseGlobalRefinementFeedbackMode(const std::string &name) {
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

static GlobalLalExpansionMode
ParseGlobalLalExpansionMode(const std::string &name) {
  if (name == "symmetric") {
    return GlobalLalExpansionMode::kSymmetric;
  }
  if (name == "best_neighbor") {
    return GlobalLalExpansionMode::kBestNeighbor;
  }
  std::cout << "Ignore unknown global_lal_expansion: " << name << "\n";
  return GlobalLalExpansionMode::kSymmetric;
}

static GlobalLalHotspotMode ParseGlobalLalHotspotMode(const std::string &name) {
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

static GlobalLalMacroBoundaryMode
ParseGlobalLalMacroBoundaryMode(const std::string &name) {
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

static StandardCellLegalizerCostMode
ParseStandardCellLegalizerCostMode(const std::string &name) {
  if (name == "displacement") {
    return StandardCellLegalizerCostMode::kDisplacement;
  }
  if (name == "hpwl") {
    return StandardCellLegalizerCostMode::kHpwl;
  }
  std::cout << "Ignore unknown standard_cell_legalizer_cost: " << name << "\n";
  return StandardCellLegalizerCostMode::kDisplacement;
}

Dali::Dali(phydb::PhyDB *phy_db_ptr, const std::string &severity_level,
           const std::string &log_file_name)
    : Dali(phy_db_ptr, StrToLoggingLevel(severity_level), log_file_name) {}

Dali::Dali(phydb::PhyDB *phy_db_ptr, severity severity_level,
           const std::string &log_file_name) {
  phy_db_ptr_ = phy_db_ptr;
  severity_level_ = severity_level;
  log_file_name_ = log_file_name;
  LoadParamsFromConfig();
  InitLogging(log_file_name_, severity_level_, disable_log_prefix_);
  if (phy_db_ptr_ != nullptr) {
    input_lef_file_name_ = phy_db_ptr_->GetTechPtr()->GetLefName();
    input_def_file_name_ = phy_db_ptr_->GetDesignPtr()->GetDefName();
  }
}

void Dali::SetGuiSnapshotSinkFactory(SnapshotSinkFactory factory) {
  gui_snapshot_sink_factory_ = std::move(factory);
}

void Dali::SetInteractiveSessionExpected(bool expected) {
  interactive_session_expected_ = expected;
}

static bool IsReadableInputFile(const std::string &file_name) {
  std::error_code error;
  return std::filesystem::is_regular_file(file_name, error);
}

static std::string NormalizeInputFileName(const std::string &file_name) {
  std::error_code error;
  const std::filesystem::path canonical_name =
      std::filesystem::canonical(file_name, error);
  return error ? std::filesystem::path(file_name).lexically_normal().string()
               : canonical_name.string();
}

/**
 * Return the output base accepted by Dali's exporters.
 *
 * Public interfaces accept either `placed` or `placed.def`. The circuit
 * exporters append their own suffixes, so remove one optional DEF extension
 * here to keep the primary and companion output names consistent.
 */
static std::string NormalizeDefOutputBaseName(const std::string &output_name) {
  std::filesystem::path output_path(output_name);
  std::string extension = output_path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char character) {
                   return static_cast<char>(std::tolower(character));
                 });
  if (extension == ".def") {
    output_path.replace_extension();
  }
  return output_path.string();
}

bool Dali::ReadLef(const std::string &file_name) {
  if (phy_db_ptr_ == nullptr) {
    LOG(error) << "Cannot read LEF without a PhyDB instance\n";
    return false;
  }
  if (is_circuit_initialized_) {
    LOG(error) << "Cannot read LEF after the Dali circuit is initialized\n";
    return false;
  }
  if (!IsReadableInputFile(file_name)) {
    LOG(error) << "Cannot read LEF file: " << file_name << "\n";
    return false;
  }
  const std::string normalized_name = NormalizeInputFileName(file_name);
  if (!input_lef_file_name_.empty()) {
    if (NormalizeInputFileName(input_lef_file_name_) == normalized_name) {
      return true;
    }
    LOG(error) << "A LEF input is already loaded: " << input_lef_file_name_
               << "\n";
    return false;
  }
  phy_db_ptr_->ReadLef(normalized_name);
  input_lef_file_name_ = normalized_name;
  return true;
}

bool Dali::ReadDef(const std::string &file_name) {
  if (phy_db_ptr_ == nullptr) {
    LOG(error) << "Cannot read DEF without a PhyDB instance\n";
    return false;
  }
  if (is_circuit_initialized_) {
    LOG(error) << "Cannot read DEF after the Dali circuit is initialized\n";
    return false;
  }
  if (input_lef_file_name_.empty()) {
    LOG(error) << "Read LEF before DEF\n";
    return false;
  }
  if (!IsReadableInputFile(file_name)) {
    LOG(error) << "Cannot read DEF file: " << file_name << "\n";
    return false;
  }
  const std::string normalized_name = NormalizeInputFileName(file_name);
  if (!input_def_file_name_.empty()) {
    if (NormalizeInputFileName(input_def_file_name_) == normalized_name) {
      return true;
    }
    LOG(error) << "A DEF input is already loaded: " << input_def_file_name_
               << "\n";
    return false;
  }
  phy_db_ptr_->ReadDef(normalized_name);
  input_def_file_name_ = normalized_name;
  return true;
}

bool Dali::ReadCell(const std::string &file_name) {
  if (phy_db_ptr_ == nullptr) {
    LOG(error) << "Cannot read CELL without a PhyDB instance\n";
    return false;
  }
  if (is_circuit_initialized_) {
    LOG(error) << "Cannot read CELL after the Dali circuit is initialized\n";
    return false;
  }
  if (input_lef_file_name_.empty() || input_def_file_name_.empty()) {
    LOG(error) << "Read LEF and DEF before CELL\n";
    return false;
  }
  if (!IsReadableInputFile(file_name)) {
    LOG(error) << "Cannot read CELL file: " << file_name << "\n";
    return false;
  }
  const std::string normalized_name = NormalizeInputFileName(file_name);
  if (!input_cell_file_name_.empty()) {
    if (NormalizeInputFileName(input_cell_file_name_) == normalized_name) {
      return true;
    }
    LOG(error) << "A CELL input is already loaded: " << input_cell_file_name_
               << "\n";
    return false;
  }
  phy_db_ptr_->ReadCell(normalized_name);
  input_cell_file_name_ = normalized_name;
  return true;
}

bool Dali::SetPlacementGrids(double grid_x, double grid_y) {
  if (phy_db_ptr_ == nullptr) {
    LOG(error) << "Cannot set placement grids without a PhyDB instance\n";
    return false;
  }
  if (is_circuit_initialized_ || grid_x <= 0 || grid_y <= 0) {
    LOG(error) << "Placement grids must be positive and set before circuit "
                  "initialization\n";
    return false;
  }
  phy_db_ptr_->SetPlacementGrids(grid_x, grid_y);
  return true;
}

bool Dali::HasInputDesign() const {
  return !input_lef_file_name_.empty() && !input_def_file_name_.empty();
}

/**
 * Log the resolved value of every option, for reproducing a run.
 *
 * One line per option, mirroring LoadParamsFromConfig, so a log records exactly
 * what configuration produced it.
 */
void Dali::ShowParamsList() {
  LOG(info)
      << "Dali runtime parameters:\n"
      << "  log_file_name: " << log_file_name_ << "\n"
      << "  disable_log_prefix: " << disable_log_prefix_ << "\n"
      << "  num_threads: " << num_threads_ << "\n"
      << "  well_legalization_mode: "
      << static_cast<int>(well_legalization_mode_) << "\n"
      << "  well_emit_mode: " << well_emit_mode_ << "\n"
      << "  disable_global_place: " << disable_global_place_ << "\n"
      << "  disable_legalization: " << disable_legalization_ << "\n"
      << "  disable_detailed_place: " << disable_detailed_place_ << "\n"
      << "  disable_io_place: " << disable_io_place_ << "\n"
      << "  target_density: " << target_density_ << "\n"
      << "  timing_period_target: " << timing_period_target_ << "\n"
      << "  timing_use_rc: " << timing_use_rc_ << "\n"
      << "  rc_min_routing_layer: " << rc_min_routing_layer_ << "\n"
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

/**
 * Load every option from the ACT configuration into its member.
 *
 * The counterpart to the command-line parser: the parser writes named config
 * entries, this reads them back. Long because it is one load call per option;
 * an option missing here is silently left at its default however it was set.
 */
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

  LoadIntConfig(ConfigName(prefix_, "well_emit_mode"), &well_emit_mode_);
  if (well_emit_mode_ < 0 || well_emit_mode_ > 2) {
    std::cout << "Ignore unknown well_emit_mode: " << well_emit_mode_ << "\n";
    well_emit_mode_ = 1;
  }

  LoadBoolConfig(ConfigName(prefix_, "disable_global_place"),
                 &disable_global_place_);
  LoadBoolConfig(ConfigName(prefix_, "disable_legalization"),
                 &disable_legalization_);
  LoadBoolConfig(ConfigName(prefix_, "disable_detailed_place"),
                 &disable_detailed_place_);
  LoadBoolConfig(ConfigName(prefix_, "disable_io_place"), &disable_io_place_);
  LoadRealConfig(ConfigName(prefix_, "target_density"), &target_density_);
  LoadBoolConfig(ConfigName(prefix_, "timing_use_rc"), &timing_use_rc_);
  LoadIntConfig(ConfigName(prefix_, "rc_min_routing_layer"),
                &rc_min_routing_layer_);
  LoadIntConfig(ConfigName(prefix_, "net_ignore_threshold"),
                &net_ignore_threshold_);
  DaliExpects(net_ignore_threshold_ >= 100 && net_ignore_threshold_ <= 1000,
              "net_ignore_threshold must be in [100, 1000]");
  LoadIntConfig(ConfigName(prefix_, "io_metal_layer"), &io_metal_layer_);
  LoadBoolConfig(ConfigName(prefix_, "disable_welltap"), &disable_welltap_);
  param_name = ConfigName(prefix_, "well_tap_pattern");
  if (ConfigExists(param_name)) {
    well_tap_pattern_ =
        ParseWellTapPattern(config_get_string(param_name.c_str()));
  }
  DaliExpects(disable_welltap_ || IsWellTapPatternSupported(well_tap_pattern_),
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
  DaliExpects(global_lal_affine_weight_ >= 0.0 &&
                  global_lal_affine_weight_ <= 1.0,
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
  output_name_ = NormalizeDefOutputBaseName(output_name_);
  DaliExpects(!output_name_.empty(), "output_name must not be empty");
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

/**
 * Apply one setting from a `.dali` command.
 *
 * The command language intentionally starts with the stable, commonly useful
 * flow options instead of accepting arbitrary ACT config keys. Explicit
 * validation catches misspellings and keeps script behavior aligned with the
 * corresponding standalone command-line options.
 */
bool Dali::SetRuntimeOption(const std::string &name, const std::string &value) {
  bool bool_value = false;
  int int_value = 0;
  double double_value = 0;

  if (name == "target_density") {
    if (!ParseCommandDouble(value, &double_value) || double_value <= 0 ||
        double_value > 1) {
      LOG(error) << "target_density must be in the range (0, 1]\n";
      return false;
    }
    target_density_ = double_value;
  } else if (name == "timing_period_target") {
    if (!ParseCommandDouble(value, &double_value) || double_value <= 0) {
      LOG(error) << "timing_period_target must be positive\n";
      return false;
    }
    timing_period_target_ = double_value;
  } else if (name == "rc_min_routing_layer") {
    if (!ParseCommandInt(value, &int_value) || int_value < 0) {
      LOG(error) << "rc_min_routing_layer must be a non-negative index\n";
      return false;
    }
    rc_min_routing_layer_ = int_value;
    // The estimator captures this at construction, and anything that measured
    // timing earlier -- a constraint-identity capture before the recipe runs,
    // for instance -- has already built one. Without this the setting is
    // silently ignored for the rest of the run and every net is charged to the
    // default layer, which on sky130 is `li` at roughly fifty times met1's
    // sheet resistance.
    if (rc_estimator != nullptr) {
      rc_estimator->SetMinRoutingLayer(rc_min_routing_layer_);
    }
    // The checkpoint schedule. Which iterations may be interrupted is a
    // placement decision and belongs in the recipe beside the other placement
    // settings, not in the environment: a run has to be reproducible from the
    // file that describes it.
  } else if (name == "topology_checkpoint_warmup") {
    if (!ParseCommandInt(value, &int_value) || int_value < 0) {
      LOG(error) << "topology_checkpoint_warmup must be a non-negative "
                    "iteration index\n";
      return false;
    }
    topology_checkpoint_warmup_ = int_value;
  } else if (name == "topology_checkpoint_interval") {
    // Zero would let two checkpoints land on the same iteration, which cannot
    // happen: each one advances the iteration counter past itself.
    if (!ParseCommandInt(value, &int_value) || int_value < 1) {
      LOG(error) << "topology_checkpoint_interval must be at least 1\n";
      return false;
    }
    topology_checkpoint_interval_ = int_value;
  } else if (name == "topology_checkpoint_stability_fraction") {
    if (!ParseCommandDouble(value, &double_value) || !(double_value > 0.0) ||
        !std::isfinite(double_value)) {
      LOG(error) << "topology_checkpoint_stability_fraction must be a finite "
                    "positive fraction\n";
      return false;
    }
    checkpoint_eligibility_.stability_fraction = double_value;
  } else if (name == "topology_checkpoint_stability_window") {
    if (!ParseCommandInt(value, &int_value) || int_value < 1) {
      LOG(error) << "topology_checkpoint_stability_window must be at least 1\n";
      return false;
    }
    checkpoint_eligibility_.stability_window = int_value;
  } else if (name == "topology_sizing_margin_ps") {
    if (!ParseCommandDouble(value, &double_value) ||
        !std::isfinite(double_value)) {
      LOG(error) << "topology_sizing_margin_ps must be a finite number\n";
      return false;
    }
    topology_sizing_margin_ps_ = double_value;
  } else if (name == "topology_sizing_max_added_pairs") {
    if (!ParseCommandInt(value, &int_value) || int_value < 1) {
      LOG(error) << "topology_sizing_max_added_pairs must be at least 1\n";
      return false;
    }
    topology_sizing_max_added_pairs_ = int_value;
  } else if (name == "topology_sizing_max_pairs") {
    if (!ParseCommandInt(value, &int_value) || int_value < 1) {
      LOG(error) << "topology_sizing_max_pairs must be at least 1\n";
      return false;
    }
    topology_sizing_max_pairs_ = int_value;
  } else if (name == "topology_stage_boundary_sizing") {
    bool bool_value = false;
    if (!ParseCommandBool(value, &bool_value)) {
      LOG(error) << "topology_stage_boundary_sizing must be true or false\n";
      return false;
    }
    topology_stage_boundary_sizing_ = bool_value;
  } else if (name == "topology_adaptive_sizing") {
    if (!ParseCommandBool(value, &bool_value)) {
      LOG(error) << "topology_adaptive_sizing must be true or false\n";
      return false;
    }
    topology_adaptive_sizing_ = bool_value;
  } else if (name == "topology_adaptive_probe_pairs") {
    if (!ParseCommandInt(value, &int_value) || int_value < 1) {
      LOG(error) << "topology_adaptive_probe_pairs must be at least 1\n";
      return false;
    }
    topology_adaptive_probe_pairs_ = int_value;
  } else if (name == "topology_adaptive_max_step_pairs") {
    if (!ParseCommandInt(value, &int_value) || int_value < 1) {
      LOG(error) << "topology_adaptive_max_step_pairs must be at least 1\n";
      return false;
    }
    topology_adaptive_max_step_pairs_ = int_value;
  } else if (name == "topology_adaptive_max_epochs") {
    if (!ParseCommandInt(value, &int_value) || int_value < 1) {
      LOG(error) << "topology_adaptive_max_epochs must be at least 1\n";
      return false;
    }
    topology_adaptive_max_epochs_ = int_value;
  } else if (name == "topology_adaptive_max_nonpositive_probes") {
    if (!ParseCommandInt(value, &int_value) || int_value < 0) {
      LOG(error) << "topology_adaptive_max_nonpositive_probes must be "
                    "non-negative\n";
      return false;
    }
    topology_adaptive_max_nonpositive_probes_ = int_value;
  } else if (name == "timing_observe_global_iterations") {
    if (!ParseCommandBool(value, &bool_value)) {
      LOG(error) << "timing_observe_global_iterations must be true or false\n";
      return false;
    }
    timing_observe_global_iterations_ = bool_value;
  } else if (name == "sizing_manifest") {
    sizing_manifest_path_ = value;
  } else if (name == "topology_batch_sizing") {
    bool bool_value = false;
    if (!ParseCommandBool(value, &bool_value)) {
      LOG(error) << "topology_batch_sizing must be true or false\n";
      return false;
    }
    topology_batch_sizing_ = bool_value;
  } else if (name == "topology_batch_require_characterized") {
    bool bool_value = false;
    if (!ParseCommandBool(value, &bool_value)) {
      LOG(error) << "topology_batch_require_characterized must be true or "
                    "false\n";
      return false;
    }
    topology_batch_require_characterized_ = bool_value;
  } else if (name == "topology_checkpoint_max") {
    if (!ParseCommandInt(value, &int_value) || int_value < 0) {
      LOG(error) << "topology_checkpoint_max must be non-negative; 0 disables "
                    "checkpoints\n";
      return false;
    }
    topology_checkpoint_max_ = int_value;
  } else if (name == "timing_use_rc") {
    if (!ParseCommandBool(value, &bool_value)) {
      LOG(error) << "timing_use_rc must be true or false\n";
      return false;
    }
    timing_use_rc_ = bool_value;
  } else if (name == "delay_line_fold_count") {
    if (!ParseCommandInt(value, &int_value) || int_value < 1) {
      LOG(error) << "delay_line_fold_count must be positive\n";
      return false;
    }
    delay_line_fold_count_ = int_value;
  } else if (name == "num_threads") {
    if (!ParseCommandInt(value, &int_value) || int_value < 1) {
      LOG(error) << "num_threads must be positive\n";
      return false;
    }
    SetNumThreads(int_value);
  } else if (name == "io_metal_layer") {
    if (!ParseCommandInt(value, &int_value) || int_value < 1) {
      LOG(error) << "io_metal_layer must be a positive, one-based layer "
                    "number\n";
      return false;
    }
    io_metal_layer_ = int_value - 1;
  } else if (name == "net_ignore_threshold") {
    if (!ParseCommandInt(value, &int_value) || int_value < 100 ||
        int_value > 1000) {
      LOG(error) << "net_ignore_threshold must be in [100, 1000]\n";
      return false;
    }
    net_ignore_threshold_ = int_value;
  } else if (name == "global_min_iterations") {
    if (!ParseCommandInt(value, &int_value) || int_value < 0) {
      LOG(error) << "global_min_iterations must be non-negative\n";
      return false;
    }
    global_min_iterations_ = int_value;
  } else if (name == "global_max_iterations") {
    if (!ParseCommandInt(value, &int_value) || int_value < 0) {
      LOG(error) << "global_max_iterations must be non-negative\n";
      return false;
    }
    global_max_iterations_ = int_value;
  } else if (name == "detailed_max_rounds") {
    if (!ParseCommandInt(value, &int_value) || int_value < 0) {
      LOG(error) << "detailed_max_rounds must be non-negative\n";
      return false;
    }
    detailed_max_rounds_ = int_value;
  } else if (name == "detailed_max_move_candidates") {
    if (!ParseCommandInt(value, &int_value) || int_value < 0) {
      LOG(error) << "detailed_max_move_candidates must be non-negative\n";
      return false;
    }
    detailed_max_move_candidates_ = int_value;
  } else if (name == "global_initializer") {
    if (value != "keep" && value != "uniform" && value != "gaussian" &&
        value != "monte_carlo" && value != "density_aware") {
      LOG(error) << "Unknown global_initializer: " << value << "\n";
      return false;
    }
    global_initializer_ = ParseGlobalInitializer(value);
  } else if (name == "well_legalization_mode") {
    if (value == "strict") {
      well_legalization_mode_ = WellPartitionMode::kStrict;
    } else if (value == "scavenge") {
      well_legalization_mode_ = WellPartitionMode::kScavenge;
    } else {
      LOG(error) << "well_legalization_mode must be strict or scavenge\n";
      return false;
    }
  } else if (name == "well_emit_mode") {
    if (!ParseCommandInt(value, &int_value) || int_value < 0 ||
        int_value > 2) {
      LOG(error) << "well_emit_mode must be 0, 1, or 2\n";
      return false;
    }
    well_emit_mode_ = int_value;
  } else if (name == "standard_cell_legalizer_cost") {
    if (value != "displacement" && value != "hpwl") {
      LOG(error) << "standard_cell_legalizer_cost must be displacement or "
                    "hpwl\n";
      return false;
    }
    standard_cell_legalizer_cost_mode_ =
        ParseStandardCellLegalizerCostMode(value);
  } else if (name == "output_name") {
    const std::string output_base = NormalizeDefOutputBaseName(value);
    if (output_base.empty()) {
      LOG(error) << "output_name must not be empty\n";
      return false;
    }
    output_name_ = output_base;
  } else if (name == "gui_debug") {
    if (!ParseCommandBool(value, &bool_value)) {
      LOG(error) << "gui_debug must be true or false\n";
      return false;
    }
    gui_debug_ = bool_value;
  } else if (name == "gui_pause") {
    if (value != "every_snapshot" && value != "off") {
      LOG(error) << "gui_pause must be every_snapshot or off\n";
      return false;
    }
    gui_pause_ = value;
  } else if (name == "disable_global_place" || name == "disable_legalization" ||
             name == "disable_detailed_place" || name == "disable_io_place" ||
             name == "disable_welltap" || name == "disable_cell_flip" ||
             name == "is_standard_cell" || name == "enable_filler_cell" ||
             name == "enable_end_cap_cell" || name == "stage_band_uniform" ||
             name == "delay_line_separation_escalation" ||
             name == "enable_gridded_upper_bound_refiner" ||
             name == "enable_gridded_upper_bound_balancing") {
    if (!ParseCommandBool(value, &bool_value)) {
      LOG(error) << name << " must be true or false\n";
      return false;
    }
    if (name == "enable_gridded_upper_bound_refiner") {
      // The refiner is what produces an accepted physical upper bound, which is
      // the only state at which topology may be changed. A flow driven by a
      // recipe rather than the command line could not reach it before.
      enable_gridded_upper_bound_refiner_ = bool_value;
    } else if (name == "enable_gridded_upper_bound_balancing") {
      enable_gridded_upper_bound_balancing_ = bool_value;
    } else if (name == "delay_line_separation_escalation") {
      delay_line_separation_escalation_ = bool_value;
    } else if (name == "stage_band_uniform") {
      gb_placer_.SetStageBandSpacing(bool_value
                                         ? StageBandSpacing::kUniform
                                         : StageBandSpacing::kAreaProportional);
    } else if (name == "disable_global_place") {
      disable_global_place_ = bool_value;
    } else if (name == "disable_legalization") {
      disable_legalization_ = bool_value;
    } else if (name == "disable_detailed_place") {
      disable_detailed_place_ = bool_value;
    } else if (name == "disable_io_place") {
      disable_io_place_ = bool_value;
    } else if (name == "disable_welltap") {
      disable_welltap_ = bool_value;
    } else if (name == "disable_cell_flip") {
      disable_cell_flip_ = bool_value;
    } else if (name == "is_standard_cell") {
      is_standard_cell_ = bool_value;
    } else if (name == "enable_filler_cell") {
      enable_filler_cell_ = bool_value;
    } else {
      enable_end_cap_cell_ = bool_value;
    }
  } else {
    LOG(error) << "Unknown or unsupported Dali setting: " << name << "\n";
    return false;
  }

  LOG(info) << "Set " << name << " = " << value << "\n";
  return true;
}

bool Dali::ReadDelayRepairSites(const std::string &file_name) {
  std::vector<DelayRepairSite> sites;
  std::string error_message;
  if (!ReadDelayRepairSiteMetadata(file_name, &sites, &error_message)) {
    LOG(error) << "Cannot read delay-site metadata '" << file_name
               << "': " << error_message << "\n";
    return false;
  }
  delay_repair_sites_ = std::move(sites);
  LOG(info) << "Loaded " << delay_repair_sites_.size()
            << " declared timing-delay sites from " << file_name << "\n";
  return true;
}

std::vector<TimingRepairSitePlanItem> Dali::LastTimingRepairPlan() const {
  return BuildTimingRepairSitePlan(last_timing_snapshot_, delay_repair_sites_);
}

Circuit &Dali::GetCircuit() { return circuit_; }

phydb::PhyDB *Dali::GetPhyDBPtr() { return phy_db_ptr_; }

/**
 * Collect the options into the struct passed to the placement engines.
 *
 * The single place the many members become one argument, so a stage takes a
 * RuntimeOptions rather than a long parameter list.
 */
Dali::RuntimeOptions Dali::GetRuntimeOptions() const {
  return RuntimeOptions{
      log_file_name_,
      disable_log_prefix_,
      num_threads_,
      well_legalization_mode_,
      well_emit_mode_,
      disable_global_place_,
      disable_legalization_,
      disable_detailed_place_,
      disable_io_place_,
      target_density_,
      timing_period_target_,
      timing_use_rc_,
      rc_min_routing_layer_,
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

bool Dali::SetIoPlacerGlobalMetalLayer(std::string const &layer_name) {
  InitializeCircuitFromPhyDBIfNeeded();
  DaliExpects(io_placer_ != nullptr, "Please initialize I/O placer first");
  bool is_metal_name = circuit_.IsMetalLayerExisting(layer_name);
  if (is_metal_name) {
    MetalLayer *metal_layer = circuit_.GetMetalLayerPtr(layer_name);
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
      << "      manually place and fix one I/O pin (coordinates in microns)\n"
      << "  -cons/--constraint <pin|dir:DIR> <edge>\n"
      << "      constrain a pin or signal direction to one die edge\n"
      << "  -area/--area-array <metal> <rows> <cols>\n"
      << "      place every unplaced pin on an interior area-array grid\n"
      << "  -group/--group <metal> <edge> <pin> [pin ...]\n"
      << "      place and fix an adjacent pin group in command order\n"
      << "  -mirror/--mirror <pin> <reference_pin> <x|y>\n"
      << "      fix a pin by reflecting the reference x or y coordinate\n"
      << "  -move/--move <pin> <x> <y> [orientation]\n"
      << "      move a pin in microns while preserving its shape and layer\n"
      << "  -unfix/--unfix <pin>\n"
      << "      mark a pin unplaced so automatic placement may place it again\n"
      << "  -show/--show [pin]\n"
      << "      report one pin, or every pin when the name is omitted\n"
      << "  -check/--check\n"
      << "      run lightweight I/O placement legality checks\n"
      << "  -c/--config (use -h to see more usage)\n"
      << "      set parameters for automatic IOPIN placement\n"
      << "  -ap/--auto-place\n"
      << "      automatically place all unplaced IOPINs, which is also the "
         "default option\n"
      << "\033[0m\n";
}

bool Dali::IoPinPlacement(int argc, char **argv) {
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
  if (option_str == "-c" or option_str == "-config" or
      option_str == "--config") {
    return io_placer_->ConfigCmd(argc - 2, argv + 2);
  } else if (option_str == "-p" or option_str == "-place" or
             option_str == "--place") {
    return io_placer_->PartialPlaceCmd(argc - 2, argv + 2);
  } else if (option_str == "-cons" or option_str == "-constraint" or
             option_str == "--constraint") {
    return io_placer_->ConstraintCmd(argc - 2, argv + 2);
  } else if (option_str == "-area" or option_str == "-area-array" or
             option_str == "--area-array") {
    return io_placer_->AreaArrayPlaceCmd(argc - 2, argv + 2);
  } else if (option_str == "-group" or option_str == "--group") {
    return io_placer_->GroupPlaceCmd(argc - 2, argv + 2);
  } else if (option_str == "-mirror" or option_str == "--mirror") {
    return io_placer_->MirrorPlaceCmd(argc - 2, argv + 2);
  } else if (option_str == "-move" or option_str == "--move") {
    return io_placer_->MoveIoPinCmd(argc - 2, argv + 2);
  } else if (option_str == "-unfix" or option_str == "--unfix") {
    return io_placer_->UnfixIoPinCmd(argc - 2, argv + 2);
  } else if (option_str == "-show" or option_str == "--show") {
    return io_placer_->ShowIoPinsCmd(argc - 2, argv + 2);
  } else if (option_str == "-check" or option_str == "--check") {
    return io_placer_->CheckIoPlacementCmd(argc - 2, argv + 2);
  } else if (option_str == "-ap" or option_str == "-auto-place" or
             option_str == "--auto-place") {
    return io_placer_->AutoPlaceCmd(argc - 2, argv + 2);
  } else {
    LOG(warning) << "IoPlace flag not specified, use --auto-place by default\n";
    return io_placer_->AutoPlaceCmd(argc - 1, argv + 1);
  }
}

bool Dali::ShouldPerformTimingDrivenPlacement() {
  return phy_db_ptr_->GetTimingApi().ReadyForTimingDriven();
}

/**
 * Refuse to measure timing whose wire model is not the configured one.
 *
 * Amendment P: an estimator built before the recipe ran kept layer 0 while the
 * configured value said 1, and every accepted-physical slack in three
 * amendments was silently charged to `li` at roughly fifty times met1's sheet
 * resistance. Nothing in the output distinguished those numbers from correct
 * ones. This is deliberately a hard failure rather than a warning: a warning is
 * exactly what the previous eight months of runs would have printed, buried in
 * a thousand others, and it would have been read the same way -- not at all.
 */
bool Dali::VerifyRcConfigurationIsEffective(const std::string &context) {
  if (!timing_use_rc_ || rc_estimator == nullptr) {
    return true;
  }
  const int effective = rc_estimator->MinRoutingLayer();
  if (effective == rc_min_routing_layer_) {
    return true;
  }
  LOG(error) << "RC_CONFIGURATION_DRIFT context " << context << " configured "
             << rc_min_routing_layer_ << " effective " << effective
             << " -- the estimator in use is not the one that was configured, "
                "so every slack this capture would produce is charged to the "
                "wrong metal layer\n";
  return false;
}

void Dali::InitializeRCEstimator() {
  rc_estimator = std::make_unique<StarPiModelEstimator>(phy_db_ptr_);
  rc_estimator->SetMinRoutingLayer(rc_min_routing_layer_);
}

#if PHYDB_USE_GALOIS
void Dali::FetchSlacks() {
  phydb::ActPhyDBTimingAPI &timing_api = phy_db_ptr_->GetTimingApi();
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
      timing_api.GetSlowWitness(i, slow_path);
      std::cout << "Slow path size: " << slow_path.edges.size() << "\n";
    }
  }
}

void Dali::InitializeTimingDrivenPlacement() {
  if (timing_analysis_initialized_)
    return;
  phy_db_ptr_->CreatePhydbActAdaptor(false);
  phy_db_ptr_->AddNetsAndCompPinsToSpefManager();
  InitializeRCEstimator();
  timing_analysis_initialized_ = true;
}

void Dali::UpdateRCs() { rc_estimator->PushNetRCToManager(); }

void Dali::PerformTimingAnalysis() {
  phydb::ActPhyDBTimingAPI &timing_api = phy_db_ptr_->GetTimingApi();
  timing_api.UpdateTimingIncremental();
}

void Dali::UpdateNetWeights() { FetchSlacks(); }

/**
 * Log enough of a timing witness to locate its endpoints and branch points
 * without flooding the placement log when a path crosses hundreds of cells.
 */
static void LogTimingPathSteps(const TimingPathSnapshot &path) {
  if (path.steps.empty())
    return;

  LOG(info) << "    " << path.steps.front().source_pin << " -> "
            << path.steps.back().target_pin << "\n";
  constexpr std::size_t kLeadingTimingSteps = 10;
  constexpr std::size_t kTrailingTimingSteps = 5;
  const std::size_t step_count = path.steps.size();
  for (std::size_t index = 0; index < step_count; ++index) {
    if (index == kLeadingTimingSteps &&
        step_count > kLeadingTimingSteps + kTrailingTimingSteps) {
      LOG(info) << "    ... "
                << step_count - kLeadingTimingSteps - kTrailingTimingSteps
                << " steps omitted ...\n";
      index = step_count - kTrailingTimingSteps;
    }
    const TimingPathStep &step = path.steps[index];
    LOG(info) << "    " << step.source_pin << " -> " << step.target_pin;
    if (!step.net_name.empty()) {
      LOG(info) << " via " << step.net_name;
    }
    LOG(info) << " : " << step.delay << "\n";
  }
}

/**
 * Report slow-witness nets that a delay element can change without also
 * lengthening the current fast witness for the same relative constraint.
 */
static void
LogDelayRepairCandidates(const RelativeTimingViolationSnapshot &violation) {
  LOG(info) << "  slow-only repair nets : "
            << violation.delay_repair_candidate_nets.size() << "\n";
  for (const std::string &net_name : violation.delay_repair_candidate_nets) {
    LOG(info) << "    " << net_name << "\n";
  }
  LOG(info) << "  delay repair sites     : "
            << violation.delay_repair_candidate_sites.size() << "\n";
  for (const std::string &site : violation.delay_repair_candidate_sites) {
    LOG(info) << "    " << site << "\n";
  }
}

/** Update per-line diagnostics from the complete relative-timing snapshot. */
static bool AttributeDelayLineRequests(
    std::vector<Dali::DelayLineSpreadRequest> *requests,
    const TimingSnapshot &snapshot, bool verbose = true) {
  if (snapshot.relative_constraint_count !=
      static_cast<int>(snapshot.relative_constraints.size())) {
    LOG(error) << "Delay-line attribution requires all relative constraints: "
               << snapshot.relative_constraint_count << " reported, "
               << snapshot.relative_constraints.size() << " retained\n";
    return false;
  }

  std::vector<std::string> prefixes;
  prefixes.reserve(requests->size());
  for (const Dali::DelayLineSpreadRequest &request : *requests)
    prefixes.push_back(request.name_prefix);

  DelayLineAttributionResult result =
      AttributeDelayLineConstraints(prefixes, snapshot.relative_constraints);
  if (!result.valid()) {
    LOG(error) << "Delay-line attribution failed: " << result.error << "\n";
    return false;
  }

  for (std::size_t index = 0; index < requests->size(); ++index) {
    Dali::DelayLineSpreadRequest &request = (*requests)[index];
    const DelayLineConstraintAttribution &attribution = result.per_line[index];
    request.matched_constraint_ids = attribution.constraint_ids;
    request.binding_slack = attribution.binding_slack;
    request.has_binding_slack = attribution.has_binding_slack;
    request.binding_constraint_id = attribution.binding_constraint_id;
    // Probes re-attribute on every trial; logging each one would bury the
    // baseline attribution under hundreds of identical lines.
    if (!verbose) continue;
    LOG(info) << "  delay line '" << request.name_prefix
              << "' constraint count " << request.matched_constraint_ids.size()
              << ", binding constraint " << request.binding_constraint_id
              << ", binding slack " << request.binding_slack << " ps\n";
    std::ostringstream constraint_ids;
    constraint_ids << "  delay line '" << request.name_prefix
                   << "' constraint IDs :";
    for (int constraint_id : request.matched_constraint_ids) {
      constraint_ids << " " << constraint_id;
    }
    constraint_ids << "\n";
    LOG(debug) << constraint_ids.str();
  }
  return true;
}

bool Dali::ReportTiming() { return RefreshTiming(true); }

bool Dali::RefreshTiming(bool capture_witnesses) {
  ElapsedTime refresh_timer;
  refresh_timer.RecordStartTime();
  double measured_phase_wall_seconds = 0.0;
  if (phy_db_ptr_ == nullptr) {
    LOG(error) << "Timing analysis is unavailable without a PhyDB instance.\n";
    return false;
  }
  auto &timing_api = phy_db_ptr_->GetTimingApi();
  if (!timing_api.ReadyForCriticalCycleTiming() &&
      !timing_api.ReadyForTimingDriven()) {
    LOG(error) << "Timing analysis is unavailable. Attach a timing host before "
                  "running timing-report.\n";
    return false;
  }

  ElapsedTime phase_timer;
  phase_timer.RecordStartTime();
  InitializeCircuitFromPhyDBIfNeeded();
  LOG(info) << "Refresh placement-synchronized timing\n";
  LOG(info) << "  initialize timing bridge\n";
  InitializeTimingDrivenPlacement();
  phase_timer.RecordEndTime();
  runtime_breakdown_.timing_initialize_wall_seconds +=
      phase_timer.GetWallTime();
  measured_phase_wall_seconds += phase_timer.GetWallTime();
  if (!VerifyRcConfigurationIsEffective("timing-report")) {
    return false;
  }
  if (timing_use_rc_ && rc_estimator != nullptr) {
    LOG(info) << "  RC_LAYER configured " << rc_min_routing_layer_
              << " effective " << rc_estimator->MinRoutingLayer()
              << " horizontal " << rc_estimator->ResolvedHorizontalLayerName()
              << " vertical " << rc_estimator->ResolvedVerticalLayerName()
              << "\n";
  }
  LOG(info) << "  export component locations\n";
  phase_timer.RecordStartTime();
  ExportOrdinaryComponentsToPhyDB();
  phase_timer.RecordEndTime();
  runtime_breakdown_.timing_export_locations_wall_seconds +=
      phase_timer.GetWallTime();
  measured_phase_wall_seconds += phase_timer.GetWallTime();
  if (timing_use_rc_) {
    LOG(info) << "  update estimated interconnect RC\n";
    phase_timer.RecordStartTime();
    UpdateRCs();
    phase_timer.RecordEndTime();
    runtime_breakdown_.timing_update_rc_wall_seconds +=
        phase_timer.GetWallTime();
    measured_phase_wall_seconds += phase_timer.GetWallTime();
  } else {
    LOG(info) << "  skip estimated interconnect RC\n";
  }
  LOG(info) << "  run incremental timing analysis\n";
  phase_timer.RecordStartTime();
  PerformTimingAnalysis();
  phase_timer.RecordEndTime();
  runtime_breakdown_.timing_analysis_wall_seconds += phase_timer.GetWallTime();
  measured_phase_wall_seconds += phase_timer.GetWallTime();
  LOG(info) << (capture_witnesses ? "  capture timing witnesses\n"
                                  : "  capture timing endpoints and slacks\n");
  phase_timer.RecordStartTime();
  const std::vector<DelayRepairSite> effective_delay_sites =
      EffectiveDelayRepairSites();
  TimingSnapshotBuilder snapshot_builder(phy_db_ptr_, &effective_delay_sites);
  last_timing_snapshot_ = capture_witnesses
                              ? snapshot_builder.Capture()
                              : snapshot_builder.CaptureConstraintIdentities();
  phase_timer.RecordEndTime();
  runtime_breakdown_.timing_witness_capture_wall_seconds +=
      phase_timer.GetWallTime();
  measured_phase_wall_seconds += phase_timer.GetWallTime();
  refresh_timer.RecordEndTime();
  runtime_breakdown_.timing_other_wall_seconds +=
      std::max(0.0, refresh_timer.GetWallTime() -
                        measured_phase_wall_seconds);
  ++runtime_breakdown_.timing_refreshes;
  LOG(info) << "  timing refresh complete\n";

  LOG(info) << "Timing report\n";
  if (last_timing_snapshot_.has_critical_cycle) {
    LOG(info) << "  critical-cycle period : "
              << last_timing_snapshot_.critical_cycle_period << "\n";
    LOG(info) << "  unroll factor         : "
              << last_timing_snapshot_.critical_cycle_unroll_factor << "\n";
    LOG(info) << "  mapped critical legs  : "
              << last_timing_snapshot_.critical_cycle_nets.size() << "\n";
    if (timing_period_target_ > 0.0) {
      const double performance_slack =
          timing_period_target_ - last_timing_snapshot_.critical_cycle_period;
      LOG(info) << "  performance target    : " << timing_period_target_
                << "\n";
      LOG(info) << "  performance slack     : " << performance_slack << "\n";
      LOG(info) << "  performance status    : "
                << (performance_slack >= 0.0 ? "MET" : "VIOLATED") << "\n";
    }
  }
  LOG(info) << "  relative constraints  : "
            << last_timing_snapshot_.relative_constraint_count << "\n";
  if (last_timing_snapshot_.worst_relative_constraint_id >= 0) {
    LOG(info) << "  worst relative ID     : "
              << last_timing_snapshot_.worst_relative_constraint_id << "\n";
    LOG(info) << "  worst relative slack  : "
              << last_timing_snapshot_.worst_relative_slack << "\n";
    LOG(info) << "  total negative slack  : "
              << last_timing_snapshot_.relative_total_negative_slack << "\n";
  }
  LOG(info) << "  relative violations   : "
            << last_timing_snapshot_.relative_violations.size() << "\n";
  if (!last_timing_snapshot_.relative_violations.empty()) {
    const RelativeTimingViolationSnapshot &worst =
        last_timing_snapshot_.relative_violations.front();
    LOG(info) << "  worst fast witness    : " << worst.fast_path.TotalDelay()
              << " (" << worst.fast_path.steps.size() << " steps)\n";
    LogTimingPathSteps(worst.fast_path);
    LOG(info) << "  worst slow witness    : " << worst.slow_path.TotalDelay()
              << " (" << worst.slow_path.steps.size() << " steps)\n";
    LogTimingPathSteps(worst.slow_path);
    LogDelayRepairCandidates(worst);
  }
  LOG(info) << "  unweighted HPWL       : " << circuit_.UnweightedHPWL()
            << " um\n";
  return true;
}

bool Dali::WriteTimingRepairPlan(const std::string &file_name) {
  if (!ReportTiming())
    return false;
  if (!WriteTimingRepairPlanJson(last_timing_snapshot_, file_name)) {
    LOG(error) << "Cannot write timing repair plan: " << file_name << "\n";
    return false;
  }
  LOG(info) << "Timing repair plan written to " << file_name << " ("
            << last_timing_snapshot_.delay_repair_candidates.size()
            << " candidates)\n";
  return true;
}

bool Dali::WriteTimingConstraintIdentities(
    const std::string &file_name,
    const std::vector<std::string> &replaceable_site_prefixes) {
  if (!ReportTiming()) return false;
  return WriteCurrentTimingConstraintIdentities(file_name,
                                                 replaceable_site_prefixes);
}

bool Dali::WriteCurrentTimingConstraintIdentities(
    const std::string &file_name,
    const std::vector<std::string> &replaceable_site_prefixes,
    unsigned long long *identity_digest, std::size_t *identity_count) {
  if (phy_db_ptr_ == nullptr ||
      !phy_db_ptr_->GetTimingApi().ReadyForTimingDriven()) {
    LOG(error) << "Timing constraint identities require a linked timing host.\n";
    return false;
  }
  TimingSnapshot snapshot =
      TimingSnapshotBuilder(phy_db_ptr_, &delay_repair_sites_)
          .CaptureConstraintIdentities();
  std::vector<std::string> effective_prefixes = replaceable_site_prefixes;
  effective_prefixes.reserve(effective_prefixes.size() +
                             delay_line_spread_requests_.size());
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    effective_prefixes.push_back(request.name_prefix);
  }
  CanonicalizeReplaceableSiteEndpoints(&snapshot, effective_prefixes);
  // Order-independent, so a permutation of the same set is not mistaken for a
  // change, while any altered endpoint is. Summarizing per sample is what makes
  // a claim about every sample checkable: the JSON is one file per stage and
  // was overwritten by each of the forty-five accepted-physical captures, so
  // only the last one's identities ever survived a run.
  if (identity_digest != nullptr || identity_count != nullptr) {
    unsigned long long digest = 0;
    for (const RelativeTimingConstraintSnapshot &constraint :
         snapshot.relative_constraints) {
      unsigned long long entry = 1469598103934665603ull;
      for (char character : (std::to_string(constraint.constraint_id) + "=" +
                             constraint.SemanticIdentity())) {
        entry ^= static_cast<unsigned long long>(
            static_cast<unsigned char>(character));
        entry *= 1099511628211ull;
      }
      digest += entry;
    }
    if (identity_digest != nullptr) *identity_digest = digest;
    if (identity_count != nullptr) {
      *identity_count = snapshot.relative_constraints.size();
    }
  }
  if (!WriteTimingConstraintIdentitiesJson(snapshot, file_name)) {
    LOG(error) << "Cannot write timing constraint identities: " << file_name
               << "\n";
    return false;
  }
  LOG(info) << "Timing constraint identities written to " << file_name << " ("
            << snapshot.relative_constraints.size() << " constraints)\n";
  return true;
}

bool Dali::WriteCurrentTimingConstraintEndpointIdentities(
    const std::string &file_name,
    const std::vector<std::string> &replaceable_site_prefixes,
    unsigned long long *identity_digest, std::size_t *identity_count) {
  if (phy_db_ptr_ == nullptr ||
      !phy_db_ptr_->GetTimingApi().ReadyForTimingDriven()) {
    LOG(error) << "Timing constraint endpoint identities require a linked "
                  "timing host.\n";
    return false;
  }
  TimingSnapshot snapshot =
      TimingSnapshotBuilder(phy_db_ptr_, &delay_repair_sites_)
          .CaptureConstraintEndpointIdentities();
  std::vector<std::string> effective_prefixes = replaceable_site_prefixes;
  effective_prefixes.reserve(effective_prefixes.size() +
                             delay_line_spread_requests_.size());
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    effective_prefixes.push_back(request.name_prefix);
  }
  CanonicalizeReplaceableSiteEndpoints(&snapshot, effective_prefixes);
  if (identity_digest != nullptr || identity_count != nullptr) {
    unsigned long long digest = 0;
    for (const RelativeTimingConstraintSnapshot &constraint :
         snapshot.relative_constraints) {
      unsigned long long entry = 1469598103934665603ull;
      for (char character : (std::to_string(constraint.constraint_id) + "=" +
                             constraint.SemanticIdentity())) {
        entry ^= static_cast<unsigned long long>(
            static_cast<unsigned char>(character));
        entry *= 1099511628211ull;
      }
      digest += entry;
    }
    if (identity_digest != nullptr) *identity_digest = digest;
    if (identity_count != nullptr) {
      *identity_count = snapshot.relative_constraints.size();
    }
  }
  if (!WriteTimingConstraintIdentitiesJson(snapshot, file_name)) {
    LOG(error) << "Cannot write timing constraint endpoint identities: "
               << file_name << "\n";
    return false;
  }
  return true;
}

bool Dali::WriteCurrentTimingDecomposition(
    const std::string &file_name,
    const std::vector<std::string> &delay_site_prefixes) {
  if (phy_db_ptr_ == nullptr ||
      !phy_db_ptr_->GetTimingApi().ReadyForTimingDriven()) {
    LOG(error) << "Timing decomposition requires a linked timing host.\n";
    return false;
  }
  const std::vector<DelayRepairSite> effective_delay_sites =
      EffectiveDelayRepairSites(delay_site_prefixes);
  const TimingSnapshot snapshot =
      TimingSnapshotBuilder(phy_db_ptr_, &effective_delay_sites).Capture();
  if (!WriteTimingDecompositionJson(snapshot, file_name)) {
    LOG(error) << "Cannot write timing decomposition: " << file_name << "\n";
    return false;
  }
  LOG(info) << "Timing decomposition written to " << file_name << " ("
            << snapshot.relative_constraints.size() << " constraints)\n";
  return true;
}

bool Dali::CheckTiming() {
  if (timing_period_target_ <= 0.0) {
    LOG(error) << "Set timing_period_target before running timing-check.\n";
    return false;
  }
  if (!ReportTiming()) {
    return false;
  }

  const bool performance_met =
      last_timing_snapshot_.has_critical_cycle &&
      last_timing_snapshot_.critical_cycle_period <= timing_period_target_;
  const bool relative_constraints_met =
      last_timing_snapshot_.relative_violations.empty();
  const bool timing_met = performance_met && relative_constraints_met;
  LOG(info) << "Timing signoff: " << (timing_met ? "PASS" : "FAIL") << "\n";
  if (!last_timing_snapshot_.has_critical_cycle) {
    LOG(error) << "  no critical-cycle result is available\n";
  }
  return timing_met;
}

bool Dali::WeightCriticalCycleNets(double multiplier) {
  if (multiplier <= 0.0) {
    LOG(error) << "Critical-cycle net-weight multiplier must be positive.\n";
    return false;
  }
  if (!last_timing_snapshot_.has_critical_cycle) {
    LOG(error) << "Run timing-report before weighting critical-cycle nets.\n";
    return false;
  }

  std::unordered_set<std::string> weighted_net_names;
  for (const CriticalCycleNetStep &step :
       last_timing_snapshot_.critical_cycle_nets) {
    if (!circuit_.IsNetExisting(step.net_name) ||
        !weighted_net_names.insert(step.net_name).second) {
      continue;
    }
    Net *net = circuit_.GetNetPtr(step.net_name);
    net->SetWeight(net->Weight() * multiplier);
  }
  LOG(info) << "Weighted " << weighted_net_names.size()
            << " critical-cycle nets by " << multiplier << "x\n";
  return !weighted_net_names.empty();
}

bool Dali::WeightFastPathNets(double multiplier) {
  if (multiplier <= 0.0) {
    LOG(error) << "Fast-path net-weight multiplier must be positive.\n";
    return false;
  }
  if (last_timing_snapshot_.relative_constraint_count == 0) {
    LOG(error) << "Run timing-report before weighting fast-path nets.\n";
    return false;
  }

  std::unordered_set<std::string> weighted_net_names;
  for (const TimingNetCandidateSnapshot &candidate :
       last_timing_snapshot_.fast_path_placement_candidates) {
    const std::string &net_name = candidate.net_name;
    if (!circuit_.IsNetExisting(net_name) ||
        !weighted_net_names.insert(net_name).second) {
      continue;
    }
    Net *net = circuit_.GetNetPtr(net_name);
    net->SetWeight(net->Weight() * multiplier);
  }
  LOG(info) << "Weighted " << weighted_net_names.size()
            << " fast-path nets by " << multiplier << "x\n";
  return !weighted_net_names.empty();
}

bool Dali::DetourDelayLine(const std::string &name_prefix, double amplitude) {
  if (amplitude <= 0.0) {
    LOG(error) << "Detour amplitude must be positive.\n";
    return false;
  }

  InitializeCircuitFromPhyDBIfNeeded();

  DelayLineChain chain;
  std::string error_message;
  if (!BuildDelayLineChain(circuit_, name_prefix, &chain, &error_message)) {
    LOG(error) << "Cannot detour '" << name_prefix << "': " << error_message
               << "\n";
    return false;
  }
  if (chain.nodes.size() < 3) {
    LOG(error) << "Delay line '" << name_prefix << "' has "
               << chain.nodes.size()
               << " elements, too few to detour with both ends pinned.\n";
    return false;
  }

  double grid_x = circuit_.GridValueX();
  double grid_y = circuit_.GridValueY();
  for (DelayLineNode &node : chain.nodes) {
    node.x *= grid_x;
    node.y *= grid_y;
  }

  std::vector<Component> &components = circuit_.Components();
  int moved = 0;
  int fixed = 0;
  for (const DetourTarget &target : BuildZigzagDetour(chain, amplitude)) {
    Component &component = components[target.component_id];
    if (!component.IsMovable()) {
      ++fixed;
      continue;
    }
    component.SetLLX(target.x / grid_x);
    component.SetLLY(target.y / grid_y);
    ++moved;
  }

  if (fixed > 0) {
    LOG(warning) << fixed << " of " << chain.nodes.size() - 2
                 << " interior elements of '" << name_prefix
                 << "' are fixed and were not moved.\n";
  }
  LOG(info) << "Detoured '" << name_prefix << "': " << chain.nodes.size()
            << " elements, moved " << moved << " by amplitude " << amplitude
            << " um\n";
  return moved > 0;
}

/**
 * Record a delay line for attribution and insertion, imposing no geometry.
 *
 * Validates that the named cells form a chain, because a caller that mistypes a
 * prefix would otherwise register a line that silently matches nothing and
 * absorb every constraint attributed to it.
 */
bool Dali::RegisterDelayLine(const std::string &name_prefix) {
  InitializeCircuitFromPhyDBIfNeeded();

  DelayLineChain chain;
  std::string error_message;
  if (!BuildDelayLineChain(circuit_, name_prefix, &chain, &error_message)) {
    LOG(error) << "Cannot register '" << name_prefix << "': " << error_message
               << "\n";
    return false;
  }

  auto existing = std::find_if(
      delay_line_spread_requests_.begin(), delay_line_spread_requests_.end(),
      [&name_prefix](const DelayLineSpreadRequest &request) {
        return request.name_prefix == name_prefix;
      });
  if (existing != delay_line_spread_requests_.end()) {
    existing->shaped = false;
    existing->separation = 0;
  } else {
    DelayLineSpreadRequest request;
    request.name_prefix = name_prefix;
    request.shaped = false;
    delay_line_spread_requests_.push_back(std::move(request));
  }

  LOG(info) << "Registered '" << name_prefix << "': " << chain.nodes.size()
            << " elements, placed freely\n";
  return true;
}

/**
 * Move one delay line's movable cells to the geometry a separation implies.
 *
 * Coordinates only: no request bookkeeping, no logging, no clamping. Split out
 * of SpreadDelayLineAcrossRows because a gain probe has to try a shape and put
 * it back without the request believing the separation changed, and a probe
 * that reused the recorded path would reset the very gain it is measuring.
 *
 * Grid units throughout: a row is as tall as the macro it holds and a column as
 * wide, both of which the placement grid already quantizes. The chain is always
 * folded -- the fold adds no delay, and it returns the chain's output to the
 * side its input entered from so the control circuitry need not straddle the
 * datapath.
 */
void Dali::MoveDelayLineCells(const DelayLineChain &chain, Macro *macro,
                              int separation, int column_stride,
                              int column_pitch,
                              std::vector<Component> *components, int *moved,
                              int *fixed) {
  const int signed_separation =
      ShouldExtendDelayLineDownward(chain, macro) ? -separation : separation;
  const std::vector<DetourTarget> targets = BuildInterleavedRowBandTargets(
      chain, signed_separation, macro->Height(),
      macro->Width() * std::max(1, column_pitch), column_stride);

  for (const DetourTarget &target : targets) {
    Component &component = (*components)[target.component_id];
    if (!component.IsMovable()) {
      if (fixed != nullptr) ++*fixed;
      continue;
    }
    component.SetLLX(target.x);
    component.SetLLY(target.y);
    if (moved != nullptr) ++*moved;
  }
}

bool Dali::SpreadDelayLineAcrossRows(const std::string &name_prefix,
                                     int separation, int column_stride,
                                     int column_pitch) {
  if (separation < 0) {
    LOG(error) << "Row separation cannot be negative.\n";
    return false;
  }

  InitializeCircuitFromPhyDBIfNeeded();

  DelayLineChain chain;
  std::string error_message;
  if (!BuildDelayLineChain(circuit_, name_prefix, &chain, &error_message)) {
    LOG(error) << "Cannot spread '" << name_prefix << "': " << error_message
               << "\n";
    return false;
  }

  std::vector<Component> &components = circuit_.Components();
  Macro *macro = components[chain.nodes.front().component_id].MacroPtr();
  for (const DelayLineNode &node : chain.nodes) {
    if (components[node.component_id].MacroPtr() != macro) {
      LOG(error) << "Delay line '" << name_prefix
                 << "' mixes macros, so it does not define rows of one "
                    "height.\n";
      return false;
    }
  }

  separation = ClampDelayLineSeparation(chain, macro, separation,
                                         "spread-delay-line");

  int moved = 0;
  int fixed = 0;
  MoveDelayLineCells(chain, macro, separation, column_stride, column_pitch,
                     &components, &moved, &fixed);

  if (fixed > 0) {
    LOG(warning) << fixed << " of " << chain.nodes.size() << " elements of '"
                 << name_prefix << "' are fixed and were not moved.\n";
  }

  // Also remembered, so a later global placement re-imposes the shape instead
  // of abutting the chain again.
  auto existing = std::find_if(
      delay_line_spread_requests_.begin(), delay_line_spread_requests_.end(),
      [&name_prefix](const DelayLineSpreadRequest &request) {
        return request.name_prefix == name_prefix;
      });
  if (existing == delay_line_spread_requests_.end()) {
    DelayLineSpreadRequest request;
    request.name_prefix = name_prefix;
    request.shaped = true;
    request.separation = separation;
    request.column_stride = column_stride;
    request.column_pitch = std::max(1, column_pitch);
    delay_line_spread_requests_.push_back(std::move(request));
  } else {
    existing->shaped = true;
    existing->column_stride = column_stride;
    existing->column_pitch = std::max(1, column_pitch);
    if (existing->separation != separation) {
      existing->separation = separation;
      existing->previous_slack = 0.0;
      existing->previous_separation = -1;
      existing->gain_per_row = 0.0;
    }
  }

  LOG(info) << "Spread '" << name_prefix << "': " << chain.nodes.size()
            << " elements folded into "
            << (chain.nodes.size() + 1) / 2 << " columns across 2 rows, "
            << "separation " << separation << ", stride "
            << EffectiveColumnStride(
                   static_cast<int>(chain.nodes.size() + 1) / 2, column_stride)
            << ", pitch " << std::max(1, column_pitch) << ", moved " << moved
            << "\n";
  return moved > 0;
}


int Dali::MaxDelayLineSeparation(const DelayLineChain& chain,
                                 const Macro* macro) const {
  if (chain.nodes.empty() || macro == nullptr || macro->Height() <= 0) return 0;
  // A backstop against a request that cannot fit the region at all, not a
  // routine limiter. Bounding tightly by the room on one side of the chain's
  // head was tried and costs more than it buys: the head moves as placement
  // proceeds, so the bound goes stale and clamps requests that legalization
  // would have resolved. Measured on the one-bit fixture, the tight bound drove
  // separations from 79/22 down to 62/15 and left a constraint violating.
  const int rows = circuit_.RegionHeight() / macro->Height();
  return std::max(0, rows - 1);
}

/** Return true when the band should extend downward from the chain's head. */
bool Dali::ShouldExtendDelayLineDownward(const DelayLineChain& chain,
                                         const Macro* macro) const {
  if (chain.nodes.empty() || macro == nullptr) return false;
  const double head_y = chain.nodes.front().y;
  const double above = circuit_.RegionURY() - head_y - macro->Height();
  const double below = head_y - circuit_.RegionLLY();
  return below > above;
}

int Dali::ClampDelayLineSeparation(const DelayLineChain& chain,
                                   const Macro* macro, int separation,
                                   const char* reason) const {
  const int max_separation = MaxDelayLineSeparation(chain, macro);
  const int clamped = std::min(std::max(0, separation), max_separation);
  if (clamped != separation) {
    LOG(warning) << "DELAY_LINE_AREA_BOUND line " << chain.name
                 << " reason " << reason << " requested_separation "
                 << separation << " applied_separation " << clamped
                 << " max_separation " << max_separation
                 << " residual_area_bound 1\n";
  }
  return clamped;
}

bool Dali::CloseTimingWithDelayLineSpread(double margin, int initial_step,
                                          int max_rounds) {
  if (delay_line_spread_requests_.empty()) {
    LOG(error) << "No delay line registered; run spread-delay-line first.\n";
    return false;
  }
  if (initial_step < 1 || max_rounds < 1) {
    LOG(error) << "Initial step and round count must be positive.\n";
    return false;
  }

  struct ClosureBracket {
    int widest_violating = -1;
    int narrowest_satisfying = -1;
  };
  std::vector<ClosureBracket> brackets(delay_line_spread_requests_.size());
  for (DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    request.previous_slack = 0.0;
    request.previous_separation = -1;
    request.gain_per_row = 0.0;
  }

  // Every round after the first continues from the previous round's placement,
  // with only the delay lines reshaped to the separation about to be tried.
  // Re-initializing instead would throw away a good datapath placement and make
  // each round an independent search, so consecutive rounds would differ by far
  // more than the one variable under test. Reshaping before placing rather than
  // after is what makes the carried-over placement a usable anchor: the first
  // solve of the round would otherwise pull toward the previous separation.
  PlacementInitializerType original_initializer = global_initializer_;
  bool first_round = true;
  auto measure = [&](int round) {
    if (!first_round) {
      global_initializer_ = PlacementInitializerType::kKeep;
      for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
        SpreadDelayLineAcrossRows(request.name_prefix, request.separation,
                                  request.column_stride,
                                  request.column_pitch);
      }
    }
    first_round = false;
    if (!StartPlacement() || !ReportTiming()) return false;
    if (!AttributeDelayLineRequests(&delay_line_spread_requests_,
                                    last_timing_snapshot_))
      return false;
    LOG(info) << "Delay-line closure round " << round << ":\n";
    for (const DelayLineSpreadRequest &request :
         delay_line_spread_requests_) {
      LOG(info) << "  '" << request.name_prefix << "' separation "
                << request.separation << ", binding slack "
                << request.binding_slack << " ps\n";
    }
    return true;
  };
  struct InitializerRestore {
    PlacementInitializerType *slot;
    PlacementInitializerType value;
    ~InitializerRestore() { *slot = value; }
  } restore{&global_initializer_, original_initializer};

  int round = 0;
  while (round < max_rounds) {
    if (!measure(round++)) return false;
    bool all_satisfied = true;
    for (std::size_t index = 0; index < delay_line_spread_requests_.size();
         ++index) {
      DelayLineSpreadRequest &request = delay_line_spread_requests_[index];
      ClosureBracket &bracket = brackets[index];
      const int separation = request.separation;
      const double slack = request.binding_slack;
      if (request.previous_separation >= 0 &&
          separation != request.previous_separation) {
        const double measured =
            (slack - request.previous_slack) /
            (separation - request.previous_separation);
        if (measured > 0.0) request.gain_per_row = measured;
      }
      request.previous_slack = slack;
      request.previous_separation = separation;
      if (slack >= margin) {
        if (bracket.narrowest_satisfying < 0 ||
            separation < bracket.narrowest_satisfying) {
          bracket.narrowest_satisfying = separation;
        }
        continue;
      }

      all_satisfied = false;
      bracket.widest_violating = separation;
      int step = initial_step;
      if (request.gain_per_row > 0.0) {
        step = static_cast<int>(std::ceil(
            (margin - slack) / request.gain_per_row));
        if (step < 1) step = 1;
      }
      request.separation = separation + step;
      LOG(info) << "  '" << request.name_prefix << "' short by "
                << (margin - slack) << " ps, gain "
                << request.gain_per_row << " ps/row, next separation "
                << request.separation << "\n";
    }
    if (all_satisfied) break;
  }

  for (const ClosureBracket &bracket : brackets) {
    if (bracket.narrowest_satisfying < 0) {
      LOG(error) << "Delay-line closure did not meet the margin in "
                 << max_rounds << " rounds.\n";
      return false;
    }
  }

  while (round < max_rounds) {
    bool any_bracket_to_tighten = false;
    for (std::size_t index = 0; index < brackets.size(); ++index) {
      ClosureBracket &bracket = brackets[index];
      if (bracket.widest_violating < 0 ||
          bracket.narrowest_satisfying - bracket.widest_violating <= 1)
        continue;
      any_bracket_to_tighten = true;
      delay_line_spread_requests_[index].separation =
          bracket.widest_violating +
          (bracket.narrowest_satisfying - bracket.widest_violating) / 2;
    }
    if (!any_bracket_to_tighten) break;
    if (!measure(round++)) return false;
    for (std::size_t index = 0; index < brackets.size(); ++index) {
      ClosureBracket &bracket = brackets[index];
      const DelayLineSpreadRequest &request = delay_line_spread_requests_[index];
      if (bracket.widest_violating < 0 ||
          bracket.narrowest_satisfying - bracket.widest_violating <= 1)
        continue;
      if (request.binding_slack >= margin)
        bracket.narrowest_satisfying = request.separation;
      else
        bracket.widest_violating = request.separation;
    }
  }

  for (std::size_t index = 0; index < brackets.size(); ++index)
    delay_line_spread_requests_[index].separation =
        brackets[index].narrowest_satisfying;
  global_initializer_ = PlacementInitializerType::kKeep;
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    SpreadDelayLineAcrossRows(request.name_prefix, request.separation,
                              request.column_stride, request.column_pitch);
  }
  if (!StartPlacement() || !ReportTiming()) return false;
  if (!AttributeDelayLineRequests(&delay_line_spread_requests_,
                                  last_timing_snapshot_))
    return false;
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    if (!request.has_binding_slack || request.binding_slack < margin) {
      LOG(error) << "Delay-line closure final margin failed for '"
                 << request.name_prefix << "': binding slack "
                 << request.binding_slack << " ps, required " << margin
                 << " ps\n";
      return false;
    }
    LOG(info) << "Constraints met for '" << request.name_prefix
              << "' at separation " << request.separation
              << ", binding slack " << request.binding_slack << " ps\n";
  }
  return true;
}

void Dali::EnableDelayLineTimingFeedback(double margin, int initial_step,
                                         int warmup, int interval, int freeze,
                                         double damping, int max_separation) {
  delay_line_feedback_ = DelayLineFeedbackState();
  delay_line_feedback_.enabled = true;
  delay_line_feedback_.margin = margin;
  delay_line_feedback_.initial_step = std::max(1, initial_step);
  delay_line_feedback_.warmup = warmup;
  delay_line_feedback_.interval = std::max(1, interval);
  delay_line_feedback_.freeze = freeze;
  delay_line_feedback_.damping = damping;
  delay_line_feedback_.max_separation = max_separation;
  for (DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    request.previous_slack = 0.0;
    request.previous_separation = -1;
    request.gain_per_row = 0.0;
  }
  LOG(info) << "Delay-line timing feedback enabled: margin " << margin
            << " ps, warmup " << warmup << ", interval " << interval
            << ", freeze " << freeze << ", damping " << damping
            << ", max separation " << max_separation << "\n";
}

/**
 * Measure slack on the placement in progress and retune separation from it.
 *
 * The correction uses the gain per row measured across the last two
 * evaluations, never an assumed rate, because what a row buys is not
 * predictable in closed form. It is damped because the placement has not
 * settled: an undamped step taken from an unsettled estimate overshoots and the
 * next evaluation corrects back.
 */
/**
 * Below this, another row of separation is not worth requesting.
 *
 * Set well under the 23-215 ps/row the converging lines on `bd_pipeline`
 * measure and well over the 0.27-4.9 ps/row the stalled ones do, so it
 * separates the two populations rather than tuning to either.
 */
static constexpr double kStalledGainPsPerRow = 10.0;

/**
 * Widest column pitch the controller will escalate to, in element widths.
 *
 * One, meaning the controller does not escalate pitch on its own. Escalation
 * was implemented and measured on `bd_pipeline` and does not pay: the stalled
 * lines gained no slack from the extra width -- dl0 sat at -504 ps with pitch
 * 16, against -553 with pitch 1 -- while a seven-column line at pitch 16 grows
 * to 269 um wide and four of them stop fitting side by side, which lost
 * legalization outright. The pitch argument on `spread-delay-line` stays, so a
 * design that wants width can still ask for it; what is disabled is the
 * controller reaching for it unprompted.
 */
static constexpr int kMaxColumnPitch = 1;

/**
 * The semantic identity of a constraint, or empty when it has none.
 *
 * Identity rather than numeric id, because the timer renumbers constraints and
 * a probe comparing numbers would call two different paths the same path.
 */
std::string Dali::BindingConstraintIdentity(int constraint_id) const {
  if (constraint_id < 0) return std::string();
  for (const RelativeTimingConstraintSnapshot &constraint :
       last_timing_snapshot_.relative_constraints) {
    if (constraint.constraint_id == constraint_id) {
      TimingSnapshot canonical;
      canonical.relative_constraints.push_back(constraint);
      std::vector<std::string> prefixes;
      prefixes.reserve(delay_line_spread_requests_.size());
      for (const DelayLineSpreadRequest &request :
           delay_line_spread_requests_) {
        prefixes.push_back(request.name_prefix);
      }
      CanonicalizeReplaceableSiteEndpoints(&canonical, prefixes);
      return canonical.relative_constraints.front().SemanticIdentity();
    }
  }
  return std::string();
}

int Dali::DelayLineRequestColumnStride(const std::string &name_prefix) const {
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    if (request.name_prefix == name_prefix) return request.column_stride;
  }
  return 1;
}

int Dali::DelayLineRequestColumnPitch(const std::string &name_prefix) const {
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    if (request.name_prefix == name_prefix) return std::max(1, request.column_pitch);
  }
  return 1;
}

std::vector<IsolatedGainProbe> Dali::ProbeDelayLineGains(
    int iteration, const std::vector<ProbeSiteRequest> &requests) {
  ProbeHooks hooks;
  hooks.snapshot = [this](const std::string &site) {
    return SnapshotDelayLinePlacement(site);
  };
  hooks.apply_shape = [this](const std::string &site, int separation) {
    DelayLineChain chain;
    std::string error_message;
    if (!BuildDelayLineChain(circuit_, site, &chain, &error_message)) {
      return false;
    }
    std::vector<Component> &components = circuit_.Components();
    Macro *macro = components[chain.nodes.front().component_id].MacroPtr();
    const int stride = DelayLineRequestColumnStride(site);
    const int pitch = DelayLineRequestColumnPitch(site);
    MoveDelayLineCells(chain, macro, separation, stride, pitch, &components,
                       nullptr, nullptr);
    return true;
  };
  // The refresh is the ordinary placement-synchronized one: export coordinates,
  // update RC, run incremental timing, capture witnesses. No placement solve is
  // reachable from here, which is the property the probe depends on.
  hooks.refresh_timing = [this]() { return ReportTiming(); };
  hooks.measure = [this](const std::string &site) {
    ProbeMeasurement measurement;
    std::vector<DelayLineSpreadRequest> probe_requests;
    for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
      DelayLineSpreadRequest copy;
      copy.name_prefix = request.name_prefix;
      probe_requests.push_back(std::move(copy));
    }
    if (!AttributeDelayLineRequests(&probe_requests, last_timing_snapshot_,
                                   /*verbose=*/false)) {
      measurement.attributed_constraints = 0;
      return measurement;
    }
    for (const DelayLineSpreadRequest &request : probe_requests) {
      if (request.name_prefix != site) continue;
      measurement.slack = request.binding_slack;
      measurement.attributed_constraints =
          static_cast<int>(request.matched_constraint_ids.size());
      measurement.attribution_unique = request.has_binding_slack;
      measurement.identity =
          BindingConstraintIdentity(request.binding_constraint_id);
    }
    return measurement;
  };
  hooks.restore = [this](const std::vector<ComponentPlacement> &placements) {
    RestoreDelayLinePlacement(placements);
  };

  const std::vector<IsolatedGainProbe> probes =
      RunIsolatedGainProbes(requests, hooks);
  for (const IsolatedGainProbe &probe : probes) {
    LOG(info) << "DELAY_LINE_GAIN_PROBE iteration " << iteration << " site "
              << probe.site << " constraint_ids";
    for (int constraint_id : probe.constraint_ids)
      LOG(info) << " " << constraint_id;
    LOG(info) << " binding_identity '" << probe.binding_identity
              << "' baseline_separation " << probe.baseline_separation
              << " trial_separation " << probe.trial_separation
              << " baseline_slack_ps " << probe.baseline_slack
              << " trial_slack_ps " << probe.trial_slack
              << " isolated_gain_ps_per_row " << probe.gain_ps_per_row
              << " baseline_digest " << probe.baseline_digest
              << " restored_digest " << probe.restored_digest << " result "
              << ToString(probe.outcome) << "\n";
  }
  return probes;
}

/**
 * Measure slack mid-placement and retune separation from what it shows.
 *
 * Two phases, because a gain is only meaningful against a placement that did
 * not move while it was being measured. The probe phase tries each line's
 * proposed shape against one frozen placement and puts the coordinates back
 * exactly; the apply phase then chooses every accepted separation from those
 * isolated gains and imposes the accepted shapes together.
 *
 * The gain is never derived from slack values separated by a global-placement
 * iteration. That arithmetic is what this replaces: on the width-1 fixture a
 * spreading step moved dl0's slack 126 ps across a one-row change and the
 * controller concluded 125.9 ps/row, twelve times the truth, then computed a
 * zero step from its own number and stopped.
 */
bool Dali::RetuneDelayLineSeparation(
    int iteration, std::vector<GlobalPlacer::DelayLineShape> *shapes) {
  if (!ReportTiming()) return false;
  if (!AttributeDelayLineRequests(&delay_line_spread_requests_,
                                  last_timing_snapshot_))
    return false;

  pending_delay_line_feedback_events_.clear();

  // ---- baseline, captured before anything moves ---------------------------
  struct SiteBaseline {
    int separation = 0;
    double slack = 0.0;
    std::string identity;
    std::vector<int> constraint_ids;
    bool attribution_unique = false;
    int attributed_constraints = 0;
    int area_limit = 0;
    bool has_chain = false;
  };
  std::vector<SiteBaseline> baselines(delay_line_spread_requests_.size());
  std::vector<ProbeSiteRequest> probe_requests;

  for (std::size_t index = 0; index < delay_line_spread_requests_.size();
       ++index) {
    DelayLineSpreadRequest &request = delay_line_spread_requests_[index];
    SiteBaseline &baseline = baselines[index];
    baseline.separation = request.separation;
    baseline.slack = request.binding_slack;
    baseline.constraint_ids = request.matched_constraint_ids;
    baseline.attributed_constraints =
        static_cast<int>(request.matched_constraint_ids.size());
    baseline.attribution_unique = request.has_binding_slack;
    baseline.identity = BindingConstraintIdentity(request.binding_constraint_id);

    DelayLineChain chain;
    std::string chain_error;
    if (BuildDelayLineChain(circuit_, request.name_prefix, &chain,
                            &chain_error)) {
      baseline.has_chain = true;
      const Macro *macro =
          circuit_.Components()[chain.nodes.front().component_id].MacroPtr();
      baseline.area_limit = MaxDelayLineSeparation(chain, macro);
    }
    if (!request.shaped) continue;

    // The trial is one step in the direction the deficit points, sized by the
    // gain already measured or by the configured first step when there is none.
    // Its only job is to be a separation different enough to measure; the
    // accepted separation is chosen afterwards from what it measured.
    const double deficit = delay_line_feedback_.margin - baseline.slack;
    int trial_step = 0;
    if (request.gain_per_row > 0.0) {
      trial_step = static_cast<int>(std::ceil(
          delay_line_feedback_.damping * deficit / request.gain_per_row));
    } else if (deficit > 0.0) {
      trial_step = delay_line_feedback_.initial_step;
    }
    if (trial_step == 0) continue;

    int trial = std::max(0, baseline.separation + trial_step);
    if (delay_line_feedback_.max_separation > 0 &&
        trial > delay_line_feedback_.max_separation) {
      trial = delay_line_feedback_.max_separation;
    }
    if (baseline.has_chain) {
      DelayLineChain trial_chain;
      std::string trial_error;
      if (BuildDelayLineChain(circuit_, request.name_prefix, &trial_chain,
                              &trial_error)) {
        const Macro *macro =
            circuit_.Components()[trial_chain.nodes.front().component_id]
                .MacroPtr();
        trial = ClampDelayLineSeparation(trial_chain, macro, trial,
                                         "gain-probe");
      }
    }
    if (trial == baseline.separation) continue;

    ProbeSiteRequest probe;
    probe.site = request.name_prefix;
    probe.constraint_ids = baseline.constraint_ids;
    probe.baseline_separation = baseline.separation;
    probe.trial_separation = trial;
    probe.baseline_slack = baseline.slack;
    probe.baseline_identity = baseline.identity;
    probe.baseline_attributed_constraints = baseline.attributed_constraints;
    probe.baseline_attribution_unique = baseline.attribution_unique;
    probe.within_area_bound =
        !baseline.has_chain || trial <= baseline.area_limit;
    probe_requests.push_back(std::move(probe));
  }

  // ---- probe phase --------------------------------------------------------
  std::map<std::string, IsolatedGainProbe> measured;
  if (!probe_requests.empty()) {
    for (const IsolatedGainProbe &probe :
         ProbeDelayLineGains(iteration, probe_requests)) {
      measured[probe.site] = probe;
    }
    // Every probe restored its own coordinates, but the timer last saw a trial.
    // Put it back on the baseline placement before any decision reads timing.
    if (!ReportTiming()) return false;
  }

  // ---- apply phase --------------------------------------------------------
  bool changed = false;
  for (std::size_t index = 0; index < delay_line_spread_requests_.size();
       ++index) {
    DelayLineSpreadRequest &request = delay_line_spread_requests_[index];
    const SiteBaseline &baseline = baselines[index];
    const int separation = baseline.separation;
    const double slack = baseline.slack;

    // A refused probe leaves the gain untouched and the separation alone. It
    // deliberately does not fall back on the previous iteration's number: that
    // number described a placement that no longer exists, which is the failure
    // this design exists to remove.
    const char *gain_source = "none";
    auto probe = measured.find(request.name_prefix);
    if (probe != measured.end() && probe->second.outcome ==
                                       ProbeOutcome::kAccepted) {
      request.gain_per_row = probe->second.gain_ps_per_row;
      gain_source = "same_placement_probe";
    } else if (probe != measured.end()) {
      LOG(info) << "  delay line '" << request.name_prefix
                << "' gain probe refused (" << ToString(probe->second.outcome)
                << "); separation unchanged this iteration\n";
      continue;
    } else if (request.gain_per_row > 0.0) {
      gain_source = "same_placement_probe";
    }

    const double gain_per_row = request.gain_per_row;
    const double deficit = delay_line_feedback_.margin - slack;
    int step = 0;
    if (gain_per_row > 0.0) {
      if (std::abs(deficit) >= gain_per_row) {
        step = static_cast<int>(std::ceil(
            delay_line_feedback_.damping * deficit / gain_per_row));
      }
    } else if (deficit > 0.0) {
      step = delay_line_feedback_.initial_step;
    }

    int next = std::max(0, separation + step);
    if (delay_line_feedback_.max_separation > 0 &&
        next > delay_line_feedback_.max_separation) {
      next = delay_line_feedback_.max_separation;
      if (separation == delay_line_feedback_.max_separation) {
        LOG(warning) << "  delay line '" << request.name_prefix
                     << "' is at its cap of " << next
                     << " and timing is still short by " << deficit
                     << " ps. The residual needs inserted elements, not more "
                        "wire.\n";
      }
    }

    int area_limit = baseline.area_limit;
    DelayLineChain chain;
    std::string chain_error;
    if (BuildDelayLineChain(circuit_, request.name_prefix, &chain,
                            &chain_error)) {
      const Macro* macro =
          circuit_.Components()[chain.nodes.front().component_id].MacroPtr();
      area_limit = MaxDelayLineSeparation(chain, macro);
      const int requested_next = next;
      next = ClampDelayLineSeparation(chain, macro, next,
                                      "timing-feedback");
      if (next == separation && slack < delay_line_feedback_.margin &&
          requested_next >= area_limit && separation >= area_limit) {
        LOG(warning) << "  delay line '" << request.name_prefix
                     << "' is at its area bound and timing is still short by "
                     << (delay_line_feedback_.margin - slack)
                     << " ps. The residual needs inserted elements or more "
                        "folds.\n";
      }
    }

    // Separation exhausted and still short: escalate the pitch instead. A row
    // that buys nothing will not buy anything at the next iteration either, so
    // continuing to request separation just reports the same deficit forever.
    // The threshold is on measured gain rather than on separation alone,
    // because a line pinned at the cap that is still gaining is converging and
    // should be left to converge.
    const bool separation_exhausted =
        next == separation && slack < delay_line_feedback_.margin &&
        delay_line_feedback_.max_separation > 0 &&
        separation >= delay_line_feedback_.max_separation;
    if (separation_exhausted && gain_per_row < kStalledGainPsPerRow &&
        request.column_pitch < kMaxColumnPitch) {
      ++request.column_pitch;
      LOG(info) << "  delay line '" << request.name_prefix
                << "' separation is capped at " << separation
                << " and gaining only " << gain_per_row
                << " ps/row; widening column pitch to " << request.column_pitch
                << "\n";
      request.gain_per_row = 0.0;
      changed = true;
    }

    // Stride is the mechanism; separation is not allowed to paper over its
    // shortfall. The deficit is reported at the end of placement instead.
    const int proposed = next;
    next = ApplySeparationEscalationGate(separation, proposed,
                                         delay_line_separation_escalation_);

    // Logged after the escalation gate, so `new_separation` is what the request
    // carries away rather than what was merely computed. The proposal is kept
    // beside it because a suppressed increase is the evidence that escalation
    // is off, and losing it leaves a silently inert controller.
    LOG(info) << "DELAY_LINE_FEEDBACK_EVENT iteration " << iteration
              << " line " << request.name_prefix << " constraint_ids";
    for (int constraint_id : request.matched_constraint_ids)
      LOG(info) << " " << constraint_id;
    LOG(info) << " binding_slack_ps " << slack << " old_separation "
              << separation << " proposed_separation " << proposed
              << " new_separation " << next << " escalation_enabled "
              << (delay_line_separation_escalation_ ? 1 : 0)
              << " gain_source " << gain_source
              << " measured_gain_ps_per_row " << gain_per_row
              << " shape_footprint_separation " << proposed
              << " shape_area_limit " << area_limit
              << " shape_next_separation " << next << "\n";
    if (next == separation) continue;
    pending_delay_line_feedback_events_.push_back(DelayLineFeedbackEvent{
        iteration, request.name_prefix, request.matched_constraint_ids, slack,
        separation, proposed, next, gain_per_row,
        delay_line_separation_escalation_});
    request.separation = next;
    changed = true;
  }

  // Every accepted shape is imposed together, after every site was measured
  // against the same baseline. Applying one before the next was probed is what
  // would make site B's measurement depend on site A's decision.
  if (changed) *shapes = BuildDelayLineShapes();
  return changed;
}

std::vector<GlobalPlacer::DelayLineShape> Dali::BuildDelayLineShapes() {
  std::vector<GlobalPlacer::DelayLineShape> shapes;
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    // A registered-but-unshaped line is placed by the ordinary objective.
    if (!request.shaped) continue;
    DelayLineChain chain;
    std::string error_message;
    if (!BuildDelayLineChain(circuit_, request.name_prefix, &chain,
                             &error_message)) {
      LOG(warning) << "Not holding '" << request.name_prefix
                   << "' spread during placement: " << error_message << "\n";
      continue;
    }
    Macro *macro =
        circuit_.Components()[chain.nodes.front().component_id].MacroPtr();
    const int separation =
        ClampDelayLineSeparation(chain, macro, request.separation,
                                 "shape-build");
    const int signed_separation =
        ShouldExtendDelayLineDownward(chain, macro) ? -separation : separation;
    std::vector<DetourTarget> targets = BuildInterleavedRowBandTargets(
        chain, signed_separation, macro->Height(),
        macro->Width() * std::max(1, request.column_pitch),
        request.column_stride);

    GlobalPlacer::DelayLineShape shape;
    for (size_t index = 0; index < chain.nodes.size(); ++index) {
      shape.component_ids.push_back(chain.nodes[index].component_id);
      shape.offset_x.push_back(targets[index].x - chain.nodes.front().x);
      shape.offset_y.push_back(targets[index].y - chain.nodes.front().y);
    }
    shapes.push_back(std::move(shape));
  }
  return shapes;
}

std::vector<PlacementDelayLineVisualization>
Dali::BuildDelayLineVisualization() {
  std::vector<PlacementDelayLineVisualization> visualizations;
  visualizations.reserve(delay_line_spread_requests_.size());
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    DelayLineChain chain;
    std::string error_message;
    if (!BuildDelayLineChain(circuit_, request.name_prefix, &chain,
                             &error_message)) {
      LOG(warning) << "Cannot visualize '" << request.name_prefix
                   << "': " << error_message << "\n";
      continue;
    }
    PlacementDelayLineVisualization visualization;
    visualization.name = chain.name;
    visualization.component_ids.reserve(chain.nodes.size());
    if (chain.nodes.size() > 1) {
      visualization.component_edges.reserve(chain.nodes.size() - 1);
    }
    for (std::size_t index = 0; index < chain.nodes.size(); ++index) {
      visualization.component_ids.push_back(chain.nodes[index].component_id);
      if (index != 0) {
        visualization.component_edges.emplace_back(
            chain.nodes[index - 1].component_id, chain.nodes[index].component_id);
      }
    }
    visualizations.push_back(std::move(visualization));
  }
  return visualizations;
}

/**
 * Declare the next pipeline stage, as the name prefixes its cells share.
 *
 * Bands stack in declaration order from the bottom of the placement region, so
 * the recipe declares stages in the direction the pipeline flows. Prefixes
 * match on a token boundary, the same rule delay-line attribution uses, so
 * `lat1` selects `lat1[0]`..`lat1[63]` without also selecting `lat10`.
 *
 * A delay line is normally left out of its stage's band: its vertical extent is
 * the latency mechanism, and confining it to one stage's share of the region
 * would cap the separation the timing controller can ask for.
 */
bool Dali::DeclareStageBand(const std::vector<std::string> &name_prefixes) {
  if (name_prefixes.empty()) {
    LOG(error) << "A stage band needs at least one name prefix.\n";
    return false;
  }

  InitializeCircuitFromPhyDBIfNeeded();
  std::vector<Component> &components = circuit_.Components();

  StageBandRequest request;
  request.name_prefixes = name_prefixes;
  for (const std::string &prefix : name_prefixes) {
    int matched = 0;
    for (int index = 0; index < static_cast<int>(components.size()); ++index) {
      if (delay_line_feedback_internal::HasTokenBoundaryPrefix(
              prefix, components[index].Name())) {
        request.component_ids.push_back(index);
        ++matched;
      }
    }
    if (matched == 0) {
      LOG(error) << "Stage band prefix '" << prefix
                 << "' matches no component.\n";
      return false;
    }
  }

  LOG(info) << "Stage band " << stage_band_requests_.size() << ": "
            << request.component_ids.size() << " cells from "
            << name_prefixes.size() << " prefix(es)\n";
  stage_band_requests_.push_back(std::move(request));
  return true;
}

/** Hand the declared stage bands to the global placer, bottom-up. */
void Dali::ConfigureStageBands() {
  if (stage_band_requests_.empty()) return;
  std::vector<GlobalPlacer::StageBand> bands;
  bands.reserve(stage_band_requests_.size());
  for (const StageBandRequest &request : stage_band_requests_) {
    bands.push_back({request.component_ids});
  }
  LOG(info) << "Holding " << bands.size()
            << " pipeline stages in horizontal bands through global "
               "placement\n";
  gb_placer_.SetStageBands(std::move(bands));
}

void Dali::ConfigureDelayLineShapes() {
  std::vector<GlobalPlacer::DelayLineShape> shapes = BuildDelayLineShapes();
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    LOG(info) << "Holding '" << request.name_prefix
              << "' spread through global placement, separation "
              << request.separation << "\n";
  }
  gb_placer_.SetDelayLineShapes(std::move(shapes));

  if (delay_line_feedback_.enabled && !delay_line_spread_requests_.empty()) {
    gb_placer_.SetDelayLineFeedback(
        [this](int iteration,
               std::vector<GlobalPlacer::DelayLineShape> *updated) {
          return RetuneDelayLineSeparation(iteration, updated);
        },
        delay_line_feedback_.warmup, delay_line_feedback_.interval,
        delay_line_feedback_.freeze);
    gb_placer_.SetDelayLineFeedbackAppliedCallback(
        [this](int iteration) { PublishDelayLineFeedbackSnapshots(iteration); });
  }
}

void Dali::ReportPerformance() {
  if (!phy_db_ptr_->GetTimingApi().ReadyForTimingDriven())
    return;
}

bool Dali::TimingDrivenPlacement(double density, int number_of_threads) {
  bool is_success = true;
  InitializeTimingDrivenPlacement();
  for (int i = 0; i < max_td_place_num_; ++i) {
    GlobalPlace(density, number_of_threads);
    is_success = UnifiedLegalization();
    ExportOrdinaryComponentsToPhyDB();
    UpdateRCs();
    PerformTimingAnalysis();
    UpdateNetWeights();
  }
  ReportPerformance();
  return is_success;
}

#else

bool Dali::ReportTiming() {
  LOG(error) << "Timing analysis is unavailable because Dali was built "
                "without GaloisEDA support.\n";
  return false;
}

bool Dali::WriteTimingRepairPlan(const std::string &file_name) {
  (void)file_name;
  LOG(error) << "Timing repair planning is unavailable because Dali was built "
                "without GaloisEDA support.\n";
  return false;
}

bool Dali::WriteCurrentTimingDecomposition(
    const std::string &, const std::vector<std::string> &) {
  LOG(error) << "Timing decomposition is unavailable because Dali was built "
                "without GaloisEDA support.\n";
  return false;
}

bool Dali::CheckTiming() {
  LOG(error) << "Timing signoff is unavailable because Dali was built without "
                "GaloisEDA support.\n";
  return false;
}

bool Dali::WeightCriticalCycleNets(double multiplier) {
  (void)multiplier;
  LOG(error) << "Critical-cycle net weighting is unavailable because Dali was "
                "built without GaloisEDA support.\n";
  return false;
}

// The entry points a recipe can name, and the two the placement stages call.
// All of them steer delay-line geometry from measured slack, so without a
// timing host there is nothing for them to measure and nothing to steer. Each
// refuses in the same shape as the timing entry points above rather than being
// compiled away: the command processor dispatches on the recipe's text, so a
// missing definition is a link error and a silent no-op would be a recipe that
// appears to work.

bool Dali::WriteTimingConstraintIdentities(
    const std::string &file_name,
    const std::vector<std::string> &replaceable_site_prefixes) {
  (void)file_name;
  (void)replaceable_site_prefixes;
  LOG(error) << "Timing constraint identities are unavailable because Dali was "
                "built without GaloisEDA support.\n";
  return false;
}

bool Dali::WriteCurrentTimingConstraintIdentities(
    const std::string &, const std::vector<std::string> &,
    unsigned long long *, std::size_t *) {
  LOG(error) << "Timing constraint identities are unavailable because Dali was "
                "built without GaloisEDA support.\n";
  return false;
}

bool Dali::WriteCurrentTimingConstraintEndpointIdentities(
    const std::string &, const std::vector<std::string> &,
    unsigned long long *, std::size_t *) {
  LOG(error) << "Timing constraint endpoint identities are unavailable because "
                "Dali was built without GaloisEDA support.\n";
  return false;
}

bool Dali::WeightFastPathNets(double multiplier) {
  (void)multiplier;
  LOG(error) << "Fast-path net weighting is unavailable because Dali was built "
                "without GaloisEDA support.\n";
  return false;
}

bool Dali::DetourDelayLine(const std::string &name_prefix, double amplitude) {
  (void)name_prefix;
  (void)amplitude;
  LOG(error) << "Delay-line detouring is unavailable because Dali was built "
                "without GaloisEDA support.\n";
  return false;
}

bool Dali::SpreadDelayLineAcrossRows(const std::string &name_prefix,
                                     int separation, int column_stride,
                                     int column_pitch) {
  (void)name_prefix;
  (void)separation;
  (void)column_stride;
  (void)column_pitch;
  LOG(error) << "Delay-line spreading is unavailable because Dali was built "
                "without GaloisEDA support.\n";
  return false;
}

bool Dali::RegisterDelayLine(const std::string &name_prefix) {
  (void)name_prefix;
  LOG(error) << "Delay-line registration is unavailable because Dali was built "
                "without GaloisEDA support.\n";
  return false;
}

bool Dali::DeclareStageBand(const std::vector<std::string> &name_prefixes) {
  (void)name_prefixes;
  LOG(error) << "Stage-band declaration is unavailable because Dali was built "
                "without GaloisEDA support.\n";
  return false;
}

bool Dali::CloseTimingWithDelayLineSpread(double margin, int initial_step,
                                          int max_rounds) {
  (void)margin;
  (void)initial_step;
  (void)max_rounds;
  LOG(error) << "Delay-line closure is unavailable because Dali was built "
                "without GaloisEDA support.\n";
  return false;
}

void Dali::EnableDelayLineTimingFeedback(double margin, int initial_step,
                                         int warmup, int interval, int freeze,
                                         double damping, int max_separation) {
  (void)margin;
  (void)initial_step;
  (void)warmup;
  (void)interval;
  (void)freeze;
  (void)damping;
  (void)max_separation;
  LOG(error) << "Delay-line timing feedback is unavailable because Dali was "
                "built without GaloisEDA support.\n";
}

// Called unconditionally by the placement stages and the snapshot writer, so
// these return an empty result rather than refusing: a build with no timing
// host has no registered delay lines to describe or shape, which is a normal
// state for it and not an error to report on every snapshot.
std::vector<PlacementDelayLineVisualization>
Dali::BuildDelayLineVisualization() {
  return {};
}

void Dali::ConfigureDelayLineShapes() {}

void Dali::ConfigureStageBands() {}

#endif

bool Dali::ReportRuntimeBreakdown() const {
  auto emit = [](const char *name, double wall_seconds, int count) {
    std::ostringstream formatted_seconds;
    formatted_seconds << std::setprecision(12) << wall_seconds;
    LOG(info) << "RUNTIME_BREAKDOWN phase " << name << " wall_seconds "
              << formatted_seconds.str() << " count " << count << "\n";
  };
  emit("global_placement", runtime_breakdown_.global_placement_wall_seconds,
       runtime_breakdown_.global_placement_runs);
  emit("legalization", runtime_breakdown_.legalization_wall_seconds,
       runtime_breakdown_.legalization_runs);
  emit("io_placement", runtime_breakdown_.io_placement_wall_seconds,
       runtime_breakdown_.io_placement_runs);
  emit("filler_placement", runtime_breakdown_.filler_placement_wall_seconds,
       runtime_breakdown_.filler_placement_runs);
  emit("topology_host", runtime_breakdown_.topology_host_wall_seconds,
       runtime_breakdown_.topology_host_calls);
  emit("topology_bookkeeping",
       runtime_breakdown_.topology_bookkeeping_wall_seconds,
       runtime_breakdown_.topology_host_calls);
  emit("timing_initialize", runtime_breakdown_.timing_initialize_wall_seconds,
       runtime_breakdown_.timing_refreshes);
  emit("timing_export_locations",
       runtime_breakdown_.timing_export_locations_wall_seconds,
       runtime_breakdown_.timing_refreshes);
  emit("timing_update_rc", runtime_breakdown_.timing_update_rc_wall_seconds,
       runtime_breakdown_.timing_refreshes);
  emit("timing_analysis", runtime_breakdown_.timing_analysis_wall_seconds,
       runtime_breakdown_.timing_refreshes);
  emit("timing_witness_capture",
       runtime_breakdown_.timing_witness_capture_wall_seconds,
       runtime_breakdown_.timing_refreshes);
  emit("timing_other", runtime_breakdown_.timing_other_wall_seconds,
       runtime_breakdown_.timing_refreshes);
  const GlobalPlacer::RuntimeBreakdown &global =
      gb_placer_.GetRuntimeBreakdown();
  auto emit_global = [](const char *name, double wall_seconds, int count) {
    std::ostringstream formatted_seconds;
    formatted_seconds << std::setprecision(12) << wall_seconds;
    LOG(info) << "RUNTIME_GLOBAL_SUBPHASE phase " << name
              << " wall_seconds " << formatted_seconds.str() << " count "
              << count << "\n";
  };
  emit_global("optimizer", global.optimizer_wall_seconds, global.iterations);
  emit_global("spreader", global.spreader_wall_seconds, global.iterations);
  emit_global("delay_line_shapes", global.shape_wall_seconds,
              global.iterations);
  emit_global("physical_refinement", global.physical_refinement_wall_seconds,
              global.physical_refinements);
  emit_global("timing_observers", global.timing_observer_wall_seconds,
              global.timing_observer_calls);
  emit_global("anchor_feedback", global.anchor_feedback_wall_seconds,
              global.anchor_feedbacks);
  emit_global("other", global.other_wall_seconds, global.iterations);
  std::ostringstream formatted_scope;
  formatted_scope << std::setprecision(12)
                  << runtime_breakdown_.placement_scope_wall_seconds;
  LOG(info) << "RUNTIME_SCOPE phase dali_placement wall_seconds "
            << formatted_scope.str() << " count "
            << runtime_breakdown_.placement_runs << "\n";
  return true;
}

/**
 * Coordinate capture and restore for one delay line.
 *
 * Deliberately outside the timing-host guard: both touch only component
 * coordinates and the chain the name prefix resolves to, so they work in any
 * configuration, and the tests that exercise them are ordinary placement tests.
 * They previously sat inside the guard by position rather than by need, which
 * compiled everywhere and failed to link the no-Galois test binary.
 */
std::vector<ComponentPlacement> Dali::SnapshotDelayLinePlacement(
    const std::string &name_prefix) {
  std::vector<ComponentPlacement> placements;
  DelayLineChain chain;
  std::string error_message;
  if (!BuildDelayLineChain(circuit_, name_prefix, &chain, &error_message)) {
    return placements;
  }
  const std::vector<Component> &components = circuit_.Components();
  placements.reserve(chain.nodes.size());
  for (const DelayLineNode &node : chain.nodes) {
    const Component &component = components[node.component_id];
    placements.push_back({node.component_id, component.LLX(), component.LLY(),
                          static_cast<int>(component.Status())});
  }
  return placements;
}

void Dali::RestoreDelayLinePlacement(
    const std::vector<ComponentPlacement> &placements) {
  std::vector<Component> &components = circuit_.Components();
  for (const ComponentPlacement &placement : placements) {
    if (placement.component_id < 0 ||
        placement.component_id >= static_cast<int>(components.size())) {
      continue;
    }
    Component &component = components[placement.component_id];
    component.SetLLX(placement.llx);
    component.SetLLY(placement.lly);
    component.SetPlacementStatus(static_cast<PlaceStatus>(placement.status));
  }
}


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
  if (!is_circuit_initialized_) {
    circuit_.SetEnableShrinkOffGridDieArea(enable_shrink_off_grid_die_area_);
    circuit_.InitializeFromPhyDB(phy_db_ptr_);
    ApplyDebugPlacementRegionScale();
    is_circuit_initialized_ = true;
  }
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
  if (debug_placement_region_scale_ == 1.0)
    return;

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

/**
 * Run global placement, configured for the current flow.
 *
 * Installs the gridded rough-legalization refiner and capacity model when the
 * flow calls for them, then runs the placement. Returns false if it did not
 * complete.
 */
bool Dali::RunGlobalPlacementStage() {
  ElapsedTime stage_timer;
  stage_timer.RecordStartTime();
  if (io_placer_ != nullptr && io_placer_->HasEdgeConstraints()) {
    if (!io_placer_->SetGlobalMetalLayer(io_metal_layer_) ||
        !io_placer_->PlaceConstrainedPinsForGlobalPlacement()) {
      LOG(error)
          << "Cannot anchor constrained I/O pins before global placement\n";
      return false;
    }
  }
  gb_placer_.SetCircuit(&circuit_);
  gb_placer_.SetNumThreads(num_threads_);
  // Later stages inherit the density through CopyPlacementContextFrom, which
  // reads it off the global placer. Setting it only where global placement runs
  // leaves legalization with no density at all, so any flow that skips global
  // placement aborts inside the gridded capacity estimator.
  gb_placer_.SetPlacementDensity(target_density_);
  ConfigureStageBands();
  ConfigureDelayLineShapes();
  gb_placer_.SetSnapshotCallback(
      [this](const std::string &id, const std::string &label,
             const std::string &subgroup, int iteration) {
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
    // The checkpoint policy is Dali's: this decides when a checkpoint happens,
    // and a registered host only says what changed. With no host, or with the
    // default schedule, nothing is ever taken and the boundary is still
    // exercised, so the state it would hand out is known to be reachable before
    // anything is allowed through it.
    // Scoped for the same reason the checkpoint observer is: the failed
    // placement below returns early, and an observer left installed would
    // still be firing on the next run.
    struct ScopedAcceptedPhysicalObserver {
      GlobalPlacer &placer;
      explicit ScopedAcceptedPhysicalObserver(GlobalPlacer &p) : placer(p) {}
      ~ScopedAcceptedPhysicalObserver() {
        placer.SetAcceptedPhysicalObserver(nullptr);
      }
    } scoped_physical_observer(gb_placer_);
    struct ScopedPostFeedbackObserver {
      GlobalPlacer &placer;
      explicit ScopedPostFeedbackObserver(GlobalPlacer &p) : placer(p) {}
      ~ScopedPostFeedbackObserver() { placer.SetPostFeedbackObserver(nullptr); }
    } scoped_post_feedback_observer(gb_placer_);
    if (timing_domain_observation_ && timing_observe_global_iterations_) {
      gb_placer_.SetAcceptedPhysicalObserver([this](int iteration) {
        ObserveTimingDomain("accepted_physical", iteration);
      });
      gb_placer_.SetPostFeedbackObserver([this](int iteration) {
        ObserveTimingDomain("post_feedback", iteration);
      });
    }
    CheckpointEligibilityConfig eligibility = checkpoint_eligibility_;
    eligibility.warmup_iteration = topology_checkpoint_warmup_;
    DaliCheckpointCoordination checkpoint_observer(this, eligibility,
                                                   topology_checkpoint_max_);
    {
      // Scoped, because the observer is a local of this frame and the placer
      // holds it as a bare pointer: the early return below would otherwise
      // leave a dangling one behind.
      ScopedCheckpointObserver installed(gb_placer_, &checkpoint_observer);
      if (!gb_placer_.StartPlacement()) {
        LOG(error) << "Global placement failed\n";
        return false;
      }
    }
    ObserveTimingDomain("restored_best_upper_bound", -1);
    LOG(info) << "  Topology checkpoints offered: " << checkpoint_observer.Count()
              << ", requested: " << checkpoint_observer.Requests()
              << ", applied: " << gb_placer_.CheckpointRestarts() << "\n";
  }
  LogDelayLineStageSpans("after_global_placement");
  // The state a two-epoch architecture would size from: everything global
  // placement will ever produce, with nothing legalized yet.
  ObserveTimingDomain("end_of_global_placement", -1);
  WriteVisualizationSnapshot("global_placement.final", "After Global Placement",
                             "global_placement");
  stage_timer.RecordEndTime();
  runtime_breakdown_.global_placement_wall_seconds +=
      stage_timer.GetWallTime();
  ++runtime_breakdown_.global_placement_runs;
  RecordPlacementMetric("time.global_placement.wall_s",
                        stage_timer.GetWallTime());
  RecordPlacementMetric("time.global_placement.cpu_s",
                        stage_timer.GetCpuTime());
  return true;
}

/** Run standard-cell legalization, falling back to Tetris on failure.
 * @return true on success. */
bool Dali::RunStandardCellLegalization() {
  if (!ShouldRunMovableCellLegalization()) {
    LOG(info) << "Skip standard-cell legalization: no movable components\n";
    return true;
  }
  WriteVisualizationSnapshot("legalization.start", "Before Legalization",
                             "legalization");
  FlushVisualizationEvents();
  Placer *legalizer_for_detailed_placement = &standard_cell_legalizer_;
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
  ObserveTimingDomain("after_detailed_placement", -1);
  return true;
}

/** Run detailed placement for the current flow.
 * @return true on success. */
bool Dali::RunDetailedPlacement() {
  if (disable_detailed_place_) {
    LOG(info) << "Skip detailed placement: disabled by configuration\n";
    return true;
  }
  detailed_placer_.SetSnapshotCallback(
      [this](const std::string &id, const std::string &label,
             const std::string &subgroup, int iteration) {
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

/**
 * Copy the resolved options onto the gridded well legalizer before it runs.
 *
 * Groups every legalizer setting in one place so the enabling flags and their
 * effect on the legalizer are visible together rather than scattered.
 */
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
      [this](const std::string &id, const std::string &label,
             const std::string &group, const std::string &subgroup,
             int iteration) {
        std::vector<PlacementWellRect> well_rects;
        if (ShouldVisualizeWellRects(group, subgroup)) {
          well_rects = well_legalizer_.CollectWellVisualizationRects();
        }
        WriteVisualizationSnapshot(group + "." + id, label, group, subgroup,
                                   iteration, std::move(well_rects));
        FlushVisualizationEvents();
      });
  well_legalizer_.SetPlacementStageCallback(
      [this](const std::string& stage) { LogDelayLineStageSpans(stage); });
}

std::vector<DelayRepairSite> Dali::EffectiveDelayRepairSites(
    const std::vector<std::string> &additional_prefixes) const {
  std::vector<DelayRepairSite> sites = delay_repair_sites_;
  std::set<std::string> ids;
  std::set<std::string> logical_prefixes;
  for (const DelayRepairSite &site : sites) {
    ids.insert(site.id);
    logical_prefixes.insert(site.logical_path_prefix.empty()
                                ? site.instance_name
                                : site.logical_path_prefix);
  }
  std::vector<std::string> prefixes = additional_prefixes;
  prefixes.reserve(prefixes.size() + delay_line_spread_requests_.size());
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    prefixes.push_back(request.name_prefix);
  }
  for (const std::string &prefix : prefixes) {
    if (prefix.empty() || ids.count(prefix) != 0 ||
        logical_prefixes.count(prefix) != 0) {
      continue;
    }
    ids.insert(prefix);
    logical_prefixes.insert(prefix);
    DelayRepairSite site;
    site.id = prefix;
    site.instance_name = prefix;
    site.logical_path_prefix = prefix;
    site.kind = "delay_line";
    site.adjustable = true;
    sites.push_back(std::move(site));
  }
  return sites;
}

void Dali::ReportDelayLineClosure(bool timing_is_current) {
#if PHYDB_USE_GALOIS
  if (delay_line_spread_requests_.empty()) return;
  if (!timing_is_current && !ReportTiming()) return;
  if (!AttributeDelayLineRequests(&delay_line_spread_requests_,
                                  last_timing_snapshot_))
    return;

  int closed = 0;
  double total_deficit = 0.0;
  const double margin = delay_line_feedback_.enabled
                            ? delay_line_feedback_.margin
                            : 0.0;
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    DelayLineChain chain;
    std::string error_message;
    if (!BuildDelayLineChain(circuit_, request.name_prefix, &chain,
                             &error_message)) {
      continue;
    }
    const int elements = static_cast<int>(chain.nodes.size());
    const int columns = (elements + 1) / 2;
    const int stride = EffectiveColumnStride(columns, request.column_stride);
    const double deficit = margin - request.binding_slack;
    const bool line_closed = deficit <= 0.0;
    if (line_closed) {
      ++closed;
    } else {
      total_deficit += deficit;
    }
    // Where the line's cells actually are, and how many of them the flow still
    // considers unplaced. ApplyTopologyDelta creates added components UNPLACED
    // and nothing promotes them, so this is the only place that distinguishes
    // "seeded and legalized" from "sitting on the seed with a stale status".
    {
      int unplaced = 0;
      double min_x = 0.0, min_y = 0.0, max_x = 0.0, max_y = 0.0;
      bool first = true;
      std::map<std::pair<long long, long long>, int> occupancy;
      for (const Component &component : circuit_.Components()) {
        if (component.Name().compare(0, request.name_prefix.size(),
                                     request.name_prefix) != 0) {
          continue;
        }
        if (component.Status() == UNPLACED) ++unplaced;
        const double x = component.LLX(), y = component.LLY();
        if (first) {
          min_x = max_x = x;
          min_y = max_y = y;
          first = false;
        } else {
          min_x = std::min(min_x, x); max_x = std::max(max_x, x);
          min_y = std::min(min_y, y); max_y = std::max(max_y, y);
        }
        ++occupancy[{static_cast<long long>(x * 1000.0),
                     static_cast<long long>(y * 1000.0)}];
      }
      int shared = 0;
      for (const auto &entry : occupancy) {
        if (entry.second > 1) shared += entry.second;
      }
      LOG(info) << "DELAY_LINE_PLACEMENT line " << request.name_prefix
                << " cells " << (elements) << " unplaced " << unplaced
                << " distinct_sites " << occupancy.size() << " cells_sharing_a_site "
                << shared << " bbox " << min_x << "," << min_y << " -> " << max_x
                << "," << max_y << "\n";
    }
    LOG(info) << "DELAY_LINE_CLOSURE line " << request.name_prefix
              << " elements " << elements << " columns " << columns
              << " stride " << stride << " separation " << request.separation
              << " binding_slack_ps " << request.binding_slack
              << " deficit_ps " << (line_closed ? 0.0 : deficit) << " status "
              << (line_closed ? "CLOSED" : "SHORT") << "\n";
  }
  LOG(info) << "DELAY_LINE_CLOSURE_SUMMARY closed " << closed << " of "
            << delay_line_spread_requests_.size() << " lines, residual deficit "
            << total_deficit << " ps\n";
  if (closed < static_cast<int>(delay_line_spread_requests_.size())) {
    LOG(info) << "  The short lines have no geometry left to spend. Closing "
                 "them needs inserted elements sized to the deficit above.\n";
  }
#else
  // Without the timing host there is no slack to attribute, so a closure
  // report has nothing to say. The constraint attribution this reads lives in
  // the PHYDB_USE_GALOIS branch of this file.
#endif
}

void Dali::LogDelayLineStageSpans(const std::string& stage) {
  for (const DelayLineSpreadRequest& request : delay_line_spread_requests_) {
    DelayLineChain chain;
    std::string error_message;
    if (!BuildDelayLineChain(circuit_, request.name_prefix, &chain,
                             &error_message)) {
      LOG(error) << "DELAY_LINE_STAGE " << stage << " " << request.name_prefix
                 << " attribution_error " << error_message << "\n";
      continue;
    }
    double min_y = std::numeric_limits<double>::max();
    double max_y = std::numeric_limits<double>::lowest();
    for (const DelayLineNode& node : chain.nodes) {
      const Component& component = circuit_.Components()[node.component_id];
      min_y = std::min(min_y, component.LLY());
      max_y = std::max(max_y, component.URY());
    }
    LOG(info) << "DELAY_LINE_STAGE " << stage << " " << request.name_prefix
              << " span_um " << (max_y - min_y) * circuit_.GridValueY()
              << " separation " << request.separation << "\n";
  }
}

void Dali::RunFixedOnlyWellCompletion() {
  LOG(info) << "Skip movable-cell well legalization: no movable components\n";
  well_legalizer_.InitializeWellLegalizer();
  well_legalizer_.RunPhysicalCompletionStages();
}

bool Dali::ReserveDelayLineComponentsForLegalization() {
  // Only shaped lines are reserved. A reservation exists to carry an imposed
  // geometry through legalization; a line the placer arranged has no such
  // geometry, and reserving its scattered cells as one block asks the legalizer
  // to protect a region that spans much of the die.
  std::vector<DelayLineReservationRequest> reservation_requests;
  reservation_requests.reserve(delay_line_spread_requests_.size());
  for (const DelayLineSpreadRequest& request : delay_line_spread_requests_) {
    if (!request.shaped) continue;
    DelayLineChain chain;
    std::string error_message;
    if (!BuildDelayLineChain(circuit_, request.name_prefix, &chain,
                             &error_message)) {
      LOG(error) << "Cannot reserve '" << request.name_prefix
                 << "' for legalization: " << error_message << "\n";
      return false;
    }
    DelayLineReservationRequest reservation_request;
    reservation_request.name = request.name_prefix;
    reservation_request.component_ids.reserve(chain.nodes.size());
    for (const DelayLineNode& node : chain.nodes)
      reservation_request.component_ids.push_back(node.component_id);
    reservation_requests.push_back(std::move(reservation_request));
  }

  // Measured before planning and independent of it, so a planner that rejects
  // a pair can be checked against whether those two lines share any site at
  // all. A folded line occupies two rows and spans its whole separation, so a
  // box test and a cell test can disagree by the entire span.
  const DelayLineOccupancyReport occupancy =
      MeasureDelayLineOccupancy(circuit_, reservation_requests);
  for (const DelayLineOccupancy& line : occupancy.lines) {
    LOG(info) << "DELAY_LINE_RESERVATION_DIAG line " << line.name
              << " components " << line.component_count << " occupied_sites "
              << line.occupied_sites << " duplicate_sites "
              << line.duplicate_sites << " bbox " << line.llx << "," << line.lly
              << " -> " << line.urx << "," << line.ury << " rows_occupied "
              << line.rows_occupied << " rows_spanned " << line.rows_spanned
              << " in_region " << (line.inside_region ? 1 : 0)
              << " has_non_movable " << (line.has_non_movable ? 1 : 0) << "\n";
  }
  for (const DelayLineConflict& conflict : occupancy.conflicts) {
    LOG(info) << "DELAY_LINE_RESERVATION_PAIR a " << conflict.left << " b "
              << conflict.right << " bbox_overlap "
              << (conflict.bounding_boxes_intersect ? 1 : 0) << " shared_sites "
              << conflict.shared_sites << "\n";
  }
  LOG(info) << "DELAY_LINE_RESERVATION_SUMMARY lines "
            << occupancy.lines.size() << " bbox_intersections "
            << occupancy.bounding_box_intersections << " cell_intersections "
            << occupancy.cell_intersections << " duplicate_sites "
            << occupancy.duplicate_sites << " obstacle_collisions "
            << occupancy.obstacle_collisions << "\n";

  const DelayLineReservationPlan plan =
      PlanDelayLineReservations(circuit_, reservation_requests);
  if (!plan.valid()) {
    LOG(error) << "Cannot reserve delay-line components for legalization: "
               << plan.error << "\n";
    return false;
  }

  delay_line_legalization_statuses_.clear();
  for (const DelayLineReservation& reservation : plan.lines) {
    for (const DelayLineReservationComponent& planned_component :
         reservation.components) {
      Component& component =
          circuit_.Components()[planned_component.component_id];
      if (!component.IsMovable()) continue;
      component.SetLLX(planned_component.llx);
      component.SetLLY(planned_component.lly);
      delay_line_legalization_statuses_.emplace_back(
          component.Id(), planned_component.original_status);
      component.SetPlacementStatus(FIXED);
    }
  }
  LOG(info) << "Reserved " << delay_line_legalization_statuses_.size()
            << " delay-line cells for gridded legalization\n";
  return true;
}

void Dali::RestoreDelayLineComponentStatuses() {
  for (const auto& entry : delay_line_legalization_statuses_) {
    circuit_.Components()[entry.first].SetPlacementStatus(entry.second);
  }
  delay_line_legalization_statuses_.clear();
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
  well_legalizer_.EmitDEFWellFile(output_name_, well_emit_mode_);
  return true;
}

bool Dali::RunLegalizationStage() {
  ElapsedTime stage_timer;
  stage_timer.RecordStartTime();
  if (!disable_legalization_) {
    if (is_standard_cell_) {
      if (!RunStandardCellLegalization()) {
        return false;
      }
    } else {
      if (!ReserveDelayLineComponentsForLegalization()) return false;
      const bool legal = RunWellLegalization();
      RestoreDelayLineComponentStatuses();
      if (!legal) return false;
    }
  }
  std::vector<PlacementWellRect> final_well_rects;
  if (!is_standard_cell_ && !disable_legalization_) {
    final_well_rects = well_legalizer_.CollectWellVisualizationRects();
  }
  WriteVisualizationSnapshot("legalization.final", "After Legalization",
                             "legalization", "", -1,
                             std::move(final_well_rects));
  stage_timer.RecordEndTime();
  runtime_breakdown_.legalization_wall_seconds += stage_timer.GetWallTime();
  ++runtime_breakdown_.legalization_runs;
  return true;
}

/**
 * One sizing decision and one topology change, between the stages.
 *
 * Every placement engine the global placer built is already closed by the time
 * this runs -- StartPlacement closes them on its way out -- so the netlist may
 * change here without corrupting an index that outlives it. That is the whole
 * reason the boundary is here rather than inside the loop, and it is asserted
 * rather than assumed.
 *
 * There is no resume: placement is finished. What follows is legalization and
 * physical completion, on the changed netlist.
 */
bool Dali::RunStageBoundaryTopologySizing() {
  if (!topology_stage_boundary_sizing_) return true;
  if (stage_boundary_attempted_) {
    LOG(error) << "STAGE_BOUNDARY_FAILED a second topology change was "
                  "requested; one run changes one site at most once\n";
    return false;
  }
  stage_boundary_attempted_ = true;

  if (gb_placer_.ArePlacementEnginesOpen()) {
    LOG(error) << "STAGE_BOUNDARY_FAILED placement engines are still open, so "
                  "a netlist change here would corrupt indices sized by the "
                  "topology\n";
    return false;
  }

  const size_t components_before = circuit_.Components().size();
  const size_t nets_before = circuit_.Nets().size();

  TopologyCheckpointContext context;
  // Not an iteration. -1 says so in every record this produces, so a sample
  // taken here can never be mistaken for one taken inside the loop.
  context.checkpoint_iteration = -1;
  context.resume_iteration = -1;
  context.component_count = components_before;
  context.net_count = nets_before;
  context.component_headroom = circuit_.Components().capacity() - components_before;
  context.net_headroom = circuit_.Nets().capacity() - nets_before;

  manifest_components_before_ = components_before;
  manifest_nets_before_ = nets_before;
  LOG(info) << "STAGE_BOUNDARY_SIZING components " << components_before
            << " nets " << nets_before << " component_headroom "
            << context.component_headroom << " net_headroom "
            << context.net_headroom << "\n";

  TopologyMutationResult result =
      DecideAndApplyTopologyChange(context, "stage_boundary_sizing");

  if (result.status == TopologyMutationStatus::kFailed) {
    LOG(error) << "STAGE_BOUNDARY_FAILED " << result.message << "\n";
    return false;
  }
  if (result.status == TopologyMutationStatus::kNoChange ||
      result.delta.IsEmpty()) {
    LOG(info) << "STAGE_BOUNDARY_NO_CHANGE components " << components_before
              << " nets " << nets_before << "\n";
    return true;
  }

  // Validated in full before the first component is added, so a rejected delta
  // leaves the circuit exactly as it was rather than half-changed.
  ElapsedTime integration_timer;
  integration_timer.RecordStartTime();
  std::string error_message;
  if (!ValidateTopologyDelta(circuit_, result.delta, &error_message)) {
    LOG(error) << "STAGE_BOUNDARY_FAILED rejected topology delta: "
               << error_message << "\n";
    return false;
  }
  ApplyTopologyDelta(circuit_, result.delta);
  ++topology_generation_;
  // The best upper-bound placement and the feedback checkpoint are sized by the
  // netlist that no longer exists. Placement is over, but leaving them behind
  // would leave state describing a component count nothing else agrees with.
  gb_placer_.ForgetTopologySizedCaches(components_before);
  integration_timer.RecordEndTime();
  runtime_breakdown_.topology_bookkeeping_wall_seconds +=
      integration_timer.GetWallTime();

  LOG(info) << "STAGE_BOUNDARY_APPLIED components " << components_before
            << " -> " << circuit_.Components().size() << " nets " << nets_before
            << " -> " << circuit_.Nets().size() << " added_components "
            << result.delta.added_components.size() << " added_nets "
            << result.delta.added_nets.size() << " retired_nets "
            << result.delta.retired_nets.size() << " rewired_nets "
            << result.delta.rewired_nets.size() << "\n";
  WriteVisualizationSnapshot(
      "topology_change.seeded", "After ACT refresh and local seeding",
      "topology_change", "seeded");
  FlushVisualizationEvents();
  return true;
}

/**
 * Learn each site's response from this placement instead of a static table.
 *
 * The first sample is already legal and has final I/O locations. Every request
 * then travels through the existing ACT-authoritative batch seam, after which
 * the changed design is legalized and measured before another decision exists.
 * No filler is created here; that irreversible stage runs once after closure.
 */
bool Dali::RunAdaptiveDelayLineSizingEpochs() {
#if PHYDB_USE_GALOIS
  if (!topology_adaptive_sizing_) return true;
  if (!topology_stage_boundary_sizing_ || topology_batch_sizing_ ||
      has_fixed_topology_request_ || !delay_line_characterization_.empty() ||
      !delay_line_response_characterization_.empty()) {
    LOG(error) << "ADAPTIVE_SIZING_FAILED adaptive sizing requires the stage "
                  "boundary and cannot be mixed with one-shot sizing inputs\n";
    return false;
  }
  if (topology_adaptive_probe_pairs_ < 1 ||
      topology_adaptive_max_step_pairs_ < 1 ||
      topology_adaptive_probe_pairs_ > topology_adaptive_max_step_pairs_ ||
      topology_adaptive_max_epochs_ < 1 ||
      topology_sizing_max_added_pairs_ < 1 ||
      topology_sizing_max_pairs_ < 1) {
    LOG(error) << "ADAPTIVE_SIZING_FAILED adaptive limits are incomplete\n";
    return false;
  }
  if (delay_line_spread_requests_.empty()) {
    LOG(error) << "ADAPTIVE_SIZING_FAILED no delay line is registered\n";
    return false;
  }

  adaptive_sizing_histories_.clear();
  adaptive_constraint_attribution_evidence_.clear();
  manifest_adaptive_epochs_.clear();
  manifest_boundary_sites_.clear();
  manifest_verdicts_.clear();
  manifest_requests_.clear();
  manifest_added_components_ = 0;
  manifest_added_nets_ = 0;
  manifest_retired_nets_ = 0;
  manifest_rewired_nets_ = 0;
  manifest_mutation_applied_ = false;
  manifest_components_before_ = circuit_.Components().size();
  manifest_nets_before_ = circuit_.Nets().size();

  std::vector<std::string> adaptive_site_prefixes;
  adaptive_site_prefixes.reserve(delay_line_spread_requests_.size());
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    adaptive_site_prefixes.push_back(request.name_prefix);
  }

  for (int epoch = 0; epoch <= topology_adaptive_max_epochs_; ++epoch) {
    const std::string stage = "adaptive_epoch_" + std::to_string(epoch);
    const int timing_generation_before = timing_generation_;
    ObserveTimingDomain(stage, -1);
    const bool capture_witnesses =
        timing_domain_observation_ ||
        adaptive_constraint_attribution_evidence_.empty();
    const bool timing_ready =
        timing_domain_observation_
            ? timing_generation_ == timing_generation_before + 1
            : RefreshTiming(capture_witnesses);
    if (!timing_ready) {
      LOG(error) << "ADAPTIVE_SIZING_FAILED epoch " << epoch
                 << " timing failed\n";
      return false;
    }
    if (!timing_domain_observation_) {
      CanonicalizeReplaceableSiteEndpoints(&last_timing_snapshot_,
                                           adaptive_site_prefixes);
      std::string attribution_error;
      if (capture_witnesses) {
        if (!CaptureConstraintAttributionEvidence(
                adaptive_site_prefixes,
                last_timing_snapshot_.relative_constraints,
                &adaptive_constraint_attribution_evidence_,
                &attribution_error)) {
          LOG(error) << "ADAPTIVE_SIZING_FAILED epoch " << epoch
                     << " could not freeze attribution: " << attribution_error
                     << "\n";
          return false;
        }
        LOG(info) << "ADAPTIVE_ATTRIBUTION_CACHED constraints "
                  << adaptive_constraint_attribution_evidence_.size() << "\n";
      } else if (!RestoreConstraintAttributionEvidence(
                     adaptive_constraint_attribution_evidence_,
                     &last_timing_snapshot_.relative_constraints,
                     &attribution_error)) {
        LOG(error) << "ADAPTIVE_SIZING_FAILED epoch " << epoch
                   << " could not reuse attribution: " << attribution_error
                   << "\n";
        return false;
      }
    }
    if (!AttributeDelayLineRequests(&delay_line_spread_requests_,
                                    last_timing_snapshot_)) {
      LOG(error) << "ADAPTIVE_SIZING_FAILED epoch " << epoch
                 << " attribution failed\n";
      return false;
    }

    std::vector<AdaptiveDelayLineSample> samples;
    for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
      DelayLineChain chain;
      std::string chain_error;
      if (!BuildDelayLineChain(circuit_, request.name_prefix, &chain,
                               &chain_error)) {
        LOG(error) << "ADAPTIVE_SIZING_FAILED site '" << request.name_prefix
                   << "' has no readable chain: " << chain_error << "\n";
        return false;
      }
      AdaptiveDelayLineSample sample;
      sample.site = request.name_prefix;
      sample.pairs = static_cast<int>(chain.nodes.size()) / 2;
      sample.binding_slack_ps = request.binding_slack;
      sample.attributed = request.has_binding_slack;
      sample.binding_identity =
          BindingConstraintIdentity(request.binding_constraint_id);
      samples.push_back(sample);

      auto history = std::find_if(
          adaptive_sizing_histories_.begin(), adaptive_sizing_histories_.end(),
          [&](const AdaptiveDelayLineHistory &candidate) {
            return candidate.site == sample.site;
          });
      if (history == adaptive_sizing_histories_.end()) {
        AdaptiveDelayLineHistory fresh;
        fresh.site = sample.site;
        fresh.initial_pairs = sample.pairs;
        adaptive_sizing_histories_.push_back(std::move(fresh));
        history = std::prev(adaptive_sizing_histories_.end());
      }
      history->samples.push_back(sample);
      LOG(info) << "ADAPTIVE_MEASURE epoch " << epoch << " site "
                << sample.site << " pairs " << sample.pairs << " slack_ps "
                << sample.binding_slack_ps << " binding_identity '"
                << sample.binding_identity << "'\n";
    }

    AdaptiveDelayLineLimits limits;
    limits.margin_ps = topology_sizing_margin_ps_;
    limits.probe_pairs = topology_adaptive_probe_pairs_;
    limits.max_step_pairs = topology_adaptive_max_step_pairs_;
    limits.max_added_pairs = topology_sizing_max_added_pairs_;
    limits.max_pairs = topology_sizing_max_pairs_;
    limits.max_nonpositive_probes =
        topology_adaptive_max_nonpositive_probes_;
    limits.component_headroom =
        circuit_.Components().capacity() - circuit_.Components().size();
    limits.net_headroom = circuit_.Nets().capacity() - circuit_.Nets().size();

    AdaptiveSizingDecision decision =
        DecideAdaptiveDelayLineSizing(adaptive_sizing_histories_, limits);
    AdaptiveEpochManifest epoch_manifest;
    epoch_manifest.epoch = epoch;
    epoch_manifest.outcome = decision.outcome;
    epoch_manifest.samples = samples;
    epoch_manifest.decisions = decision.sites;
    epoch_manifest.reason = decision.reason;

    if (decision.outcome == AdaptiveSizingOutcome::kClosed) {
      manifest_adaptive_epochs_.push_back(std::move(epoch_manifest));
      LOG(info) << "ADAPTIVE_CLOSED epoch " << epoch << " margin_ps "
                << topology_sizing_margin_ps_ << "\n";
      return true;
    }
    if (decision.outcome == AdaptiveSizingOutcome::kInvalid) {
      manifest_adaptive_epochs_.push_back(std::move(epoch_manifest));
      LOG(error) << "ADAPTIVE_SIZING_FAILED epoch " << epoch << ": "
                 << decision.reason << "\n";
      return false;
    }
    if (epoch == topology_adaptive_max_epochs_) {
      epoch_manifest.outcome = AdaptiveSizingOutcome::kInvalid;
      epoch_manifest.reason = "the adaptive epoch limit was reached";
      manifest_adaptive_epochs_.push_back(std::move(epoch_manifest));
      LOG(error) << "ADAPTIVE_SIZING_FAILED the epoch limit was reached\n";
      return false;
    }

    for (const AdaptiveSiteDecision &site : decision.sites) {
      LOG(info) << "ADAPTIVE_DECISION epoch " << epoch << " site "
                << site.site << " pairs " << site.current_pairs << " -> "
                << site.requested_pairs << " slack_ps "
                << site.binding_slack_ps << " gain_ps_per_pair "
                << site.measured_gain_ps_per_pair << " mode "
                << (site.is_probe ? "probe" : "predicted") << " reason "
                << site.reason << "\n";
    }

    const std::size_t components_before = circuit_.Components().size();
    const std::size_t nets_before = circuit_.Nets().size();
    TopologyCheckpointContext context;
    context.checkpoint_iteration = -1;
    context.resume_iteration = -1;
    context.component_count = components_before;
    context.net_count = nets_before;
    context.component_headroom =
        circuit_.Components().capacity() - components_before;
    context.net_headroom = circuit_.Nets().capacity() - nets_before;

    TopologyMutationResult result =
        ApplyTopologyChangeRequest(context, decision.batch);
    if (result.status != TopologyMutationStatus::kApplied ||
        result.delta.IsEmpty()) {
      LOG(error) << "ADAPTIVE_SIZING_FAILED epoch " << epoch
                 << " host did not apply the requested batch"
                 << (result.message.empty() ? "" : ": ") << result.message
                 << "\n";
      return false;
    }
    ElapsedTime integration_timer;
    integration_timer.RecordStartTime();
    std::string delta_error;
    if (!ValidateTopologyDelta(circuit_, result.delta, &delta_error)) {
      LOG(error) << "ADAPTIVE_SIZING_FAILED rejected topology delta: "
                 << delta_error << "\n";
      return false;
    }
    ApplyTopologyDelta(circuit_, result.delta);
    ++topology_generation_;
    gb_placer_.ForgetTopologySizedCaches(components_before);
    integration_timer.RecordEndTime();
    runtime_breakdown_.topology_bookkeeping_wall_seconds +=
        integration_timer.GetWallTime();
    manifest_mutation_applied_ = true;
    epoch_manifest.added_components =
        static_cast<int>(result.delta.added_components.size());
    epoch_manifest.added_nets =
        static_cast<int>(result.delta.added_nets.size());
    epoch_manifest.rewired_nets =
        static_cast<int>(result.delta.rewired_nets.size());
    manifest_adaptive_epochs_.push_back(std::move(epoch_manifest));

    WriteVisualizationSnapshot("adaptive." + std::to_string(epoch) + ".seeded",
                               "Adaptive insertion seeded", "topology_change",
                               "adaptive");
    FlushVisualizationEvents();
    if (!RunLegalizationStage() || !PromoteLegalizedTopologyComponents() ||
        !RunIoPinPlacementStage()) {
      LOG(error) << "ADAPTIVE_SIZING_FAILED epoch " << epoch
                 << " did not reach a legal timed state\n";
      return false;
    }
  }
  return false;
#else
  // Same first question the timing branch asks. Without it every placement in a
  // no-Galois build failed, including ordinary I/O flows that never requested
  // adaptive sizing: a feature nobody asked for was refusing the whole run.
  if (!topology_adaptive_sizing_) return true;
  LOG(error) << "Adaptive delay-line sizing requires Galois timing support\n";
  return false;
#endif
}

/**
 * Mark added cells placed, now that legalization has actually placed them.
 *
 * Only reached when legalization succeeded, so every one of these has a legal
 * site. A cell still sharing a site with another has not been legalized, and
 * saying so here is cheaper than discovering it from a DEF later.
 */
bool Dali::PromoteLegalizedTopologyComponents() {
  if (components_awaiting_legal_placement_.empty()) return true;
  std::string placement_error;
  if (!ValidateTopologyAddedPlacement(
          circuit_, components_awaiting_legal_placement_, &placement_error)) {
    LOG(error) << "TOPOLOGY_PROMOTION_FAILED " << placement_error << "\n";
    return false;
  }
  if (!is_standard_cell_) {
    const GriddedPlacementLegalityReport report = ValidatePlacementLegality();
    if (!report.IsLegal()) {
      LOG(error) << "TOPOLOGY_PROMOTION_FAILED whole gridded placement has "
                 << report.TotalViolationCount() << " violation(s)\n";
      return false;
    }
  }
  size_t promoted = 0;
  for (const std::string &name : components_awaiting_legal_placement_) {
    Component *component = circuit_.GetComponentPtr(name);
    component->SetPlacementStatus(PLACED);
    ++promoted;
  }
  LOG(info) << "TOPOLOGY_PROMOTED components " << promoted
            << " to placed after legalization\n";
  components_awaiting_legal_placement_.clear();
  return true;
}

bool Dali::RunCorePlacementStages() {
  if (!RunGlobalPlacementStage()) return false;
  if (!topology_adaptive_sizing_ && !RunStageBoundaryTopologySizing())
    return false;
  if (!RunLegalizationStage()) return false;
  if (!PromoteLegalizedTopologyComponents()) return false;
  ObserveTimingDomain("after_legalization", -1);
  // The canonical reference. Named separately from `after_legalization` even
  // where the two describe the same coordinates, because which stages run is a
  // property of the flow: this design legalizes wells and never reaches
  // detailed placement, and a state that is canonical here by coincidence must
  // not be assumed canonical elsewhere. The digests say which case holds.
  ObserveTimingDomain("final_placement", -1);
  return true;
}

bool Dali::RunFillerCellPlacement() {
  if (!enable_filler_cell_) {
    return true;
  }
  ElapsedTime stage_timer;
  stage_timer.RecordStartTime();
  filler_cell_placer_.CopyPlacementContextFrom(&gb_placer_);
  filler_cell_placer_.phy_db_ptr_ = phy_db_ptr_;
  filler_cell_placer_.CreateFillerMacros(2);
  if (!filler_cell_placer_.StartPlacement()) {
    LOG(error) << "Filler-cell placement failed\n";
    return false;
  }
  stage_timer.RecordEndTime();
  runtime_breakdown_.filler_placement_wall_seconds += stage_timer.GetWallTime();
  ++runtime_breakdown_.filler_placement_runs;
  return true;
}

bool Dali::RunIoPinPlacementStage() {
  if (disable_io_place_) {
    return true;
  }
  ElapsedTime stage_timer;
  stage_timer.RecordStartTime();
  InstantiateIoPlacer();
  bool is_io_placer_config_success =
      io_placer_->SetGlobalMetalLayer(io_metal_layer_);
  DaliExpects(is_io_placer_config_success,
              "Cannot successfully configure I/O placer");
  if (!io_placer_->RunAutoPlacement()) {
    LOG(error) << "I/O pin placement failed\n";
    return false;
  }
  stage_timer.RecordEndTime();
  runtime_breakdown_.io_placement_wall_seconds += stage_timer.GetWallTime();
  ++runtime_breakdown_.io_placement_runs;
  return true;
}

std::vector<PlacementSnapshotStage> Dali::ExpectedSnapshotStages() const {
  // Declare the stages that will actually run this configuration, in execution
  // order, so consumers (the live GUI) reserve exactly those chart slots.
  std::vector<PlacementSnapshotStage> stages;
  stages.push_back({"global_placement", "Global placement"});
  if (topology_stage_boundary_sizing_ || topology_adaptive_sizing_) {
    stages.push_back({"topology_change", "Topology change"});
  }
  if (!disable_legalization_) {
    stages.push_back({"legalization", "Legalization"});
    // Detailed placement produces a curve only when it actually runs, which
    // differs between the standard-cell and gridded-cell flows.
    const bool has_detailed = is_standard_cell_
                                  ? !disable_detailed_place_
                                  : (enable_gridded_detailed_placement_ ||
                                     enable_gridded_local_reorder_);
    if (has_detailed) {
      stages.push_back({"detailed_placement", "Detailed placement"});
    }
  }
  return stages;
}

/** Wire up the snapshot sink stages will publish to. */
void Dali::InitializeVisualizationSnapshots() {
  if (!gui_debug_) {
    snapshot_sink_.reset();
    return;
  }
  if (snapshot_sink_ != nullptr && snapshot_sink_->IsEnabled()) {
    return;
  }
  if (!gui_snapshot_sink_factory_) {
    LOG(error) << "GUI debug mode requested, but this Dali executable does "
                  "not include a GUI snapshot sink. It was built without Qt6 "
                  "Widgets; install Qt6 and reconfigure to get one.\n";
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
    const std::string &id, const std::string &label, const std::string &group,
    const std::string &subgroup, int iteration,
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
  metadata.delay_lines = BuildDelayLineVisualization();
  metadata.topology_generation = topology_generation_;
  metadata.topology_changes = BuildTopologyChangeVisualization();
  metadata.topology_added_component_ids = BuildTopologyAddedComponentIds();
  metadata.well_rects = std::move(well_rects);
  PopulateTimingVisualization(&metadata);
  metadata.sizing_decisions = BuildSizingDecisionEvidence();
  snapshot_sink_->PublishSnapshot(&circuit_, metadata);
}

std::vector<PlacementTopologySiteChange>
Dali::BuildTopologyChangeVisualization() {
  std::vector<PlacementTopologySiteChange> changes;
  changes.reserve(visualization_topology_requests_.size());
  for (const TopologyChangeRequest &request : visualization_topology_requests_) {
    PlacementTopologySiteChange change;
    change.site = request.site;
    change.current_pairs = request.current_pairs;
    change.requested_pairs = request.requested_pairs;
    for (const SiteVerdict &verdict : manifest_verdicts_) {
      if (verdict.site == request.site) {
        change.boundary_slack_ps = verdict.binding_slack_ps;
        change.has_boundary_slack = true;
        break;
      }
    }
    changes.push_back(std::move(change));
  }
  return changes;
}

/**
 * Attach the latest timing witnesses, resolved to component ids.
 *
 * The resolver is a lookup into the live circuit, so this must run while the
 * circuit is alive -- which is the whole reason the work happens here and not
 * in the viewer. A pin that names no component, an I/O endpoint or a terminal
 * inside a replaceable site, resolves to -1 and the path is marked as carrying
 * no geometry rather than being given a coordinate it does not have.
 */
void Dali::PopulateTimingVisualization(PlacementSnapshotMetadata *metadata) {
  if (last_timing_snapshot_.relative_constraints.empty()) return;

  std::vector<std::string> registered;
  registered.reserve(delay_line_spread_requests_.size());
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    registered.push_back(request.name_prefix);
  }

  TimingVisualizationResult result = BuildTimingPathVisualization(
      last_timing_snapshot_, registered,
      [this](const std::string &name) -> int {
        const Component *component = circuit_.GetComponentPtr(name);
        return component == nullptr ? -1 : component->Id();
      });

  metadata->timing_sample_stage = last_timing_sample_stage_;
  metadata->delay_line_timing = std::move(result.delay_lines);
  metadata->unattributed_constraints = std::move(result.unattributed);
  metadata->ambiguous_constraints = std::move(result.ambiguous);
}

/**
 * What Dali decided, per site, as far as it is known at this frame.
 *
 * Built from the request Dali issued and the delta the host returned, so a
 * frame published before the mutation shows the request with no actual growth
 * yet, and a later frame shows both. Nothing here is inferred: a field with no
 * evidence keeps its unset marker so the viewer can say so.
 */
std::vector<PlacementSizingDecisionEvidence> Dali::BuildSizingDecisionEvidence() {
  std::vector<PlacementSizingDecisionEvidence> evidence;
  for (const TopologyChangeRequest &request : visualization_topology_requests_) {
    PlacementSizingDecisionEvidence entry;
    entry.site = request.site;
    entry.decision_point = "end_of_global_placement";
    entry.current_pairs = request.current_pairs;
    entry.requested_pairs = request.requested_pairs;
    entry.expected_added_components = request.expected_added_components;
    entry.expected_added_nets = request.expected_added_nets;
    for (const SiteVerdict &verdict : manifest_verdicts_) {
      if (verdict.site != request.site) continue;
      entry.boundary_slack_ps = verdict.binding_slack_ps;
      entry.has_boundary_slack = true;
      break;
    }
    // The measured response Dali actually selected: the table entry for the
    // target it chose. Copied rather than recomputed, so the pane reports the
    // decision instead of re-deriving one.
    const auto response = delay_line_response_characterization_.find(request.site);
    if (response != delay_line_response_characterization_.end()) {
      for (const DelayLineSiteMeasurement::ResponsePoint &point :
           response->second.points) {
        if (point.target_pairs != request.requested_pairs) continue;
        entry.measured_response_ps = point.covered_deficit_ps;
        entry.has_measured_response = true;
        break;
      }
    }
    if (manifest_mutation_applied_) {
      // Per site, not the aggregate: a site's own growth is what its row is
      // about, and the rewired count is a property of the whole batch.
      const std::string prefix = request.site + "_";
      entry.actual_added_components = 0;
      entry.actual_added_nets = 0;
      for (const std::string &name : topology_added_component_names_) {
        if (name.compare(0, prefix.size(), prefix) == 0) {
          ++entry.actual_added_components;
        }
      }
      entry.actual_added_nets = entry.actual_added_components;
      // Batch scope, recorded as batch scope. There is no per-site rewire:
      // each rewired net joins two neighbouring sites.
      entry.batch_added_components = manifest_added_components_;
      entry.batch_added_nets = manifest_added_nets_;
      entry.batch_retired_nets = manifest_retired_nets_;
      entry.batch_rewired_nets = manifest_rewired_nets_;
    }
    for (const DelayLineSpreadRequest &line : delay_line_spread_requests_) {
      if (line.name_prefix != request.site) continue;
      DelayLineChain chain;
      std::string error_message;
      if (BuildDelayLineChain(circuit_, line.name_prefix, &chain,
                              &error_message)) {
        entry.final_pairs = static_cast<int>(chain.nodes.size()) / 2;
      }
      if (line.has_binding_slack) {
        entry.final_slack_ps = line.binding_slack;
        entry.has_final_slack = true;
        entry.closed = line.binding_slack >= topology_sizing_margin_ps_;
      }
      break;
    }
    evidence.push_back(std::move(entry));
  }
  return evidence;
}

std::vector<int> Dali::BuildTopologyAddedComponentIds() {
  std::vector<int> component_ids;
  component_ids.reserve(topology_added_component_names_.size());
  for (const std::string &name : topology_added_component_names_) {
    const Component *component = circuit_.GetComponentPtr(name);
    if (component != nullptr) component_ids.push_back(component->Id());
  }
  return component_ids;
}

void Dali::PublishDelayLineFeedbackSnapshots(int iteration) {
  if (snapshot_sink_ == nullptr || !snapshot_sink_->IsEnabled()) {
    pending_delay_line_feedback_events_.clear();
    return;
  }
  for (const DelayLineFeedbackEvent &event :
       pending_delay_line_feedback_events_) {
    PlacementSnapshotMetadata metadata;
    metadata.id = "global_placement.delay_line_feedback." +
                  std::to_string(iteration) + "." + event.line_name;
    metadata.label = "Delay line " + event.line_name + ": separation " +
                     std::to_string(event.old_separation) + " -> " +
                     std::to_string(event.new_separation);
    metadata.group = "global_placement";
    metadata.subgroup = "delay_line_feedback";
    metadata.iteration = event.iteration;
    metadata.is_delay_line_feedback = true;
    metadata.delay_line_name = event.line_name;
    metadata.delay_line_constraint_ids = event.constraint_ids;
    metadata.delay_line_binding_slack_ps = event.binding_slack_ps;
    metadata.delay_line_old_separation = event.old_separation;
    metadata.delay_line_new_separation = event.new_separation;
    metadata.delay_line_measured_gain_ps_per_row =
        event.measured_gain_ps_per_row;
    metadata.delay_lines = BuildDelayLineVisualization();
    snapshot_sink_->PublishSnapshot(&circuit_, metadata);
    snapshot_sink_->FlushEvents();
  }
  pending_delay_line_feedback_events_.clear();
}

void Dali::WriteInteractiveCommandSnapshot(const std::string &command) {
  if (!interactive_session_expected_ || !is_circuit_initialized_) {
    return;
  }
  InitializeVisualizationSnapshots();
  std::vector<PlacementWellRect> well_rects;
  if (!is_standard_cell_ && !disable_legalization_) {
    well_rects = well_legalizer_.CollectWellVisualizationRects();
  }
  WriteVisualizationSnapshot("interactive." + command,
                             "After command: " + command, "interactive", "", -1,
                             std::move(well_rects));
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
  if (!HasInputDesign()) {
    LOG(error) << "Placement requires both LEF and DEF inputs\n";
    return false;
  }
  ElapsedTime placement_timer;
  placement_timer.RecordStartTime();
  ApplyPlacementOverrides(density, number_of_threads);
  InitializeMainPlacementCircuit();
  ResolveTargetDensity();

  if (!RunCorePlacementStages()) return false;

  // I/O locations participate in timing and must therefore be fixed before the
  // final-equivalent measurement. Filler cells do not participate in timing;
  // inserting them after closure keeps this irreversible stage out of every
  // correction epoch.
  if (!RunIoPinPlacementStage()) return false;

  if (!RunAdaptiveDelayLineSizingEpochs()) return false;

  // The timing state the closure report is about to grade, observed before it
  // grades anything. Every earlier sample is taken with some stage still to
  // run, so none of them can say which boundary a line's slack was lost at --
  // the in-loop controller's last measurement and this one differed by 13 to
  // 35 ps with no sample in between to attribute the difference to.
  LogDelayLineStageSpans("final_equivalent");
  ObserveTimingDomain("final_equivalent", -1);
  if (!RunFillerCellPlacement()) return false;

  LOG(debug) << "dali git commit: " << get_git_version_short() << "\n";
  // A successful adaptive run returns only after measuring a closed topology.
  // I/O placement already happened and fillers do not participate in timing,
  // so repeating the full timing analysis here would grade the same state.
  ReportDelayLineClosure(topology_adaptive_sizing_);
  if (!sizing_manifest_path_.empty()) {
    WriteSizingManifest(sizing_manifest_path_);
  }
  RecordPlacementHpwlMetrics("final", circuit_);
  std::vector<PlacementWellRect> final_well_rects;
  if (!is_standard_cell_ && !disable_legalization_) {
    final_well_rects = well_legalizer_.CollectWellVisualizationRects();
  }
  WriteVisualizationSnapshot("final", "Final", "final", "", -1,
                             std::move(final_well_rects));
  if (!interactive_session_expected_) {
    FinishVisualizationSnapshots();
  }

  placement_timer.RecordEndTime();
  runtime_breakdown_.placement_scope_wall_seconds +=
      placement_timer.GetWallTime();
  ++runtime_breakdown_.placement_runs;

  return true;
}

void Dali::AddWellTaps(phydb::Macro *cell, double cell_interval_microns,
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

/**
 * Insert well taps at a fixed pitch, driven by argv-style arguments.
 *
 * Part of the interactive API rather than the batch flow; the batch flow
 * inserts taps through the legalizer's physical completion instead.
 */
bool Dali::AddWellTaps(int argc, char **argv) {
  phydb::Macro *cell = nullptr;
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
void Dali::ExternalDetailedPlaceAndLegalize(std::string const &engine,
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

void Dali::ExportToPhyDB(PhyDBExportMode mode) {
  // 1. COMPONENTS
  ExportComponentsToPhyDB();
  // 2. IOPINs
  ExportIoPinsToPhyDB();
  if (well_legalizer_.ckt_ptr_ != nullptr) {
    // 3. MiniRows
    ExportMiniRowsToPhyDB();
    if (mode == PhyDBExportMode::kFull) {
      ExportPpNpToPhyDB();
      ExportWellToPhyDB();
    }
  }
}

bool Dali::ExportPlacement(const std::string &output_name) {
  if (!HasInputDesign()) {
    LOG(error) << "Cannot export placement without LEF and DEF inputs\n";
    return false;
  }
  InitializeCircuitFromPhyDBIfNeeded();
  const std::string resolved_output_name = NormalizeDefOutputBaseName(
      output_name.empty() ? output_name_ : output_name);
  if (resolved_output_name.empty()) {
    LOG(error) << "Placement output name must not be empty\n";
    return false;
  }
  const std::filesystem::path output_path(resolved_output_name);
  if (!output_path.parent_path().empty()) {
    std::error_code error;
    std::filesystem::create_directories(output_path.parent_path(), error);
    if (error) {
      LOG(error) << "Cannot create placement output directory "
                 << output_path.parent_path().string() << ": "
                 << error.message() << "\n";
      return false;
    }
  }

  MaybeExportToLEF(input_lef_file_name_, resolved_output_name);
  ExportToDEF(input_def_file_name_, resolved_output_name);
  ExportToPhyDB();
  phy_db_ptr_->WriteDef("phydb.def");
  has_explicit_placement_export_ = true;
  return true;
}

bool Dali::HasExplicitPlacementExport() const {
  return has_explicit_placement_export_;
}

void Dali::RebindPhyDB(phydb::PhyDB *phy_db_ptr) {
  DaliExpects(phy_db_ptr != nullptr, "Cannot rebind Dali to a null PhyDB");
  const bool reinitialize_timing = timing_analysis_initialized_;
  timing_analysis_initialized_ = false;
  phy_db_ptr_ = phy_db_ptr;
  circuit_.SetPhyDB(phy_db_ptr);
  filler_cell_placer_.phy_db_ptr_ = phy_db_ptr;
  if (io_placer_ != nullptr) {
    io_placer_->SetPhyDB(phy_db_ptr);
  }
  if (reinitialize_timing) {
#if PHYDB_USE_GALOIS
    InitializeTimingDrivenPlacement();
#else
    LOG(error) << "Cannot restore timing after a PhyDB rebind because Dali "
                  "was built without GaloisEDA support.\n";
#endif
  }
  // Measured against a netlist that no longer exists. Discarded rather than
  // repointed, so a stale measurement cannot be mistaken for a current one.
  last_timing_snapshot_ = TimingSnapshot();
  LOG(info) << "  Rebound Dali to a rebuilt PhyDB: "
            << phy_db_ptr->design().GetComponentsRef().size()
            << " components, " << phy_db_ptr->design().GetNetsRef().size()
            << " nets\n";
}

void Dali::SeedTopologyDeltaComponents(TopologyDelta &delta) const {
  size_t seeded = 0;
  for (TopologyDeltaComponent &component : delta.added_components) {
    const DelayLineSpreadRequest *line = nullptr;
    for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
      if (component.name.compare(0, request.name_prefix.size(),
                                 request.name_prefix) == 0) {
        line = &request;
        break;
      }
    }
    if (line == nullptr) continue;
    double sum_x = 0.0;
    double sum_y = 0.0;
    int count = 0;
    for (const Component &existing : circuit_.Components()) {
      if (existing.Name().compare(0, line->name_prefix.size(),
                                  line->name_prefix) != 0) {
        continue;
      }
      sum_x += existing.LLX();
      sum_y += existing.LLY();
      ++count;
    }
    if (count == 0) continue;
    component.seed_x = sum_x / count;
    component.seed_y = sum_y / count;
    ++seeded;
    // Per cell, so locality is a measurement rather than an assurance. The seed
    // is the centroid of the line the cell belongs to, which is local by
    // construction; what that does not say is where legalization then puts it,
    // and only the pair of coordinates together answers that.
    LOG(info) << "  SEEDED_COMPONENT name " << component.name << " site "
              << line->name_prefix << " seed_x " << component.seed_x
              << " seed_y " << component.seed_y << " line_cells " << count
              << "\n";
  }
  LOG(info) << "  Seeded " << seeded << " of " << delta.added_components.size()
            << " added components from their registered delay line ("
            << delay_line_spread_requests_.size() << " lines registered)\n";
}

void Dali::EnableTimingDomainObservation(const std::string &prefix,
                                         std::vector<std::string> sites) {
  timing_domain_observation_ = true;
  timing_domain_prefix_ = prefix;
  timing_domain_sites_ = std::move(sites);
}

/**
 * Records one placement state: coordinates, HPWL, per-site slack, and the full
 * constraint set.
 *
 * The coordinate digest is what makes the four states comparable at all -- two
 * states with the same HPWL are not necessarily the same placement, and the
 * whole question here is which coordinates the timing was taken on.
 */
/**
 * Digest of every I/O pin's placement.
 *
 * Separate from the component digest because I/O placement is its own stage and
 * moves on its own schedule; a boundary that changed only the pins would
 * otherwise be indistinguishable from one that changed nothing.
 */
unsigned long long Dali::IoPinPlacementDigest() {
  std::vector<ComponentPlacement> pins;
  const std::vector<IoPin> &io_pins = circuit_.design().IoPins();
  pins.reserve(io_pins.size());
  for (std::size_t index = 0; index < io_pins.size(); ++index) {
    const IoPin &pin = io_pins[index];
    pins.push_back({static_cast<int>(index), pin.X(), pin.Y(),
                    static_cast<int>(pin.GetPlaceStatus())});
  }
  return PlacementDigest(pins);
}

void Dali::ObserveTimingDomain(const std::string &stage, int iteration) {
#if PHYDB_USE_GALOIS
  if (!timing_domain_observation_) return;

  unsigned long long digest = 1469598103934665603ull;
  for (const Component &component : circuit_.Components()) {
    const long long x = static_cast<long long>(component.LLX() * 1000.0);
    const long long y = static_cast<long long>(component.LLY() * 1000.0);
    for (long long value : {x, y}) {
      digest ^= static_cast<unsigned long long>(value);
      digest *= 1099511628211ull;
    }
  }

  const double weighted = circuit_.WeightedHPWL();
  const double unweighted = circuit_.UnweightedHPWL();

  if (!ReportTiming()) {
    LOG(error) << "TIMING_DOMAIN_FAILED stage " << stage
               << " could not measure timing\n";
    return;
  }
  if (!AttributeDelayLineRequests(&delay_line_spread_requests_,
                                  last_timing_snapshot_)) {
    LOG(error) << "TIMING_DOMAIN_FAILED stage " << stage
               << " could not attribute constraints\n";
    return;
  }

  ++timing_generation_;
  last_timing_sample_stage_ = stage;
  LOG(info) << "TIMING_DOMAIN stage " << stage << " iteration " << iteration
            << " digest " << digest << " weighted_hpwl " << weighted
            << " unweighted_hpwl " << unweighted << " generation "
            << timing_generation_ << " rc_enabled " << (timing_use_rc_ ? 1 : 0)
            << " configured_layer " << rc_min_routing_layer_
            << " effective_layer " << EffectiveRcRoutingLayer()
            << " resolved_horizontal "
            << (rc_estimator == nullptr
                    ? std::string("none")
                    : rc_estimator->ResolvedHorizontalLayerName())
            << " resolved_vertical "
            << (rc_estimator == nullptr
                    ? std::string("none")
                    : rc_estimator->ResolvedVerticalLayerName())
            << " io_pin_digest " << IoPinPlacementDigest() << "\n";
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    DelayLineChain chain;
    std::string error_message;
    const bool readable = BuildDelayLineChain(circuit_, request.name_prefix,
                                              &chain, &error_message);
    double span_um = -1.0;
    if (readable) {
      double min_y = std::numeric_limits<double>::max();
      double max_y = std::numeric_limits<double>::lowest();
      for (const DelayLineNode &node : chain.nodes) {
        const Component &component = circuit_.Components()[node.component_id];
        min_y = std::min(min_y, component.LLY());
        max_y = std::max(max_y, component.URY());
      }
      span_um = (max_y - min_y) * circuit_.GridValueY();
    }
    // Cell and wire are split by the same discriminator the decomposition
    // report uses -- a witness step carrying a net name is a leg across that
    // net -- so a boundary that moves wire delay can be told from one that
    // moves cell delay without consulting a second classifier.
    TimingConstraintDecomposition decomposition;
    std::string decomposition_error;
    bool decomposed = false;
    for (const RelativeTimingConstraintSnapshot &constraint :
         last_timing_snapshot_.relative_constraints) {
      if (constraint.constraint_id != request.binding_constraint_id) continue;
      decomposed = DecomposeTimingConstraint(constraint, &decomposition,
                                             &decomposition_error);
      break;
    }
    LOG(info) << "TIMING_DOMAIN_SITE stage " << stage << " site "
              << request.name_prefix << " pairs "
              << (readable ? static_cast<int>(chain.nodes.size()) / 2 : -1)
              << " binding_slack_ps " << request.binding_slack
              << " binding_constraint_id " << request.binding_constraint_id
              << " binding_identity '"
              << BindingConstraintIdentity(request.binding_constraint_id)
              << "' separation " << request.separation << " span_um " << span_um
              << " line_digest "
              << PlacementDigest(SnapshotDelayLinePlacement(request.name_prefix))
              << " fast_total_ps "
              << (decomposed ? decomposition.fast.total_delay : -1.0)
              << " fast_cell_ps "
              << (decomposed ? decomposition.fast.cell_delay : -1.0)
              << " fast_wire_ps "
              << (decomposed ? decomposition.fast.wire_delay : -1.0)
              << " slow_total_ps "
              << (decomposed ? decomposition.slow.total_delay : -1.0)
              << " slow_cell_ps "
              << (decomposed ? decomposition.slow.cell_delay : -1.0)
              << " slow_wire_ps "
              << (decomposed ? decomposition.slow.wire_delay : -1.0)
              << " constraint_count " << request.matched_constraint_ids.size()
              << " attributed " << (request.has_binding_slack ? 1 : 0) << "\n";
  }
  // One file per capture rather than per stage: the accepted-physical name was
  // rewritten by all forty-five samples, so any claim about the identities of
  // samples other than the last was unverifiable.
  const std::string suffix =
      iteration >= 0 ? "_" + std::to_string(iteration) : std::string();
  unsigned long long identity_digest = 0;
  std::size_t identity_count = 0;
  const bool wrote = WriteCurrentTimingConstraintIdentities(
      timing_domain_prefix_ + "_" + stage + suffix + ".json",
      timing_domain_sites_, &identity_digest, &identity_count);
  if (!wrote) {
    LOG(error) << "TIMING_DOMAIN_FAILED stage " << stage
               << " could not write semantic identities\n";
    return;
  }
  LOG(info) << "TIMING_DOMAIN_IDENTITY stage " << stage << " iteration "
            << iteration << " count " << identity_count << " digest "
            << identity_digest << "\n";
#else
  (void)stage;
  (void)iteration;
#endif
}

void Dali::SetFixedTopologyRequest(const std::string &site, int current_pairs,
                                   int requested_pairs) {
  fixed_topology_request_.site = site;
  fixed_topology_request_.current_pairs = current_pairs;
  fixed_topology_request_.requested_pairs = requested_pairs;
  const int added = requested_pairs - current_pairs;
  fixed_topology_request_.expected_added_components = 2 * added;
  fixed_topology_request_.expected_added_nets = 2 * added;
  has_fixed_topology_request_ = true;
}

void Dali::SetDelayLineCharacterization(const std::string &site,
                                        double ps_per_pair, int min_pairs,
                                        int max_pairs) {
  SiteCharacterization characterization;
  characterization.ps_per_pair = ps_per_pair;
  characterization.min_pairs = min_pairs;
  characterization.max_pairs = max_pairs;
  delay_line_characterization_[site] = characterization;
}

bool Dali::AddDelayLineResponse(const std::string &site, int current_pairs,
                                int target_pairs,
                                double covered_deficit_ps) {
  SiteResponseCharacterization &response =
      delay_line_response_characterization_[site];
  if (!response.points.empty()) {
    if (response.current_pairs != current_pairs) {
      LOG(error) << "delay-line-response for '" << site
                 << "' mixes current pair counts " << response.current_pairs
                 << " and " << current_pairs << "\n";
      return false;
    }
    const DelayLineSiteMeasurement::ResponsePoint &last =
        response.points.back();
    if (target_pairs <= last.target_pairs ||
        covered_deficit_ps <= last.covered_deficit_ps) {
      LOG(error) << "delay-line-response for '" << site
                 << "' must increase both target pairs and covered deficit\n";
      return false;
    }
  } else {
    response.current_pairs = current_pairs;
  }
  response.points.push_back({target_pairs, covered_deficit_ps});
  return true;
}

/**
 * Measure every registered site, decide once, and apply only if there is a
 * request.
 *
 * Runs with the placement engines closed, which is what makes the timing
 * measurement here describe a state the flow could actually produce. Every
 * refusal is a failure rather than a quiet continuation: evidence that cannot
 * support a decision must stop the run, or the automatic path would report
 * success for sites it never looked at.
 */
TopologyMutationResult Dali::HandleTopologyCheckpoint(
    const TopologyCheckpointContext &context) {
  return DecideAndApplyTopologyChange(context, "sizing_measurement");
}

/**
 * The one place a site and a count are chosen, whoever asked.
 *
 * Both the in-loop checkpoint and the stage boundary come through here, so the
 * policy cannot diverge between them; only the state it measures and the name
 * that state is recorded under differ. Nothing outside Dali decides anything:
 * the host is handed an already-decided request and reports what ACT produced.
 */
TopologyMutationResult Dali::DecideAndApplyTopologyChange(
    const TopologyCheckpointContext &context, const std::string &stage) {
#if PHYDB_USE_GALOIS
  // Sizing is engaged only when the recipe asked for it. A recipe that
  // configured neither a characterized site nor a fixed request never intended
  // a topology change, so its checkpoint is a plain no-change boundary rather
  // than a decision that cannot be made -- which is what the transparency proof
  // exercises.
  if (!has_fixed_topology_request_ && delay_line_characterization_.empty() &&
      delay_line_response_characterization_.empty()) {
    LOG(info) << "  SIZING_DECISION outcome no-change reason no sizing was "
                 "configured for this run\n";
    return TopologyMutationResult::NoChange();
  }
  if (!delay_line_characterization_.empty() &&
      !delay_line_response_characterization_.empty()) {
    return TopologyMutationResult::Failed(
        "scalar and fixed-response characterizations cannot be mixed");
  }
  if (delay_line_spread_requests_.empty()) {
    return TopologyMutationResult::Failed(
        "no delay line is registered, so no site can be measured");
  }
  ObserveTimingDomain(stage, context.checkpoint_iteration);
  if (!ReportTiming()) {
    return TopologyMutationResult::Failed(
        "timing could not be measured at the checkpoint");
  }
  if (!AttributeDelayLineRequests(&delay_line_spread_requests_,
                                  last_timing_snapshot_)) {
    return TopologyMutationResult::Failed(
        "constraints could not be attributed to the registered sites");
  }

  std::vector<DelayLineSiteMeasurement> measurements;
  SizingLimits limits;
  limits.margin_ps = topology_sizing_margin_ps_;
  limits.max_added_pairs = topology_sizing_max_added_pairs_;
  limits.max_pairs = topology_sizing_max_pairs_;
  limits.component_headroom = context.component_headroom;
  limits.net_headroom = context.net_headroom;

  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    limits.expected_sites.push_back(request.name_prefix);
    DelayLineChain chain;
    std::string error_message;
    if (!BuildDelayLineChain(circuit_, request.name_prefix, &chain,
                             &error_message)) {
      return TopologyMutationResult::Failed(
          "site '" + request.name_prefix + "' has no readable chain: " +
          error_message);
    }
    DelayLineSiteMeasurement measurement;
    measurement.site = request.name_prefix;
    measurement.current_pairs = static_cast<int>(chain.nodes.size()) / 2;
    measurement.binding_slack_ps = request.binding_slack;
    measurement.has_attribution = request.has_binding_slack;
    const auto characterization =
        delay_line_characterization_.find(request.name_prefix);
    if (characterization != delay_line_characterization_.end()) {
      measurement.ps_per_pair = characterization->second.ps_per_pair;
      measurement.has_coefficient = true;
      measurement.characterized_min_pairs = characterization->second.min_pairs;
      measurement.characterized_max_pairs = characterization->second.max_pairs;
    }
    const auto response =
        delay_line_response_characterization_.find(request.name_prefix);
    if (response != delay_line_response_characterization_.end() &&
        response->second.current_pairs == measurement.current_pairs) {
      measurement.response_points = response->second.points;
    }
    LOG(info) << "  SIZING_MEASURE site " << measurement.site << " pairs "
              << measurement.current_pairs << " binding_slack_ps "
              << measurement.binding_slack_ps << " attributed "
              << (measurement.has_attribution ? 1 : 0) << " ps_per_pair "
              << (measurement.has_coefficient ? measurement.ps_per_pair : 0.0)
              << "\n";
    measurements.push_back(std::move(measurement));
  }
  manifest_boundary_sites_ = measurements;

  if (has_fixed_topology_request_) {
    LOG(info) << "  SIZING_DECISION outcome fixed site "
              << fixed_topology_request_.site << " current_pairs "
              << fixed_topology_request_.current_pairs << " requested_pairs "
              << fixed_topology_request_.requested_pairs
              << " reason predetermined by the recipe\n";
    TopologyChangeBatch batch;
    batch.requests.push_back(fixed_topology_request_);
    return ApplyTopologyChangeRequest(context, batch);
  }

  // The batch path sizes every site that needs it; the single-site path sizes
  // the worst one. Both remain because they answer different questions, and the
  // single-site one is what the in-loop checkpoint was proved with.
  if (topology_batch_sizing_) {
    const BatchSizingDecision batch_decision =
        delay_line_response_characterization_.empty()
            ? DecideDelayLineBatchSizing(
                  measurements, limits, topology_batch_require_characterized_)
            : DecideDelayLineResponseBatch(
                  measurements, limits, topology_batch_require_characterized_);
    for (const SiteVerdict &verdict : batch_decision.verdicts) {
      LOG(info) << "  SIZING_VERDICT site " << verdict.site << " outcome "
                << (verdict.outcome == SizingOutcome::kRequest
                        ? "request"
                        : (verdict.outcome == SizingOutcome::kDecline
                               ? "decline"
                               : "ineligible"))
                << " current_pairs " << verdict.current_pairs
                << " requested_pairs " << verdict.requested_pairs
                << " binding_slack_ps " << verdict.binding_slack_ps
                << " deficit_ps " << verdict.deficit_ps << " reason "
                << verdict.reason << "\n";
    }
    LOG(info) << "  SIZING_BATCH outcome "
              << (batch_decision.outcome == SizingOutcome::kRequest
                      ? "request"
                      : (batch_decision.outcome == SizingOutcome::kDecline
                             ? "decline"
                             : "invalid"))
              << " sites " << batch_decision.requests.size()
              << " added_components " << batch_decision.expected_added_components
              << " added_nets " << batch_decision.expected_added_nets
              << " reason " << batch_decision.reason << "\n";

    if (batch_decision.outcome == SizingOutcome::kInvalid) {
      return TopologyMutationResult::Failed("batch sizing evidence rejected: " +
                                            batch_decision.reason);
    }
    manifest_verdicts_ = batch_decision.verdicts;
    if (batch_decision.outcome == SizingOutcome::kDecline) {
      return TopologyMutationResult::NoChange();
    }
    TopologyChangeBatch batch;
    for (const SiteVerdict &verdict : batch_decision.requests) {
      TopologyChangeRequest request;
      request.site = verdict.site;
      request.current_pairs = verdict.current_pairs;
      request.requested_pairs = verdict.requested_pairs;
      const int added = verdict.requested_pairs - verdict.current_pairs;
      request.expected_added_components = 2 * added;
      request.expected_added_nets = 2 * added;
      batch.requests.push_back(std::move(request));
    }
    return ApplyTopologyChangeRequest(context, batch);
  }

  const SizingDecision decision = DecideDelayLineSizing(measurements, limits);
  LOG(info) << "  SIZING_DECISION outcome "
            << (decision.outcome == SizingOutcome::kRequest
                    ? "request"
                    : (decision.outcome == SizingOutcome::kDecline ? "decline"
                                                                  : "invalid"))
            << " site " << (decision.site.empty() ? "-" : decision.site)
            << " current_pairs " << decision.current_pairs
            << " requested_pairs " << decision.requested_pairs
            << " deficit_ps " << decision.deficit_ps << " reason "
            << decision.reason << "\n";

  if (decision.outcome == SizingOutcome::kInvalid) {
    return TopologyMutationResult::Failed("sizing evidence rejected: " +
                                          decision.reason);
  }
  if (decision.outcome == SizingOutcome::kDecline) {
    // Nothing to apply, so the host is not called at all.
    return TopologyMutationResult::NoChange();
  }
  if (topology_checkpoint_host_ == nullptr) {
    return TopologyMutationResult::Failed(
        "a topology change was decided but no host is registered to apply it");
  }

  TopologyChangeRequest request;
  request.site = decision.site;
  request.current_pairs = decision.current_pairs;
  request.requested_pairs = decision.requested_pairs;
  request.expected_added_components = decision.expected_added_components;
  request.expected_added_nets = decision.expected_added_nets;
  TopologyChangeBatch batch;
  batch.requests.push_back(std::move(request));
  return ApplyTopologyChangeRequest(context, batch);
#else
  (void)context;
  (void)stage;
  // Same first question the timing branch asks. A recipe that configured no
  // characterized site and no fixed request never intended a topology change,
  // so its boundary is a plain no-change here too. Failing it instead made an
  // enabled-but-unconfigured stage boundary fail the run in a build that was
  // never going to size anything.
  if (!has_fixed_topology_request_ && delay_line_characterization_.empty() &&
      delay_line_response_characterization_.empty()) {
    return TopologyMutationResult::NoChange();
  }
  return TopologyMutationResult::Failed(
      "timing-driven sizing requires a Galois-enabled build");
#endif
}

/**
 * Sends a decided request to the host and holds the answer to it.
 *
 * Shared by the automatic and fixed paths so the fixed gate exercises the same
 * transport and the same checks, and cannot quietly diverge from the path the
 * policy uses.
 */
TopologyMutationResult Dali::ApplyTopologyChangeRequest(
    const TopologyCheckpointContext &context,
    const TopologyChangeBatch &batch) {
  if (topology_checkpoint_host_ == nullptr) {
    return TopologyMutationResult::Failed(
        "a topology change was decided but no host is registered to apply it");
  }
  if (batch.IsEmpty()) {
    return TopologyMutationResult::Failed(
        "an empty batch reached the host call; nothing to change should have "
        "declined instead");
  }
  // Checked here as well as in the policy, because this is the last point
  // before an authoritative netlist is told to change.
  for (size_t index = 1; index < batch.requests.size(); ++index) {
    if (!(batch.requests[index - 1].site < batch.requests[index].site)) {
      return TopologyMutationResult::Failed(
          "batch requests are not in strict site-name order, so they are "
          "either unsorted or contain a duplicate site");
    }
  }
  for (const TopologyChangeRequest &request : batch.requests) {
    if (request.requested_pairs <= request.current_pairs) {
      return TopologyMutationResult::Failed(
          "batch request for '" + request.site + "' would not grow the site");
    }
  }

  visualization_topology_requests_ = batch.requests;
  WriteVisualizationSnapshot("topology_change.request", "Topology request",
                             "topology_change", "request");
  FlushVisualizationEvents();

  const double timing_before_host =
      runtime_breakdown_.timing_initialize_wall_seconds +
      runtime_breakdown_.timing_export_locations_wall_seconds +
      runtime_breakdown_.timing_update_rc_wall_seconds +
      runtime_breakdown_.timing_analysis_wall_seconds +
      runtime_breakdown_.timing_witness_capture_wall_seconds +
      runtime_breakdown_.timing_other_wall_seconds;
  ElapsedTime host_timer;
  host_timer.RecordStartTime();
  TopologyMutationResult result =
      topology_checkpoint_host_->ApplyTopologyChange(context, batch);
  host_timer.RecordEndTime();
  const double timing_after_host =
      runtime_breakdown_.timing_initialize_wall_seconds +
      runtime_breakdown_.timing_export_locations_wall_seconds +
      runtime_breakdown_.timing_update_rc_wall_seconds +
      runtime_breakdown_.timing_analysis_wall_seconds +
      runtime_breakdown_.timing_witness_capture_wall_seconds +
      runtime_breakdown_.timing_other_wall_seconds;
  runtime_breakdown_.topology_host_wall_seconds += std::max(
      0.0, host_timer.GetWallTime() - (timing_after_host - timing_before_host));
  ++runtime_breakdown_.topology_host_calls;
  if (result.status != TopologyMutationStatus::kApplied) return result;

  ElapsedTime bookkeeping_timer;
  bookkeeping_timer.RecordStartTime();
  // The host reports what ACT produced; this is where it is held to what was
  // asked for. A delta that grew by a different amount is not this experiment.
  const int added_components =
      static_cast<int>(result.delta.added_components.size());
  const int added_nets = static_cast<int>(result.delta.added_nets.size());
  if (added_components != batch.ExpectedAddedComponents() ||
      added_nets != batch.ExpectedAddedNets()) {
    return TopologyMutationResult::Failed(
        "ACT delta adds " + std::to_string(added_components) +
        " components and " + std::to_string(added_nets) + " nets; " +
        std::to_string(batch.ExpectedAddedComponents()) + " and " +
        std::to_string(batch.ExpectedAddedNets()) + " were requested");
  }
  if (!result.delta.retired_nets.empty()) {
    return TopologyMutationResult::Failed(
        "ACT delta retires " + std::to_string(result.delta.retired_nets.size()) +
        " nets; growing a delay site should retire none");
  }
  // Per site as well as in aggregate: a host that grew one site by another's
  // count would balance perfectly in the total and be wrong in every part.
  for (const TopologyChangeRequest &request : batch.requests) {
    const std::string prefix = request.site + "_";
    int site_added = 0;
    for (const TopologyDeltaComponent &component : result.delta.added_components) {
      if (component.name.compare(0, prefix.size(), prefix) == 0) ++site_added;
    }
    if (site_added != request.expected_added_components) {
      return TopologyMutationResult::Failed(
          "ACT added " + std::to_string(site_added) + " components to '" +
          request.site + "'; " +
          std::to_string(request.expected_added_components) + " were requested");
    }
    LOG(info) << "  SIZING_APPLIED site " << request.site << " pairs "
              << request.current_pairs << " -> " << request.requested_pairs
              << " added_components " << site_added << " added_nets "
              << site_added << " rewired " << result.delta.rewired_nets.size()
              << "\n";
  }
  if (topology_adaptive_sizing_) {
    manifest_requests_.insert(manifest_requests_.end(), batch.requests.begin(),
                              batch.requests.end());
    manifest_added_components_ += added_components;
    manifest_added_nets_ += added_nets;
    manifest_retired_nets_ +=
        static_cast<int>(result.delta.retired_nets.size());
    manifest_rewired_nets_ +=
        static_cast<int>(result.delta.rewired_nets.size());
  } else {
    manifest_requests_ = batch.requests;
    manifest_added_components_ = added_components;
    manifest_added_nets_ = added_nets;
    manifest_retired_nets_ =
        static_cast<int>(result.delta.retired_nets.size());
    manifest_rewired_nets_ =
        static_cast<int>(result.delta.rewired_nets.size());
  }
  manifest_mutation_applied_ = true;
  LOG(info) << "  SIZING_BATCH_APPLIED sites " << batch.requests.size()
            << " added_components " << added_components << " added_nets "
            << added_nets << " retired_nets " << result.delta.retired_nets.size()
            << " rewired_nets " << result.delta.rewired_nets.size() << "\n";
  SeedTopologyDeltaComponents(result.delta);
  for (const TopologyDeltaComponent &component : result.delta.added_components) {
    components_awaiting_legal_placement_.push_back(component.name);
    topology_added_component_names_.push_back(component.name);
  }
  bookkeeping_timer.RecordEndTime();
  runtime_breakdown_.topology_bookkeeping_wall_seconds +=
      bookkeeping_timer.GetWallTime();
  return result;
}

/**
 * The typed record of what this run decided, applied and produced.
 *
 * Written once, at the end, from state accumulated during the run rather than
 * recovered from the log. Every number the acceptance path needs is here with a
 * name, so a checker never has to know what a log line looked like.
 */
bool Dali::WriteSizingManifest(const std::string &file_name) {
#if PHYDB_USE_GALOIS
  auto quote = [](const std::string &value) { return "\"" + value + "\""; };
  std::ostringstream out;
  out.setf(std::ios::fixed);
  out << std::setprecision(6);
  out << "{\n";
  out << "  \"dali_commit\": " << quote(get_git_version_short()) << ",\n";
  out << "  \"rc\": {\"enabled\": " << (timing_use_rc_ ? "true" : "false")
      << ", \"configured_layer\": " << rc_min_routing_layer_
      << ", \"effective_layer\": " << EffectiveRcRoutingLayer()
      << ", \"horizontal\": "
      << quote(rc_estimator == nullptr
                   ? std::string()
                   : rc_estimator->ResolvedHorizontalLayerName())
      << ", \"vertical\": "
      << quote(rc_estimator == nullptr
                   ? std::string()
                   : rc_estimator->ResolvedVerticalLayerName())
      << "},\n";

  out << "  \"adaptive_config\": {\"enabled\": "
      << (topology_adaptive_sizing_ ? "true" : "false")
      << ", \"probe_pairs\": " << topology_adaptive_probe_pairs_
      << ", \"max_step_pairs\": " << topology_adaptive_max_step_pairs_
      << ", \"max_epochs\": " << topology_adaptive_max_epochs_
      << ", \"max_nonpositive_probes\": "
      << topology_adaptive_max_nonpositive_probes_
      << ", \"observe_global_iterations\": "
      << (timing_observe_global_iterations_ ? "true" : "false")
      << ", \"max_added_pairs\": " << topology_sizing_max_added_pairs_
      << ", \"max_pairs\": " << topology_sizing_max_pairs_ << "},\n";

  out << "  \"boundary\": {\"mutation_applied\": "
      << (manifest_mutation_applied_ ? "true" : "false")
      << ", \"components_before\": " << manifest_components_before_
      << ", \"nets_before\": " << manifest_nets_before_
      << ", \"components_after\": " << circuit_.Components().size()
      << ", \"nets_after\": " << circuit_.Nets().size()
      << ", \"added_components\": " << manifest_added_components_
      << ", \"added_nets\": " << manifest_added_nets_
      << ", \"retired_nets\": " << manifest_retired_nets_
      << ", \"rewired_nets\": " << manifest_rewired_nets_ << "},\n";

  out << "  \"verdicts\": [";
  for (size_t index = 0; index < manifest_verdicts_.size(); ++index) {
    const SiteVerdict &verdict = manifest_verdicts_[index];
    out << (index == 0 ? "\n" : ",\n") << "    {\"site\": "
        << quote(verdict.site) << ", \"outcome\": "
        << quote(verdict.outcome == SizingOutcome::kRequest
                     ? "request"
                     : (verdict.outcome == SizingOutcome::kDecline
                            ? "decline"
                            : "ineligible"))
        << ", \"current_pairs\": " << verdict.current_pairs
        << ", \"requested_pairs\": " << verdict.requested_pairs
        << ", \"binding_slack_ps\": " << verdict.binding_slack_ps
        << ", \"deficit_ps\": " << verdict.deficit_ps
        << ", \"reason\": " << quote(verdict.reason) << "}";
  }
  out << (manifest_verdicts_.empty() ? "" : "\n  ") << "],\n";

  out << "  \"boundary_sites\": [";
  for (size_t index = 0; index < manifest_boundary_sites_.size(); ++index) {
    const DelayLineSiteMeasurement &site = manifest_boundary_sites_[index];
    out << (index == 0 ? "\n" : ",\n") << "    {\"site\": "
        << quote(site.site) << ", \"current_pairs\": " << site.current_pairs
        << ", \"binding_slack_ps\": " << site.binding_slack_ps
        << ", \"attributed\": "
        << (site.has_attribution ? "true" : "false") << "}";
  }
  out << (manifest_boundary_sites_.empty() ? "" : "\n  ") << "],\n";

  out << "  \"response_characterization\": [";
  bool first_response = true;
  for (const auto &[site, response] :
       delay_line_response_characterization_) {
    for (const auto &point : response.points) {
      out << (first_response ? "\n" : ",\n") << "    {\"site\": "
          << quote(site) << ", \"current_pairs\": "
          << response.current_pairs << ", \"target_pairs\": "
          << point.target_pairs << ", \"covered_deficit_ps\": "
          << point.covered_deficit_ps << "}";
      first_response = false;
    }
  }
  out << (first_response ? "" : "\n  ") << "],\n";

  out << "  \"requests\": [";
  for (size_t index = 0; index < manifest_requests_.size(); ++index) {
    const TopologyChangeRequest &request = manifest_requests_[index];
    out << (index == 0 ? "\n" : ",\n") << "    {\"site\": "
        << quote(request.site) << ", \"current_pairs\": "
        << request.current_pairs << ", \"requested_pairs\": "
        << request.requested_pairs << ", \"expected_added_components\": "
        << request.expected_added_components
        << ", \"expected_added_nets\": " << request.expected_added_nets << "}";
  }
  out << (manifest_requests_.empty() ? "" : "\n  ") << "],\n";

  out << "  \"adaptive_epochs\": [";
  for (size_t epoch_index = 0; epoch_index < manifest_adaptive_epochs_.size();
       ++epoch_index) {
    const AdaptiveEpochManifest &epoch = manifest_adaptive_epochs_[epoch_index];
    out << (epoch_index == 0 ? "\n" : ",\n") << "    {\"epoch\": "
        << epoch.epoch << ", \"outcome\": "
        << quote(epoch.outcome == AdaptiveSizingOutcome::kRequest
                     ? "request"
                     : (epoch.outcome == AdaptiveSizingOutcome::kClosed
                            ? "closed"
                            : "invalid"))
        << ", \"reason\": " << quote(epoch.reason)
        << ", \"added_components\": " << epoch.added_components
        << ", \"added_nets\": " << epoch.added_nets
        << ", \"rewired_nets\": " << epoch.rewired_nets
        << ", \"samples\": [";
    for (size_t sample_index = 0; sample_index < epoch.samples.size();
         ++sample_index) {
      const AdaptiveDelayLineSample &sample = epoch.samples[sample_index];
      out << (sample_index == 0 ? "" : ",") << "{\"site\":"
          << quote(sample.site) << ",\"pairs\":" << sample.pairs
          << ",\"binding_slack_ps\":" << sample.binding_slack_ps
          << ",\"attributed\":" << (sample.attributed ? "true" : "false")
          << ",\"binding_identity\":" << quote(sample.binding_identity)
          << "}";
    }
    out << "], \"decisions\": [";
    for (size_t decision_index = 0; decision_index < epoch.decisions.size();
         ++decision_index) {
      const AdaptiveSiteDecision &site = epoch.decisions[decision_index];
      out << (decision_index == 0 ? "" : ",") << "{\"site\":"
          << quote(site.site) << ",\"current_pairs\":"
          << site.current_pairs << ",\"requested_pairs\":"
          << site.requested_pairs << ",\"binding_slack_ps\":"
          << site.binding_slack_ps << ",\"gain_ps_per_pair\":"
          << site.measured_gain_ps_per_pair << ",\"probe\":"
          << (site.is_probe ? "true" : "false") << "}";
    }
    out << "]}";
  }
  out << (manifest_adaptive_epochs_.empty() ? "" : "\n  ") << "],\n";

  // The final parameter map: what every registered site actually ended at.
  // This is what a live run and a clean rebuild must share, and it is the only
  // sense in which the two are required to agree.
  out << "  \"final_sites\": [";
  bool first = true;
  for (const DelayLineSpreadRequest &request : delay_line_spread_requests_) {
    DelayLineChain chain;
    std::string error_message;
    const bool readable = BuildDelayLineChain(circuit_, request.name_prefix,
                                              &chain, &error_message);
    out << (first ? "\n" : ",\n") << "    {\"site\": "
        << quote(request.name_prefix) << ", \"pairs\": "
        << (readable ? static_cast<int>(chain.nodes.size()) / 2 : -1)
        << ", \"elements\": "
        << (readable ? static_cast<int>(chain.nodes.size()) : -1)
        << ", \"binding_slack_ps\": " << request.binding_slack
        << ", \"binding_constraint_id\": " << request.binding_constraint_id
        << ", \"attributed\": "
        << (request.has_binding_slack ? "true" : "false") << "}";
    first = false;
  }
  out << (first ? "" : "\n  ") << "],\n";

  unsigned long long identity_digest = 0;
  std::size_t identity_count = 0;
  WriteCurrentTimingConstraintIdentities(file_name + ".identities.json", {},
                                         &identity_digest, &identity_count);
  out << "  \"identities\": {\"count\": " << identity_count
      << ", \"digest\": \"" << identity_digest << "\"},\n";
  out << "  \"margin_ps\": " << topology_sizing_margin_ps_ << "\n";
  out << "}\n";

  std::ofstream handle(file_name);
  if (!handle.is_open()) {
    LOG(error) << "Cannot write the sizing manifest to " << file_name << "\n";
    return false;
  }
  handle << out.str();
  LOG(info) << "SIZING_MANIFEST written to " << file_name << "\n";
  return true;
#else
  (void)file_name;
  return false;
#endif
}

void Dali::Close() { CloseLogging(); }

void Dali::MaybeExportToLEF(std::string const &input_lef_file_full_name,
                            std::string const &output_lef_name) {
  if (!enable_end_cap_cell_) {
    return;
  }
  circuit_.SaveLefFile(input_lef_file_full_name, output_lef_name);
}

void Dali::ExportToDEF(std::string const &input_def_file_full_name,
                       std::string const &output_def_name) {
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
    std::string const &engine, std::string const &script_name) {
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

  DaliExpects(IsExecutableExisting(engine), "Cannot find the given engine");
  return out_def;
}

void Dali::ExportOrdinaryComponentsToPhyDB() {
  double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
  double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();
  for (auto &component : circuit_.Components()) {
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

    phydb::Component *comp_ptr = phy_db_ptr_->GetComponentPtr(comp_name);
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
  for (auto &component : circuit_.design().WellTaps()) {
    std::string comp_name = component.Name();
    std::string macro_name = component.MacroPtr()->Name();
    int lx =
        (int)(component.LLX() * factor_x) + circuit_.design().DieAreaOffsetX();
    int ly =
        (int)(component.LLY() * factor_y) + circuit_.design().DieAreaOffsetY();
    auto place_status = phydb::PlaceStatus(component.Status());
    auto orient = phydb::CompOrient(component.Orient());

    auto *phydb_macro_ptr = phy_db_ptr_->GetMacroPtr(macro_name);
    DaliExpects(phydb_macro_ptr != nullptr,
                "Cannot find " << macro_name << " in PhyDB?!");
    phy_db_ptr_->AddComponent(comp_name, phydb_macro_ptr, place_status, lx, ly,
                              orient, phydb::CompSource::USER);
  }
}

void Dali::ExportFillerCellsToPhyDB() {
  double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
  double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();
  for (auto &component : circuit_.design().Fillers()) {
    std::string comp_name = component.Name();
    std::string macro_name = component.MacroPtr()->Name();
    int lx =
        (int)(component.LLX() * factor_x) + circuit_.design().DieAreaOffsetX();
    int ly =
        (int)(component.LLY() * factor_y) + circuit_.design().DieAreaOffsetY();
    auto place_status = phydb::PlaceStatus(component.Status());
    auto orient = phydb::CompOrient(component.Orient());

    auto *phydb_macro_ptr = phy_db_ptr_->GetMacroPtr(macro_name);
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

/** Write placed I/O pins back to PhyDB. */
void Dali::ExportIoPinsToPhyDB() {
  DaliExpects(!circuit_.Metals().empty(),
              "Need metal layer info to generate PIN location\n");
  for (auto &iopin : circuit_.design().IoPins()) {
    if (!iopin.IsPrePlaced() && iopin.IsPlaced()) {
      DaliExpects(iopin.LayerPtr() != nullptr,
                  "IOPIN metal layer not set? Cannot export it to PhyDB");
      std::string metal_name = iopin.LayerPtr()->Name();
      std::string iopin_name = iopin.Name();
      DaliExpects(phy_db_ptr_->IsIoPinExisting(iopin_name),
                  "IOPIN not in PhyDB? " << iopin_name);
      phydb::IOPin *phydb_iopin = phy_db_ptr_->GetIoPinPtr(iopin_name);
      auto &rect = iopin.Shape();
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
      } else if (iopin.Y() == circuit_.design().RegionTop()) {
        pin_orient = phydb::CompOrient::S;
      } else {
        // Interior area-array pin, not on any edge: face up.
        pin_orient = phydb::CompOrient::N;
      }

      phydb_iopin->SetPlacement(PlaceStatusDali2PhyDB(iopin.Status()), pin_x,
                                pin_y, pin_orient);
    }
  }
}

/**
 * Write the gridded rows back to PhyDB as DEF rows, in database units.
 *
 * Each gridded row becomes a DEF row so downstream tools see the row structure
 * legalization produced.
 */
void Dali::ExportMiniRowsToPhyDB() {
  double factor_x = circuit_.DistanceMicrons() * circuit_.GridValueX();
  double factor_y = circuit_.DistanceMicrons() * circuit_.GridValueY();

  int counter = 0;
  for (auto &col : well_legalizer_.col_list_) {
    for (auto &strip : col.stripe_list_) {
      std::string column_name = "column" + std::to_string(counter++);
      std::string bot_signal_;
      if (strip.is_first_row_orient_N_) {
        bot_signal_ = "GND";
      } else {
        bot_signal_ = "Vdd";
      }
      phydb::ClusterCol *p_col =
          phy_db_ptr_->AddClusterCol(column_name, bot_signal_);

      int col_lx =
          (int)(strip.LLX() * factor_x) + circuit_.design().DieAreaOffsetX();
      int col_ux =
          (int)(strip.URX() * factor_x) + circuit_.design().DieAreaOffsetX();
      p_col->SetXRange(col_lx, col_ux);

      if (strip.is_bottom_up_) {
        for (auto &cluster : strip.gridded_rows_) {
          int row_ly = (int)(cluster.LLY() * factor_y) +
                       circuit_.design().DieAreaOffsetY();
          int row_uy = (int)(cluster.URY() * factor_y) +
                       circuit_.design().DieAreaOffsetY();
          p_col->AddRow(row_ly, row_uy);
        }
      } else {
        int sz = static_cast<int>(strip.gridded_rows_.size());
        for (int j = sz - 1; j >= 0; --j) {
          auto &cluster = strip.gridded_rows_[j];
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
  well_legalizer_.ExportWellToPhyDB(phy_db_ptr_, well_emit_mode_);
}

} // namespace dali
