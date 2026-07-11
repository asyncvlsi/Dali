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

#include "global_placer.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "dali/common/logging.h"
#include "dali/common/placement_metrics.h"

namespace dali {

/****
 * @brief Set the maximum number of iterations.
 *
 * @param max_iter: maximum number of iterations.
 */
void GlobalPlacer::SetMaxIteration(int max_iter) {
  DaliExpects(max_iter >= 0, "negative number of iterations?");
  max_iter_ = max_iter;
}

void GlobalPlacer::SetMinIteration(int min_iter) {
  DaliExpects(min_iter >= 0, "negative number of iterations?");
  min_iter_ = min_iter;
}

/****
 * @brief Set an internal boolean variable to save or not save intermediate
 * results.
 *
 * @param should_save_intermediate_result : if true, intermediate results will
 * be saved; otherwise, not.
 */
void GlobalPlacer::SetShouldSaveIntermediateResult(
    bool should_save_intermediate_result) {
  should_save_intermediate_result_ = should_save_intermediate_result;
}

void GlobalPlacer::SetSnapshotCallback(SnapshotCallback snapshot_callback) {
  snapshot_callback_ = std::move(snapshot_callback);
}

void GlobalPlacer::SetInitializerType(
    PlacementInitializerType initializer_type) {
  initializer_type_ = initializer_type;
}

void GlobalPlacer::SetAnchorSchedule(GlobalAnchorSchedule schedule) {
  anchor_schedule_ = schedule;
}

void GlobalPlacer::SetGridSchedule(GlobalGridSchedule schedule) {
  grid_schedule_ = schedule;
}

void GlobalPlacer::SetLalExpansionMode(GlobalLalExpansionMode mode) {
  lal_expansion_mode_ = mode;
}

void GlobalPlacer::SetLalHotspotMode(GlobalLalHotspotMode mode) {
  lal_hotspot_mode_ = mode;
}

void GlobalPlacer::SetLalAffineScalingWeight(double weight) {
  DaliExpects(weight >= 0.0 && weight <= 1.0,
              "LAL affine scaling weight must be in [0, 1]");
  lal_affine_scaling_weight_ = weight;
}

void GlobalPlacer::SetLalMacroBoundaryMode(GlobalLalMacroBoundaryMode mode) {
  lal_macro_boundary_mode_ = mode;
}

void GlobalPlacer::SetCapacityModel(
    std::shared_ptr<const PlacementCapacityModel> capacity_model) {
  DaliExpects(capacity_model != nullptr,
              "Global placer capacity model cannot be null");
  capacity_model_ = std::move(capacity_model);
}

/****
 * @brief Load a configuration file for this placer.
 *
 * @param config_file: name of the configuration file.
 */
void GlobalPlacer::LoadConf(std::string const& config_file) {
  config_read(config_file.c_str());
  DaliFatal("This function is not fully implemented");
}

void GlobalPlacer::InitializePlacementEngines() {
  optimizer_ =
      std::make_unique<BoundToBoundHpwlOptimizer>(ckt_ptr_, num_threads_);
  optimizer_->SetAnchorSchedule(anchor_schedule_);
  optimizer_->SetShouldSaveIntermediateResult(should_save_intermediate_result_);
  optimizer_->Initialize();

  auto look_ahead_spreader = std::make_unique<LookAheadSpreader>(
      ckt_ptr_, capacity_model_);
  look_ahead_spreader->SetGridSchedule(grid_schedule_);
  look_ahead_spreader->SetExpansionMode(lal_expansion_mode_);
  look_ahead_spreader->SetHotspotMode(lal_hotspot_mode_);
  look_ahead_spreader->SetAffineScalingWeight(lal_affine_scaling_weight_);
  look_ahead_spreader->SetMacroBoundaryMode(lal_macro_boundary_mode_);
  look_ahead_spreader->SetShouldSaveIntermediateResult(
      should_save_intermediate_result_);
  look_ahead_spreader->Initialize(PlacementDensity());
  spreader_ = std::move(look_ahead_spreader);
}

void GlobalPlacer::ClosePlacementEngines() {
  if (optimizer_) {
    optimizer_->Close();
    optimizer_.reset();
  }
  if (spreader_) {
    spreader_->Close();
    spreader_.reset();
  }
}

/****
 * @brief This function is a wrapper to report HPWL before and after component
 * location initialization using different methods.
 *
 * @param mode: the method to initialize the component locations
 * @param std_dev: the standard deviation if normal distribution is used
 */
void GlobalPlacer::InitializeComponentLocation() {
  if (initializer_type_ == PlacementInitializerType::kKeep) {
    LOG(info) << "  Component location initialization:\n"
              << "    Preserve input component locations\n"
              << "    HPWL before, " << WeightedHPWL() << "\n"
              << "    HPWL after, " << WeightedHPWL() << "\n";
    RecordPlacementMetric("initialization.before", WeightedHPWL());
    RecordPlacementMetric("initialization.after", WeightedHPWL());
    return;
  }

  std::unique_ptr<PlacementInitializer> initializer(nullptr);
  switch (initializer_type_) {
    case PlacementInitializerType::kUniform: {
      initializer = std::make_unique<UniformInitializer>(ckt_ptr_, 1);
      break;
    }
    case PlacementInitializerType::kGaussian: {
      initializer = std::make_unique<GaussianInitializer>(ckt_ptr_, 1);
      break;
    }
    case PlacementInitializerType::kMonteCarlo: {
      initializer = std::make_unique<MonteCarloInitializer>(ckt_ptr_, 1);
      break;
    }
    case PlacementInitializerType::kDensityAware: {
      initializer = std::make_unique<DensityAwareInitializer>(ckt_ptr_, 1);
      break;
    }
    default: {
      DaliFatal("Unknown random initializer type");
    }
  }
  initializer->SetShouldSaveIntermediateResult(
      should_save_intermediate_result_);
  initializer->InitializeLocations();
}

void GlobalPlacer::PreparePlacement() {
  SanityCheck();
  EmitSnapshot("initialization.before", "Before Location Initialization",
               "initialization", -1);
  InitializeComponentLocation();
  EmitSnapshot("initialization.after", "After Location Initialization",
               "initialization", -1);
  InitializePlacementEngines();
}

void GlobalPlacer::RunPlacementIterations() {
  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    optimizer_->SetIteration(cur_iter_);
    optimizer_->OptimizeHpwl();
    EmitIterationSnapshot("lower_bound", "Lower Bound", "lower_bound");
    spreader_->SetIteration(cur_iter_);
    spreader_->Spread();
    EmitIterationSnapshot("upper_bound", "Upper Bound", "upper_bound");
    PrintHpwl();
    if (IsPlacementConverged()) break;
  }
}

