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
#include <cmath>
#include <memory>
#include <utility>

#include "dali/common/act_config.h"
#include "dali/common/logging.h"
#include "dali/common/placement_metrics.h"

namespace dali {

/** Return a concise log name for a physical-refinement feedback policy. */
const char* RefinementFeedbackModeName(GlobalRefinementFeedbackMode mode) {
  switch (mode) {
    case GlobalRefinementFeedbackMode::kFull:
      return "full";
    case GlobalRefinementFeedbackMode::kXOnly:
      return "x_only";
    case GlobalRefinementFeedbackMode::kYOnly:
      return "y_only";
    case GlobalRefinementFeedbackMode::kYRowScale:
      return "y_row_scale";
    case GlobalRefinementFeedbackMode::kYRowHpwl:
      return "y_row_hpwl";
    case GlobalRefinementFeedbackMode::kYRowTransactional:
      return "y_row_transactional";
    case GlobalRefinementFeedbackMode::kYRowTransactionalPositive:
      return "y_row_transactional_positive";
    case GlobalRefinementFeedbackMode::kYRowTransactionalConsistent:
      return "y_row_transactional_consistent";
    case GlobalRefinementFeedbackMode::kYRowTransactionalCoherent:
      return "y_row_transactional_coherent";
    case GlobalRefinementFeedbackMode::kNone:
      return "none";
  }
  return "unknown";
}

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

void GlobalPlacer::SetSnapshotCallback(SnapshotCallback snapshot_callback) {
  snapshot_callback_ = std::move(snapshot_callback);
}

void GlobalPlacer::SetInitializerType(
    PlacementInitializerType initializer_type) {
  initializer_type_ = initializer_type;
}

void GlobalPlacer::SetNetIgnoreThreshold(int net_ignore_threshold) {
  DaliExpects(net_ignore_threshold > 1,
              "Net ignore threshold must be greater than one");
  net_ignore_threshold_ = net_ignore_threshold;
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
    std::shared_ptr<PlacementCapacityModel> capacity_model) {
  DaliExpects(capacity_model != nullptr,
              "Global placer capacity model cannot be null");
  capacity_model_ = std::move(capacity_model);
}

void GlobalPlacer::SetUpperBoundRefiner(
    std::unique_ptr<GlobalUpperBoundRefiner> upper_bound_refiner,
    int warmup_iteration, int interval) {
  DaliExpects(upper_bound_refiner != nullptr,
              "Global upper-bound refiner cannot be null");
  DaliExpects(warmup_iteration >= 0,
              "Upper-bound refiner warmup cannot be negative");
  DaliExpects(interval > 0, "Upper-bound refiner interval must be positive");
  upper_bound_refiner_ = std::move(upper_bound_refiner);
  upper_bound_refiner_warmup_ = warmup_iteration;
  upper_bound_refiner_interval_ = interval;
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
  optimizer_->SetNetIgnoreThreshold(net_ignore_threshold_);
  optimizer_->Initialize();

  auto look_ahead_spreader =
      std::make_unique<LookAheadSpreader>(ckt_ptr_, capacity_model_);
  look_ahead_spreader->SetExpansionMode(lal_expansion_mode_);
  look_ahead_spreader->SetHotspotMode(lal_hotspot_mode_);
  look_ahead_spreader->SetAffineScalingWeight(lal_affine_scaling_weight_);
  look_ahead_spreader->SetMacroBoundaryMode(lal_macro_boundary_mode_);
  look_ahead_spreader->Initialize(PlacementDensity());
  spreader_ = std::move(look_ahead_spreader);
  auto pressure_model =
      std::dynamic_pointer_cast<LegalizationPressureCapacityModel>(
          capacity_model_);
  if (pressure_model != nullptr) {
    pressure_model->SetDemandMultipliers(
        std::vector<double>(ckt_ptr_->Components().size(), 1.0));
  }
  accepted_upper_bound_hpwl_.clear();
  accepted_upper_bound_hpwl_x_.clear();
  accepted_upper_bound_hpwl_y_.clear();
  best_upper_bound_placement_.clear();
  previous_feedback_checkpoint_.clear();
  best_upper_bound_hpwl_ = std::numeric_limits<double>::max();
  current_upper_bound_is_physical_ = upper_bound_refiner_ == nullptr;
  if (upper_bound_refiner_) {
    upper_bound_refiner_->Initialize(PlacementDensity());
  }
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
  if (upper_bound_refiner_) {
    upper_bound_refiner_->Close();
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
    RecordPlacementHpwlMetrics("initialization.before", *ckt_ptr_);
    RecordPlacementHpwlMetrics("initialization.after", *ckt_ptr_);
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
    double accepted_hpwl = spreader_->Hpwls().back();
    double accepted_hpwl_x = spreader_->HpwlsX().back();
    double accepted_hpwl_y = spreader_->HpwlsY().back();
    bool accepted_physical_refinement = false;
    bool anchor_all_components = true;
    std::vector<int> selective_anchor_component_ids;
    std::vector<std::vector<int>> refined_component_rows;
    current_upper_bound_is_physical_ = upper_bound_refiner_ == nullptr;
    std::vector<ComponentLocation> placement_before_refinement;
    if (ShouldRefineUpperBound()) {
      placement_before_refinement = SaveCurrentPlacement();
      GlobalUpperBoundRefinement refinement =
          upper_bound_refiner_->Refine(cur_iter_);
      UpdateLegalizationPressure(refinement);
      if (RollbackRefinementFeedbackIfRequested(refinement)) {
        accepted_hpwl = WeightedHPWL();
        accepted_hpwl_x = ckt_ptr_->WeightedHPWLX();
        accepted_hpwl_y = ckt_ptr_->WeightedHPWLY();
      } else if (refinement.feasible) {
        accepted_hpwl = refinement.hpwl;
        accepted_hpwl_x = ckt_ptr_->WeightedHPWLX();
        accepted_hpwl_y = ckt_ptr_->WeightedHPWLY();
        accepted_physical_refinement = true;
        anchor_all_components = refinement.anchor_all_components;
        selective_anchor_component_ids =
            std::move(refinement.anchor_component_ids);
        refined_component_rows = std::move(refinement.component_rows);
        current_upper_bound_is_physical_ = true;
        LogRefinementDisplacement(placement_before_refinement);
      } else {
        previous_feedback_checkpoint_.clear();
      }
    }
    accepted_upper_bound_hpwl_.push_back(accepted_hpwl);
    accepted_upper_bound_hpwl_x_.push_back(accepted_hpwl_x);
    accepted_upper_bound_hpwl_y_.push_back(accepted_hpwl_y);
    if (accepted_physical_refinement) {
      UpdateBestUpperBoundPlacement(accepted_hpwl);
    }
    EmitIterationSnapshot("upper_bound", "Upper Bound", "upper_bound");
    if (accepted_physical_refinement) {
      ApplyRefinedAnchorFeedback(
          placement_before_refinement, anchor_all_components,
          selective_anchor_component_ids, refined_component_rows);
      previous_feedback_checkpoint_ = std::move(placement_before_refinement);
    }
    PrintHpwl();
    if (IsPlacementConverged()) break;
  }
}

void GlobalPlacer::UpdateLegalizationPressure(
    const GlobalUpperBoundRefinement& refinement) {
  auto pressure_model =
      std::dynamic_pointer_cast<LegalizationPressureCapacityModel>(
          capacity_model_);
  if (pressure_model == nullptr) return;

  std::vector<double> demand_multipliers(ckt_ptr_->Components().size(), 1.0);
  int pressured_component_count = 0;
  double maximum_multiplier = 1.0;
  for (const GlobalUpperBoundViolation& violation :
       refinement.initial_violations) {
    double region_height = violation.uy - violation.ly;
    if (region_height <= 0.0 || violation.overflow <= 0.0) continue;
    double multiplier = 1.0 + violation.overflow / region_height;
    maximum_multiplier = std::max(maximum_multiplier, multiplier);
    for (int component_id : violation.component_ids) {
      DaliExpects(
          component_id >= 0 &&
              component_id < static_cast<int>(demand_multipliers.size()),
          "Legalization pressure contains an invalid component id");
      if (demand_multipliers[component_id] == 1.0) {
        ++pressured_component_count;
      }
      demand_multipliers[component_id] =
          std::max(demand_multipliers[component_id], multiplier);
    }
  }
  pressure_model->SetDemandMultipliers(std::move(demand_multipliers));
  LOG(info) << "    legalization capacity pressure: "
            << pressured_component_count << " components, max multiplier "
            << maximum_multiplier << "\n";
  RecordPlacementMetric("global_placement.pressure.last_component_count",
                        pressured_component_count);
  RecordPlacementMetric("global_placement.pressure.last_max_multiplier",
                        maximum_multiplier);
}

void GlobalPlacer::ApplyRefinedAnchorFeedback(
    const std::vector<ComponentLocation>& placement_before_refinement,
    bool anchor_all_components, const std::vector<int>& component_ids,
    const std::vector<std::vector<int>>& component_rows) {
  DaliExpects(
      placement_before_refinement.size() == ckt_ptr_->Components().size(),
      "Cannot apply refinement feedback: component count changed");

  std::vector<bool> selected_components(ckt_ptr_->Components().size(),
                                        anchor_all_components);
  for (int component_id : component_ids) {
    DaliExpects(
        component_id >= 0 &&
            component_id < static_cast<int>(ckt_ptr_->Components().size()),
        "Refined anchor contains an invalid component id");
    selected_components[component_id] = true;
  }

  const bool keep_x =
      refinement_feedback_mode_ == GlobalRefinementFeedbackMode::kFull ||
      refinement_feedback_mode_ == GlobalRefinementFeedbackMode::kXOnly;
  const bool keep_y =
      refinement_feedback_mode_ == GlobalRefinementFeedbackMode::kFull ||
      refinement_feedback_mode_ == GlobalRefinementFeedbackMode::kYOnly ||
      refinement_feedback_mode_ == GlobalRefinementFeedbackMode::kYRowScale ||
      refinement_feedback_mode_ == GlobalRefinementFeedbackMode::kYRowHpwl ||
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactional ||
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactionalPositive ||
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactionalConsistent ||
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactionalCoherent;
  const bool require_row_scale_y =
      refinement_feedback_mode_ == GlobalRefinementFeedbackMode::kYRowScale ||
      refinement_feedback_mode_ == GlobalRefinementFeedbackMode::kYRowHpwl ||
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactional ||
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactionalPositive ||
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactionalConsistent ||
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactionalCoherent;
  const bool require_non_worsening_hpwl =
      refinement_feedback_mode_ == GlobalRefinementFeedbackMode::kYRowHpwl;
  const bool use_transactional_y =
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactional ||
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactionalPositive ||
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactionalConsistent ||
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactionalCoherent;
  const bool require_positive_transactional_gain =
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactionalPositive ||
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactionalConsistent ||
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactionalCoherent;
  const bool require_positive_baseline_gain =
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactionalConsistent ||
      refinement_feedback_mode_ ==
          GlobalRefinementFeedbackMode::kYRowTransactionalCoherent;
  const bool use_relative_y_constraints =
      refinement_feedback_mode_ ==
      GlobalRefinementFeedbackMode::kYRowTransactionalCoherent;

  // Classify every candidate against the same complete refined placement.
  // Applying restorations in a second pass keeps this filter independent of
  // component traversal order.
  std::vector<bool> keep_component_y(ckt_ptr_->Components().size(), false);
  int selected_movable_count = 0;
  for (size_t i = 0; i < ckt_ptr_->Components().size(); ++i) {
    Component& component = ckt_ptr_->Components()[i];
    const bool selected = selected_components[i];
    keep_component_y[i] = selected && keep_y;
    // A move shorter than the component height is treated as local packing
    // noise rather than evidence that the analytical placement chose a bad
    // row. This preserves only the discrete part of rough legalization.
    if (keep_component_y[i] && require_row_scale_y &&
        std::fabs(component.LLY() - placement_before_refinement[i].ly) <
            component.Height()) {
      keep_component_y[i] = false;
    }
    if (keep_component_y[i] && require_non_worsening_hpwl &&
        !IsRefinedYLocallyNonWorsening(component,
                                       placement_before_refinement[i].ly)) {
      keep_component_y[i] = false;
    }
  }
  if (use_transactional_y) {
    keep_component_y = SelectTransactionalYFeedback(
        keep_component_y, placement_before_refinement,
        require_positive_transactional_gain, require_positive_baseline_gain);
  }

  if (use_relative_y_constraints) {
    std::vector<RelativeYConstraint> constraints =
        BuildRelativeYConstraints(component_rows, keep_component_y);
    LOG(info) << "    relative Y feedback: " << constraints.size()
              << " adjacent component pairs\n";
    if (optimizer_ != nullptr) {
      optimizer_->SetRelativeYConstraints(std::move(constraints));
    }
  }

  for (size_t i = 0; i < ckt_ptr_->Components().size(); ++i) {
    Component& component = ckt_ptr_->Components()[i];
    const bool selected = selected_components[i];
    if (component.IsMovable() &&
        ((selected && keep_x) || keep_component_y[i])) {
      ++selected_movable_count;
    }
    const double lx = selected && keep_x ? component.LLX()
                                         : placement_before_refinement[i].lx;
    const double ly = keep_component_y[i] ? component.LLY()
                                          : placement_before_refinement[i].ly;
    component.SetLowerLeft(lx, ly);
  }
  LOG(info) << "    legalization feedback: "
            << RefinementFeedbackModeName(refinement_feedback_mode_) << ", "
            << selected_movable_count << " selected components\n";
}

std::vector<RelativeYConstraint> GlobalPlacer::BuildRelativeYConstraints(
    const std::vector<std::vector<int>>& component_rows,
    const std::vector<bool>& accepted_components) const {
  DaliExpects(accepted_components.size() == ckt_ptr_->Components().size(),
              "Relative Y feedback count does not match component count");
  std::vector<RelativeYConstraint> constraints;
  for (const std::vector<int>& component_row : component_rows) {
    for (size_t i = 1; i < component_row.size(); ++i) {
      const int first = component_row[i - 1];
      const int second = component_row[i];
      DaliExpects(first >= 0 &&
                      first < static_cast<int>(accepted_components.size()) &&
                      second >= 0 &&
                      second < static_cast<int>(accepted_components.size()),
                  "Physical row contains an invalid component id");
      if (!accepted_components[first] || !accepted_components[second]) {
        continue;
      }
      constraints.push_back({first, second,
                             ckt_ptr_->Components()[first].LLY() -
                                 ckt_ptr_->Components()[second].LLY()});
    }
  }
  return constraints;
}

double GlobalPlacer::ConnectedNetWeightedHpwlY(
    const Component& component) const {
  double hpwl = 0.0;
  for (int net_id : component.NetList()) {
    Net& net = ckt_ptr_->Nets()[net_id];
    if (net.PinCnt() <= 1 ||
        net.PinCnt() >= static_cast<size_t>(net_ignore_threshold_)) {
      continue;
    }
    hpwl += net.WeightedHPWLY() * ckt_ptr_->GridValueY();
  }
  return hpwl;
}

bool GlobalPlacer::IsRefinedYLocallyNonWorsening(Component& component,
                                                 double analytical_y) const {
  const double refined_y = component.LLY();
  const double refined_hpwl = ConnectedNetWeightedHpwlY(component);
  component.SetLLY(analytical_y);
  const double analytical_hpwl = ConnectedNetWeightedHpwlY(component);
  component.SetLLY(refined_y);
  return refined_hpwl <= analytical_hpwl;
}

std::vector<bool> GlobalPlacer::SelectTransactionalYFeedback(
    const std::vector<bool>& candidates,
    const std::vector<ComponentLocation>& analytical_placement,
    bool require_positive_gain, bool require_positive_baseline_gain) const {
  DaliExpects(candidates.size() == ckt_ptr_->Components().size(),
              "Y feedback candidate count does not match component count");
  DaliExpects(analytical_placement.size() == ckt_ptr_->Components().size(),
              "Analytical placement count does not match component count");

  struct Candidate {
    size_t component_index = 0;
    double refined_y = 0.0;
    double estimated_gain = 0.0;
  };

  std::vector<double> refined_y(ckt_ptr_->Components().size(), 0.0);
  for (size_t i = 0; i < ckt_ptr_->Components().size(); ++i) {
    Component& component = ckt_ptr_->Components()[i];
    refined_y[i] = component.LLY();
    component.SetLLY(analytical_placement[i].ly);
  }

  const size_t candidate_count = static_cast<size_t>(
      std::count(candidates.begin(), candidates.end(), true));
  std::vector<Candidate> ordered_candidates;
  ordered_candidates.reserve(candidate_count);
  for (size_t i = 0; i < ckt_ptr_->Components().size(); ++i) {
    if (!candidates[i]) continue;
    Component& component = ckt_ptr_->Components()[i];
    const double hpwl_before = ConnectedNetWeightedHpwlY(component);
    component.SetLLY(refined_y[i]);
    const double hpwl_after = ConnectedNetWeightedHpwlY(component);
    component.SetLLY(analytical_placement[i].ly);
    ordered_candidates.push_back({i, refined_y[i], hpwl_before - hpwl_after});
  }
  std::sort(ordered_candidates.begin(), ordered_candidates.end(),
            [](const Candidate& lhs, const Candidate& rhs) {
              if (lhs.estimated_gain != rhs.estimated_gain) {
                return lhs.estimated_gain > rhs.estimated_gain;
              }
              return lhs.component_index < rhs.component_index;
            });

  std::vector<bool> accepted(candidates.size(), false);
  double hpwl_improvement = 0.0;
  size_t neutral_rejection_count = 0;
  size_t baseline_rejection_count = 0;
  for (const Candidate& candidate : ordered_candidates) {
    if (require_positive_baseline_gain && candidate.estimated_gain <= 0.0) {
      ++baseline_rejection_count;
      continue;
    }
    Component& component = ckt_ptr_->Components()[candidate.component_index];
    const double hpwl_before = ConnectedNetWeightedHpwlY(component);
    component.SetLLY(candidate.refined_y);
    const double hpwl_after = ConnectedNetWeightedHpwlY(component);
    const bool has_acceptable_gain = require_positive_gain
                                         ? hpwl_after < hpwl_before
                                         : hpwl_after <= hpwl_before;
    if (has_acceptable_gain) {
      accepted[candidate.component_index] = true;
      hpwl_improvement += hpwl_before - hpwl_after;
    } else {
      if (require_positive_gain && hpwl_after == hpwl_before) {
        ++neutral_rejection_count;
      }
      component.SetLLY(analytical_placement[candidate.component_index].ly);
    }
  }
  LOG(info) << "    transactional Y feedback: " << candidate_count
            << " candidates, "
            << std::count(accepted.begin(), accepted.end(), true)
            << " accepted, " << neutral_rejection_count
            << " neutral moves rejected, " << baseline_rejection_count
            << " non-positive baseline moves rejected, modeled HPWL "
               "improvement "
            << hpwl_improvement << "um\n";
  return accepted;
}

void GlobalPlacer::UpdateBestUpperBoundPlacement(double upper_bound_hpwl) {
  if (upper_bound_hpwl >= best_upper_bound_hpwl_) return;

  best_upper_bound_hpwl_ = upper_bound_hpwl;
  best_upper_bound_placement_ = SaveCurrentPlacement();
}

std::vector<GlobalPlacer::ComponentLocation>
GlobalPlacer::SaveCurrentPlacement() const {
  std::vector<ComponentLocation> placement;
  placement.reserve(ckt_ptr_->Components().size());
  for (const Component& component : ckt_ptr_->Components()) {
    placement.push_back({component.LLX(), component.LLY(), component.Orient()});
  }
  return placement;
}

void GlobalPlacer::RestorePlacement(
    const std::vector<ComponentLocation>& placement) {
  DaliExpects(placement.size() == ckt_ptr_->Components().size(),
              "Cannot restore placement: component count changed");
  for (size_t i = 0; i < placement.size(); ++i) {
    ckt_ptr_->Components()[i].SetLowerLeft(placement[i].lx, placement[i].ly);
    ckt_ptr_->Components()[i].SetOrient(placement[i].orient);
  }
}

bool GlobalPlacer::RollbackRefinementFeedbackIfRequested(
    const GlobalUpperBoundRefinement& refinement) {
  if (!refinement.rollback_previous_anchor_feedback) return false;

  DaliExpects(!refinement.feasible,
              "A feasible refinement cannot request feedback rollback");
  DaliExpects(!previous_feedback_checkpoint_.empty(),
              "Cannot roll back refinement feedback without a checkpoint");
  RestorePlacement(previous_feedback_checkpoint_);
  previous_feedback_checkpoint_.clear();
  LOG(info) << "    restore placement before destabilizing refinement "
               "feedback\n";
  return true;
}

void GlobalPlacer::LogRefinementDisplacement(
    const std::vector<ComponentLocation>& placement_before_refinement) {
  DaliExpects(
      placement_before_refinement.size() == ckt_ptr_->Components().size(),
      "Cannot measure refinement displacement: component count "
      "changed");
  double sum_x = 0.0;
  double sum_y = 0.0;
  double max_distance = 0.0;
  int movable_count = 0;
  for (size_t i = 0; i < placement_before_refinement.size(); ++i) {
    const Component& component = ckt_ptr_->Components()[i];
    if (!component.IsMovable()) continue;
    double displacement_x =
        std::fabs(component.LLX() - placement_before_refinement[i].lx) *
        ckt_ptr_->GridValueX();
    double displacement_y =
        std::fabs(component.LLY() - placement_before_refinement[i].ly) *
        ckt_ptr_->GridValueY();
    sum_x += displacement_x;
    sum_y += displacement_y;
    max_distance =
        std::max(max_distance, std::sqrt(displacement_x * displacement_x +
                                         displacement_y * displacement_y));
    ++movable_count;
  }
  double average_x = movable_count == 0 ? 0.0 : sum_x / movable_count;
  double average_y = movable_count == 0 ? 0.0 : sum_y / movable_count;
  LOG(info) << "    LAL-to-legal displacement avg X/Y: " << average_x << " / "
            << average_y << "um, max: " << max_distance << "um\n";
  RecordPlacementMetric("global_placement.feedback.last_avg_x_um", average_x);
  RecordPlacementMetric("global_placement.feedback.last_avg_y_um", average_y);
  RecordPlacementMetric("global_placement.feedback.last_max_um", max_distance);
}

void GlobalPlacer::RestoreBestUpperBoundPlacement() {
  if (best_upper_bound_placement_.empty()) return;
  DaliExpects(
      best_upper_bound_placement_.size() == ckt_ptr_->Components().size(),
      "Cannot restore best global placement: component count changed");

  RestorePlacement(best_upper_bound_placement_);
  LOG(info) << "  Restore best global upper bound: " << best_upper_bound_hpwl_
            << "um\n";
}

bool GlobalPlacer::ShouldRefineUpperBound() const {
  return upper_bound_refiner_ != nullptr &&
         cur_iter_ >= upper_bound_refiner_warmup_ &&
         (cur_iter_ - upper_bound_refiner_warmup_) %
                 upper_bound_refiner_interval_ ==
             0;
}

bool GlobalPlacer::HasCurrentConvergenceUpperBound() const {
  return upper_bound_refiner_ == nullptr || current_upper_bound_is_physical_;
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
  RestoreBestUpperBoundPlacement();
  UpdateMovableComponentPlacementStatus();
  RecordPlacementHpwlMetrics("global_placement", *ckt_ptr_);
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

/**
 * @brief Whether the upper-bound HPWL has stopped trending downward.
 *
 * Compares the mean of the last `upper_bound_improvement_patience_` iterations
 * against the mean of the window before it, and reports a stall when the two
 * differ by less than `upper_bound_min_improvement_`.
 *
 * Means over windows, rather than the best value seen so far, because the
 * upper bound is noisy: it comes from a rough legalization whose result can
 * jump by tens of percent between iterations. A single lucky iteration would
 * otherwise become a best that nothing beats for a long time, freezing the
 * detector on it and reporting a stall while the placement was still improving
 * steadily. Comparing windows tracks the trend and rides out one-off outliers.
 */
bool GlobalPlacer::HasUpperBoundHpwlStalled(
    const std::vector<double>& upper_bound_hpwl) const {
  const int series_size = static_cast<int>(upper_bound_hpwl.size());
  const int window = upper_bound_improvement_patience_;
  if (series_size < 2 * window) return false;

  auto window_mean = [&](int begin) {
    double sum = 0.0;
    for (int i = begin; i < begin + window; ++i) sum += upper_bound_hpwl[i];
    return sum / window;
  };

  const double previous = window_mean(series_size - 2 * window);
  const double recent = window_mean(series_size - window);
  return RelativeImprovement(previous, recent) < upper_bound_min_improvement_;
}

/**
 * @brief Whether global placement has stopped making progress.
 *
 * Criterion 1 (default): the best upper-bound HPWL has not improved
 * meaningfully for `upper_bound_improvement_patience_` iterations. That is what
 * convergence means here -- further iterations are not buying anything.
 *
 * Criterion 2 (POLAR): the lower/upper HPWL gap is below
 * `polar_converge_criterion_`.
 *
 * Criterion 1 deliberately does not also require a small lower/upper gap. That
 * gap measures how much legalization costs for the flow in use, not whether the
 * optimization has converged, and it does not shrink with iterations: in the
 * gridded well flow the upper bound is a rough gridded legalization and the gap
 * plateaus near 30%, far above the POLAR threshold. Requiring both conditions
 * made criterion 1 unsatisfiable for gridded designs, so global placement always
 * ran to `-global_max_iterations` and the cap, not convergence, decided when it
 * stopped.
 */
bool GlobalPlacer::IsPlacementConverged() {
  if (cur_iter_ + 1 < min_iter_) return false;
  if (!HasCurrentConvergenceUpperBound()) return false;

  bool res;
  auto& lower_bound_hpwl = optimizer_->GetHpwls();
  auto& upper_bound_hpwl = accepted_upper_bound_hpwl_;
  if (convergence_criteria_ == 1) {
    res = !upper_bound_hpwl.empty() && HasUpperBoundHpwlStalled(upper_bound_hpwl);
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
  if (optimizer_->GetHpwls().empty() || optimizer_->GetHpwlsX().empty() ||
      optimizer_->GetHpwlsY().empty() || accepted_upper_bound_hpwl_.empty() ||
      accepted_upper_bound_hpwl_x_.empty() ||
      accepted_upper_bound_hpwl_y_.empty()) {
    return;
  }
  double lo_hpwl = optimizer_->GetHpwls().back();
  double hi_hpwl = accepted_upper_bound_hpwl_.back();
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
  LOG(info) << "            lower X/Y " << optimizer_->GetHpwlsX().back()
            << " / " << optimizer_->GetHpwlsY().back() << ", upper X/Y "
            << accepted_upper_bound_hpwl_x_.back() << " / "
            << accepted_upper_bound_hpwl_y_.back() << "\n";
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
  LOG(debug) << "  Lower bound X: " << optimizer_->GetHpwlsX() << "\n";
  LOG(debug) << "  Lower bound Y: " << optimizer_->GetHpwlsY() << "\n";
  LOG(debug) << "  Upper bound: " << accepted_upper_bound_hpwl_ << "\n";
  LOG(debug) << "  Upper bound X: " << accepted_upper_bound_hpwl_x_ << "\n";
  LOG(debug) << "  Upper bound Y: " << accepted_upper_bound_hpwl_y_ << "\n";
  LOG(debug) << "cg time: " << optimizer_->GetTime()
             << "s, lal time: " << spreader_->GetTime() << "s";
  if (upper_bound_refiner_) {
    LOG(debug) << ", upper-bound refinement time: "
               << upper_bound_refiner_->GetTime() << "s";
  }
  LOG(debug) << "\n";
  Placer::PrintEndStatement(name_of_process, is_success);
}

}  // namespace dali