void GlobalPlacer::EmitIterationSnapshot(const std::string& id_suffix,
                                         const std::string& label_suffix,
                                         const std::string& subgroup) {
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "iter_%03d.%s", cur_iter_,
           id_suffix.c_str());
  std::string id = buffer;
  snprintf(buffer, sizeof(buffer), "Iteration %d %s", cur_iter_,
           label_suffix.c_str());
  std::string label = buffer;
  EmitSnapshot(id, label, subgroup, cur_iter_);
}

void GlobalPlacer::EmitSnapshot(const std::string& id, const std::string& label,
                                const std::string& subgroup, int iteration) {
  if (!snapshot_callback_) return;
  snapshot_callback_(id, label, subgroup, iteration);
}

void GlobalPlacer::FinalizePlacement() {
  UpdateMovableComponentPlacementStatus();
  RecordPlacementMetric("global_placement", WeightedHPWL());
}

/****
 * @brief The entry point of global placement.
 * @return A boolean value indicating whether global placement can be
 * successfully performed.
 */
bool GlobalPlacer::StartPlacement() {
  if (IsComponentListOrNetListEmpty()) return true;
  PrintStartStatement("global placement");

  PreparePlacement();
  RunPlacementIterations();
  FinalizePlacement();

  PrintEndStatement("Global placement", true);
  ClosePlacementEngines();
  return true;
}

/****
 * @brief Check if component_list is empty or net_list is empty. If either of
 * them is empty, return true, so that the global placement can be skipped.
 * @return a boolean value indicate whether component_list or net_list is empty
 * or not.
 */
bool GlobalPlacer::IsComponentListOrNetListEmpty() const {
  if (ckt_ptr_->Components().empty()) {
    LOG(info)
        << "Empty component list, nothing to place! Skip global placement!\n";
    return true;
  }
  if (ckt_ptr_->Nets().empty()) {
    LOG(info)
        << "Empty net list, nothing to optimize! Skip global placement!\n";
    return true;
  }
  return false;
}

bool GlobalPlacer::IsPositive(double value) { return value > 1e-10; }

double GlobalPlacer::RelativeImprovement(double old_value, double new_value) {
  DaliExpects(old_value >= 0 && new_value >= 0,
              "negative HPWL values are not supported");
  if (!IsPositive(old_value)) return 0.0;
  return (old_value - new_value) / old_value;
}

bool GlobalPlacer::HasUpperBoundHpwlStalled(
    const std::vector<double>& upper_bound_hpwl) const {
  int series_size = static_cast<int>(upper_bound_hpwl.size());
  if (series_size <= upper_bound_improvement_patience_) return false;

  double best_upper_bound_hpwl = upper_bound_hpwl.front();
  int last_meaningful_improvement_iter = 0;
  for (int i = 1; i < series_size; ++i) {
    double improvement =
        RelativeImprovement(best_upper_bound_hpwl, upper_bound_hpwl[i]);
    if (improvement >= upper_bound_min_improvement_) {
      best_upper_bound_hpwl = upper_bound_hpwl[i];
      last_meaningful_improvement_iter = i;
    }
  }
  return series_size - 1 - last_meaningful_improvement_iter >=
         upper_bound_improvement_patience_;
}

/****
 * @brief Returns true or false indicating the convergence of the global
 * placement.
 *
 * Stopping criteria (SimPL, option 1):
 *    the current lower/upper gap is small and the best upper-bound HPWL has
 *    not improved meaningfully for several iterations
 * Stopping criteria (POLAR, option 2):
 *    the gap between lower bound wire-length and upper bound wire-length is
 *    less than 8%
 * ****/
bool GlobalPlacer::IsPlacementConverged() {
  if (cur_iter_ + 1 < min_iter_) return false;

  bool res;
  auto& lower_bound_hpwl = optimizer_->GetHpwls();
  auto& upper_bound_hpwl = spreader_->Hpwls();
  if (convergence_criteria_ == 1) {
    if (lower_bound_hpwl.empty() || upper_bound_hpwl.empty()) {
      res = false;
    } else {
      double lower_bound = lower_bound_hpwl.back();
      double upper_bound = upper_bound_hpwl.back();
      bool small_gap =
          !IsPositive(lower_bound) ||
          (upper_bound / lower_bound - 1 < polar_converge_criterion_);
      res = small_gap && HasUpperBoundHpwlStalled(upper_bound_hpwl);
    }
  } else if (convergence_criteria_ == 2) {
    if (lower_bound_hpwl.empty()) {
      res = false;
    } else {
      double lower_bound = lower_bound_hpwl.back();
      double upper_bound = upper_bound_hpwl.back();
      res = (lower_bound > 1e-10) && (lower_bound < upper_bound) &&
            (upper_bound / lower_bound - 1 < polar_converge_criterion_);
    }
  } else {
    DaliExpects(false, "Unknown Convergence Criteria!");
  }

  return res;
}

/****
 * @brief A helper function to format and print HPWL in each iteration.
 */
void GlobalPlacer::PrintHpwl() const {
  if (optimizer_->GetHpwls().empty() || spreader_->Hpwls().empty()) return;
  double lo_hpwl = optimizer_->GetHpwls().back();
  double hi_hpwl = spreader_->Hpwls().back();
  double hpwl_gap = hi_hpwl - lo_hpwl;
  double hpwl_gap_percent = lo_hpwl <= 1e-10 ? 0 : hpwl_gap / lo_hpwl * 100.0;
  size_t buffer_size = 1024;
  std::string buffer(buffer_size, '\0');
  int written_length =
      snprintf(&buffer[0], buffer_size,
               "  iter-%-3d: lower %.4e, upper %.4e, gap %.4e (%.2f%%)\n",
               cur_iter_, lo_hpwl, hi_hpwl, hpwl_gap, hpwl_gap_percent);
  buffer.resize(written_length);
  LOG(info) << buffer;
  LOG(debug) << cur_iter_ << "-th iteration completed\n";
}

/****
 * @brief Printf the summary of global placement.
 */
void GlobalPlacer::PrintEndStatement(std::string const& name_of_process,
                                     bool is_success) {
  LOG(debug) << "  Iterative look-ahead legalization complete\n";
  LOG(debug) << "  Total number of iteration: " << cur_iter_ + 1 << "\n";
  LOG(debug) << "  Lower bound: " << optimizer_->GetHpwls() << "\n";
  LOG(debug) << "  Upper bound: " << spreader_->Hpwls() << "\n";
  LOG(debug) << "cg time: " << optimizer_->GetTime()
             << "s, lal time: " << spreader_->GetTime() << "s\n";
  Placer::PrintEndStatement(name_of_process, is_success);
}

}  // namespace dali
