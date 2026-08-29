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
#include <limits>
#include <memory>
#include <utility>

#include "dali/common/act_config.h"
#include "dali/common/elapsed_time.h"
#include "dali/placer/global_placer/stage_band.h"
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

void GlobalPlacer::SetDelayLineShapes(std::vector<DelayLineShape> shapes) {
  for (const DelayLineShape &shape : shapes) {
    DaliExpects(shape.offset_x.size() == shape.component_ids.size() &&
                    shape.offset_y.size() == shape.component_ids.size(),
                "Delay-line shape needs one offset pair per component");
  }
  delay_line_shapes_ = std::move(shapes);
}

void GlobalPlacer::SetStageBands(std::vector<StageBand> bands) {
  stage_bands_ = std::move(bands);
}

/**
 * Map each stage's cells into that stage's share of the placement region.
 *
 * The mapping is affine in y: a band's current vertical extent is scaled and
 * translated onto its interval, which preserves the ordering and the relative
 * spacing the objective just produced and changes only where the stage sits and
 * how tall it is. A hard clamp would instead pile cells onto the two band
 * edges, and pulling every cell toward a band centre would collapse the stage
 * into a line; neither leaves the spreader anything to work with.
 *
 * X is deliberately untouched. The bit axis is the objective's to decide, and
 * on `bd_pipeline` it already resolves bit index to column position at a
 * correlation above 0.98 without help.
 *
 * Fixed components are read but not moved: they contribute their position to
 * the band's measured extent, so the stage is mapped around them.
 */
std::vector<StageBandInterval> GlobalPlacer::StageBandIntervals() const {
  std::vector<Component> &components = ckt_ptr_->Components();
  std::vector<double> areas;
  areas.reserve(stage_bands_.size());
  for (const StageBand &band : stage_bands_) {
    double area = 0.0;
    for (int component_id : band.component_ids) {
      const Component &component = components[component_id];
      area += static_cast<double>(component.Width()) *
              static_cast<double>(component.Height());
    }
    areas.push_back(area);
  }
  return BuildStageBandIntervals(areas, ckt_ptr_->RegionLLY(),
                                 ckt_ptr_->RegionURY(), stage_band_spacing_);
}

/**
 * Publish each stage's band as pseudo-net targets for the next analytical
 * solve.
 *
 * This is the band's primary expression: it is a term the quadratic problem
 * balances against wirelength, not a correction applied to the answer
 * afterwards. Every cell in a band is aimed at that band's centre, and the
 * solve decides how far each one actually travels.
 */
void GlobalPlacer::PublishStageBandAnchors() {
  if (stage_bands_.empty()) return;
  const std::vector<StageBandInterval> intervals = StageBandIntervals();
  if (intervals.empty()) return;
  std::vector<StageBandAnchor> anchors;
  for (size_t band_index = 0; band_index < stage_bands_.size(); ++band_index) {
    const StageBandInterval &interval = intervals[band_index];
    const double centre = 0.5 * (interval.y_lo + interval.y_hi);
    for (int component_id : stage_bands_[band_index].component_ids) {
      anchors.push_back({component_id, centre});
    }
  }
  optimizer_->SetStageBandAnchors(std::move(anchors));
}

void GlobalPlacer::ApplyStageBands() {
  if (stage_bands_.empty() || !stage_band_reshape_upper_bound_) return;
  std::vector<Component> &components = ckt_ptr_->Components();

  const std::vector<StageBandInterval> intervals = StageBandIntervals();
  if (intervals.empty()) return;

  for (size_t band_index = 0; band_index < stage_bands_.size(); ++band_index) {
    const StageBand &band = stage_bands_[band_index];
    const StageBandInterval &interval = intervals[band_index];
    if (band.component_ids.empty() || interval.Height() <= 0.0) continue;

    double current_lo = std::numeric_limits<double>::infinity();
    double current_hi = -std::numeric_limits<double>::infinity();
    double tallest = 0.0;
    for (int component_id : band.component_ids) {
      const Component &component = components[component_id];
      const double height = static_cast<double>(component.Height());
      current_lo = std::min(current_lo, component.LLY());
      current_hi = std::max(current_hi, component.LLY() + height);
      tallest = std::max(tallest, height);
    }

    // Leave room for the cells themselves, so a mapped origin plus its cell
    // height still lands inside the band rather than poking out of the top.
    const double target_lo = interval.y_lo;
    const double target_hi = std::max(interval.y_hi - tallest, interval.y_lo);
    const double current_span = current_hi - tallest - current_lo;
    const double scale =
        current_span > 0.0 ? (target_hi - target_lo) / current_span : 0.0;

    for (int component_id : band.component_ids) {
      Component &component = components[component_id];
      if (!component.IsMovable()) continue;
      const double mapped =
          current_span > 0.0
              ? target_lo + (component.LLY() - current_lo) * scale
              : 0.5 * (target_lo + target_hi);
      component.SetLLY(std::min(std::max(mapped, target_lo), target_hi));
    }
  }
}

void GlobalPlacer::SetDelayLineFeedback(DelayLineFeedbackCallback callback,
                                        int warmup, int interval, int freeze) {
  DaliExpects(interval > 0, "Delay-line feedback interval must be positive");
  delay_line_feedback_ = std::move(callback);
  delay_line_feedback_warmup_ = warmup;
  delay_line_feedback_interval_ = interval;
  delay_line_feedback_freeze_ = freeze;
}

void GlobalPlacer::SetDelayLineFeedbackAppliedCallback(
    DelayLineFeedbackAppliedCallback callback) {
  delay_line_feedback_applied_callback_ = std::move(callback);
}

/**
 * Ask the owner for retuned shapes when this iteration is due for it.
 *
 * Called after the shape has been re-imposed, so the timing the owner measures
 * describes the placement the shape actually produced rather than the abutted
 * one the solve would have preferred.
 */
void GlobalPlacer::RefreshDelayLineShapes(int iteration) {
  if (delay_line_feedback_ == nullptr) return;
  if (iteration < delay_line_feedback_warmup_) return;
  if (iteration > delay_line_feedback_freeze_) return;
  if ((iteration - delay_line_feedback_warmup_) %
          delay_line_feedback_interval_ !=
      0) {
    return;
  }

  std::vector<DelayLineShape> shapes = delay_line_shapes_;
  if (!delay_line_feedback_(iteration, &shapes)) return;
  SetDelayLineShapes(std::move(shapes));
  ApplyDelayLineShapes();
  last_delay_line_shape_change_iteration_ = iteration;
  if (delay_line_feedback_applied_callback_ != nullptr) {
    delay_line_feedback_applied_callback_(iteration);
  }
}

/**
 * Re-impose each delay line's shape around wherever its first element sits.
 *
 * Applied to the upper bound every iteration, so it is the shape the anchor
 * pseudo-nets pull the next solve toward. Without that the wirelength objective
 * abuts the chain again on the very next solve, since a delay line's internal
 * nets are exactly what it is trying to shorten.
 *
 * Fixed components keep their locations, including a fixed first element, whose
 * position is then simply the origin everything else is measured from.
 */
void GlobalPlacer::ApplyDelayLineShapes() {
  if (delay_line_shapes_.empty()) return;
  std::vector<Component> &components = ckt_ptr_->Components();
  for (size_t shape_index = 0; shape_index < delay_line_shapes_.size();
       ++shape_index) {
    const DelayLineShape &shape = delay_line_shapes_[shape_index];
    if (shape.component_ids.empty()) continue;
    double base_x = components[shape.component_ids.front()].LLX();
    double base_y = components[shape.component_ids.front()].LLY();
    double lower_x = -std::numeric_limits<double>::infinity();
    double upper_x = std::numeric_limits<double>::infinity();
    double lower_y = -std::numeric_limits<double>::infinity();
    double upper_y = std::numeric_limits<double>::infinity();
    double requested_llx = std::numeric_limits<double>::infinity();
    double requested_lly = std::numeric_limits<double>::infinity();
    double requested_urx = -std::numeric_limits<double>::infinity();
    double requested_ury = -std::numeric_limits<double>::infinity();
    bool feasible = true;
    bool has_movable = false;
    for (size_t index = 0; index < shape.component_ids.size(); ++index) {
      Component &component = components[shape.component_ids[index]];
      const double target_x = base_x + shape.offset_x[index];
      const double target_y = base_y + shape.offset_y[index];
      requested_llx = std::min(requested_llx, target_x);
      requested_lly = std::min(requested_lly, target_y);
      requested_urx = std::max(requested_urx, target_x + component.Width());
      requested_ury = std::max(requested_ury, target_y + component.Height());
      const double component_lower_x = ckt_ptr_->RegionLLX() - target_x;
      const double component_upper_x =
          ckt_ptr_->RegionURX() - target_x - component.Width();
      const double component_lower_y = ckt_ptr_->RegionLLY() - target_y;
      const double component_upper_y =
          ckt_ptr_->RegionURY() - target_y - component.Height();
      if (component_lower_x > component_upper_x ||
          component_lower_y > component_upper_y) {
        feasible = false;
        break;
      }
      if (component.IsMovable()) {
        has_movable = true;
        lower_x = std::max(lower_x, component_lower_x);
        upper_x = std::min(upper_x, component_upper_x);
        lower_y = std::max(lower_y, component_lower_y);
        upper_y = std::min(upper_y, component_upper_y);
      } else {
        lower_x = std::max(lower_x, 0.0);
        upper_x = std::min(upper_x, 0.0);
        lower_y = std::max(lower_y, 0.0);
        upper_y = std::min(upper_y, 0.0);
      }
    }
    feasible &= has_movable && lower_x <= upper_x && lower_y <= upper_y;
    if (!feasible) {
      double current_llx = std::numeric_limits<double>::infinity();
      double current_lly = std::numeric_limits<double>::infinity();
      double current_urx = -std::numeric_limits<double>::infinity();
      double current_ury = -std::numeric_limits<double>::infinity();
      for (int component_id : shape.component_ids) {
        const Component &component = components[component_id];
        current_llx = std::min(current_llx, component.LLX());
        current_lly = std::min(current_lly, component.LLY());
        current_urx = std::max(current_urx,
                               component.LLX() + component.Width());
        current_ury = std::max(current_ury,
                               component.LLY() + component.Height());
      }
      const bool current_inside =
          current_llx >= ckt_ptr_->RegionLLX() &&
          current_lly >= ckt_ptr_->RegionLLY() &&
          current_urx <= ckt_ptr_->RegionURX() &&
          current_ury <= ckt_ptr_->RegionURY();
      LOG(warning) << "DELAY_LINE_SHAPE_BOUND index " << shape_index
                   << " inside " << (current_inside ? 1 : 0)
                   << " applied 0"
                   << " requested_llx " << requested_llx << " requested_lly "
                   << requested_lly << " requested_urx " << requested_urx
                   << " requested_ury " << requested_ury
                   << " applied_llx " << current_llx << " applied_lly "
                   << current_lly << " applied_urx " << current_urx
                   << " applied_ury " << current_ury
                   << " residual_area_bound 1\n";
      continue;
    }
    // Translating the whole chain moves its head, and the head sits on the net
    // that also feeds its stage's clock tree, so a large shift lengthens the
    // constraint's fast path. Pinning the head and scaling the shape to fit
    // instead was tried and is worse: a head near a region edge then scales its
    // band toward zero and the chain collapses to the abutted layout that has
    // no delay at all. Measured on `bd_pipeline`, pinning took dl5 from -286 ps
    // to -1316 and lost legalization, against -553 worst for translating. The
    // shift stays until the head can be kept away from the edges in the first
    // place.
    const double shift_x = std::clamp(0.0, lower_x, upper_x);
    const double shift_y = std::clamp(0.0, lower_y, upper_y);
    const double applied_llx = requested_llx + shift_x;
    const double applied_lly = requested_lly + shift_y;
    const double applied_urx = requested_urx + shift_x;
    const double applied_ury = requested_ury + shift_y;
    for (size_t index = 0; index < shape.component_ids.size(); ++index) {
      Component &component = components[shape.component_ids[index]];
      if (!component.IsMovable()) continue;
      component.SetLLX(base_x + shape.offset_x[index] + shift_x);
      component.SetLLY(base_y + shape.offset_y[index] + shift_y);
    }
    LOG(info) << "DELAY_LINE_SHAPE_BOUND index " << shape_index
              << " inside 1 applied 1 shift_x " << shift_x << " shift_y "
              << shift_y << " requested_llx "
              << requested_llx << " requested_lly " << requested_lly
              << " requested_urx " << requested_urx << " requested_ury "
              << requested_ury << " applied_llx " << applied_llx
              << " applied_lly " << applied_lly << " applied_urx "
              << applied_urx << " applied_ury " << applied_ury
              << " residual_area_bound 0\n";
  }
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

/** Construct the optimizer, spreader, and refiner for this run's flow. */
void GlobalPlacer::InitializePlacementEngines() {
  optimizer_ =
      std::make_unique<BoundToBoundHpwlOptimizer>(ckt_ptr_, num_threads_);
  optimizer_->SetNetIgnoreThreshold(net_ignore_threshold_);
  optimizer_->Initialize();

  auto look_ahead_spreader = std::make_unique<LookAheadSpreader>(
      ckt_ptr_, capacity_model_, num_threads_);
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
  current_upper_bound_is_physical_ = upper_bound_refiner_ == nullptr;
  if (upper_bound_refiner_) {
    upper_bound_refiner_->Initialize(PlacementDensity());
  }
}

/**
 * Clears what belongs to the placement run rather than to one topology.
 *
 * Kept apart from engine construction because the engines are rebuilt at every
 * topology checkpoint while the run continues across them. Folding the two
 * together restarted the trajectory, the convergence windows, and the
 * best-so-far in the middle of what is meant to be a single placement
 * operation.
 */
void GlobalPlacer::InitializeRunState() {
  converged_at_checkpoint_ = false;
  accepted_upper_bound_hpwl_.clear();
  physical_upper_bound_hpwl_.clear();
  accepted_upper_bound_hpwl_x_.clear();
  accepted_upper_bound_hpwl_y_.clear();
  best_upper_bound_placement_.clear();
  previous_feedback_checkpoint_.clear();
  best_upper_bound_hpwl_ = std::numeric_limits<double>::max();
  checkpoint_restarts_ = 0;
  runtime_breakdown_ = RuntimeBreakdown{};
}

/**
 * Drops the caches a topology change invalidated, and only those.
 *
 * Both hold one entry per component, indexed by component id, so once the
 * component count changes they describe a netlist that no longer exists and
 * restoring either would abort on the size check. The HPWL trajectory is
 * deliberately not touched: it is a record of the run, and the run continues.
 */
void GlobalPlacer::InvalidateTopologySizedCaches(size_t components_before) {
  if (ckt_ptr_->Components().size() == components_before) return;
  if (!best_upper_bound_placement_.empty()) {
    LOG(info) << "  Dropping best upper-bound placement: it describes "
              << best_upper_bound_placement_.size() << " components, the "
              << "circuit now has " << ckt_ptr_->Components().size() << "\n";
  }
  best_upper_bound_placement_.clear();
  best_upper_bound_hpwl_ = std::numeric_limits<double>::max();
  previous_feedback_checkpoint_.clear();
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

/**
 * The global placement loop: solve for a wirelength lower bound, spread to get
 * an upper bound, then pull the two together with anchor pseudo-nets whose
 * strength rises each iteration.
 *
 * The gridded flow additionally rough-legalizes each iteration, which replaces
 * the spread upper bound with a nearly legal one and can feed row assignments
 * back into the placement. Stops when IsPlacementConverged agrees or the
 * iteration limit is reached.
 */
bool GlobalPlacer::RunPlacementIterations() {
  for (; cur_iter_ < max_iter_; ++cur_iter_) {
    ElapsedTime iteration_timer;
    iteration_timer.RecordStartTime();
    double measured_wall_seconds = 0.0;

    ElapsedTime phase_timer;
    phase_timer.RecordStartTime();
    optimizer_->SetIteration(cur_iter_);
    PublishStageBandAnchors();
    optimizer_->OptimizeHpwl();
    phase_timer.RecordEndTime();
    runtime_breakdown_.optimizer_wall_seconds += phase_timer.GetWallTime();
    measured_wall_seconds += phase_timer.GetWallTime();
    EmitIterationSnapshot("lower_bound", "Lower Bound", "lower_bound");

    phase_timer.RecordStartTime();
    spreader_->SetIteration(cur_iter_);
    spreader_->Spread();
    phase_timer.RecordEndTime();
    runtime_breakdown_.spreader_wall_seconds += phase_timer.GetWallTime();
    measured_wall_seconds += phase_timer.GetWallTime();

    phase_timer.RecordStartTime();
    // Bands first: a delay line's shape is the timing mechanism and must win
    // over the stage geometry wherever a recipe declares both for one cell.
    ApplyStageBands();
    ApplyDelayLineShapes();
    RefreshDelayLineShapes(cur_iter_);
    double accepted_hpwl = spreader_->Hpwls().back();
    double accepted_hpwl_x = spreader_->HpwlsX().back();
    double accepted_hpwl_y = spreader_->HpwlsY().back();
    if (!delay_line_shapes_.empty()) {
      // The spread deliberately lengthens nets, so the spreader's own totals no
      // longer describe the placement that will be anchored and reported.
      accepted_hpwl = WeightedHPWL();
      accepted_hpwl_x = ckt_ptr_->WeightedHPWLX();
      accepted_hpwl_y = ckt_ptr_->WeightedHPWLY();
    }
    phase_timer.RecordEndTime();
    runtime_breakdown_.shape_wall_seconds += phase_timer.GetWallTime();
    measured_wall_seconds += phase_timer.GetWallTime();
    bool accepted_physical_refinement = false;
    bool anchor_all_components = true;
    std::vector<int> selective_anchor_component_ids;
    std::vector<std::vector<int>> refined_component_rows;
    current_upper_bound_is_physical_ = upper_bound_refiner_ == nullptr;
    std::vector<ComponentLocation> placement_before_refinement;
    if (ShouldRefineUpperBound()) {
      phase_timer.RecordStartTime();
      placement_before_refinement = SaveCurrentPlacement();
      GlobalUpperBoundRefinement refinement =
          upper_bound_refiner_->Refine(cur_iter_);
      /** Feed a rough-legalization result into the density model as added pressure. */
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
        /** Log how far the refiner moved cells this iteration. */
        LogRefinementDisplacement(placement_before_refinement);
      } else {
        previous_feedback_checkpoint_.clear();
      }
      phase_timer.RecordEndTime();
      runtime_breakdown_.physical_refinement_wall_seconds +=
          phase_timer.GetWallTime();
      measured_wall_seconds += phase_timer.GetWallTime();
      ++runtime_breakdown_.physical_refinements;
    }
    accepted_upper_bound_hpwl_.push_back(accepted_hpwl);
    accepted_upper_bound_hpwl_x_.push_back(accepted_hpwl_x);
    accepted_upper_bound_hpwl_y_.push_back(accepted_hpwl_y);
    if (accepted_physical_refinement) {
      physical_upper_bound_hpwl_.push_back(accepted_hpwl);
      UpdateBestUpperBoundPlacement(accepted_hpwl);
    }
    // One machine-readable record per iteration, reporting whether this
    // iteration produced a physical upper bound that was accepted rather than
    // rolled back. That is the only state at which topology may be changed, so
    // a checkpoint schedule has to be chosen from where these appear rather
    // than from a fixed guess.
    {
      const double previous_physical =
          physical_upper_bound_hpwl_.size() >= 2
              ? physical_upper_bound_hpwl_[physical_upper_bound_hpwl_.size() - 2]
              : 0.0;
      const double change =
          (accepted_physical_refinement && previous_physical > 0.0)
              ? (accepted_hpwl - previous_physical) / previous_physical
              : 0.0;
      LOG(info) << "PLACEMENT_TRAJECTORY iteration " << cur_iter_
                << " physical " << (accepted_physical_refinement ? 1 : 0)
                << " accepted_hpwl " << accepted_hpwl
                << " hpwl_change_fraction " << change
                << " lower_hpwl " << optimizer_->GetHpwls().back()
                << " physical_upper_bounds "
                << physical_upper_bound_hpwl_.size() << "\n";
      // The checkpoint is described here, where its numbers are in hand, and
      // offered at the end of the iteration.
      pending_checkpoint_.iteration = cur_iter_;
      pending_checkpoint_.accepted_hpwl = accepted_hpwl;
      pending_checkpoint_.hpwl_change_fraction = change;
      pending_checkpoint_.lower_bound_hpwl = optimizer_->GetHpwls().back();
      pending_checkpoint_.physical_upper_bound_count =
          static_cast<int>(physical_upper_bound_hpwl_.size());
    }
    EmitIterationSnapshot("upper_bound", "Upper Bound", "upper_bound");
    // The accepted physical placement is live exactly here. The feedback below
    // restores coordinates from the analytical solve, so an observer that ran
    // after it would be looking at a different placement from the one the
    // accepted upper bound describes.
    if (accepted_physical_refinement && accepted_physical_observer_) {
      phase_timer.RecordStartTime();
      accepted_physical_observer_(cur_iter_);
      phase_timer.RecordEndTime();
      runtime_breakdown_.timing_observer_wall_seconds +=
          phase_timer.GetWallTime();
      measured_wall_seconds += phase_timer.GetWallTime();
      ++runtime_breakdown_.timing_observer_calls;
    }
    if (accepted_physical_refinement) {
      phase_timer.RecordStartTime();
      ApplyRefinedAnchorFeedback(
          placement_before_refinement, anchor_all_components,
          selective_anchor_component_ids, refined_component_rows);
      previous_feedback_checkpoint_ = std::move(placement_before_refinement);
      phase_timer.RecordEndTime();
      runtime_breakdown_.anchor_feedback_wall_seconds +=
          phase_timer.GetWallTime();
      measured_wall_seconds += phase_timer.GetWallTime();
      ++runtime_breakdown_.anchor_feedbacks;
      if (post_feedback_observer_) {
        phase_timer.RecordStartTime();
        post_feedback_observer_(cur_iter_);
        phase_timer.RecordEndTime();
        runtime_breakdown_.timing_observer_wall_seconds +=
            phase_timer.GetWallTime();
        measured_wall_seconds += phase_timer.GetWallTime();
        ++runtime_breakdown_.timing_observer_calls;
      }
    }
    /** Log the iteration's lower/upper bounds, tagged by upper-bound kind. */
    PrintHpwl();

    // Offer the checkpoint only after the iteration has finished, so that an
    // iteration which is checkpointed is otherwise indistinguishable from one
    // that is not. Leaving earlier skipped this iteration's refined anchor
    // feedback entirely, which is not a neutral thing to skip: it both seeds
    // the next solve and, in the transactional modes, moves cells. On
    // bd_pipeline that alone diverted the placement from iteration 13 onward.
    //
    // Offering it here is safe because a delta only ever adds: existing
    // component ids are stable across one, so pseudo-nets naming them stay
    // valid whether or not the topology changes.
    // Evaluated on every iteration, checkpointed or not, so that taking a
    // checkpoint cannot change the answer by not asking the question.
    const bool converged = IsPlacementConverged();
    auto finish_iteration_timing = [&]() {
      iteration_timer.RecordEndTime();
      runtime_breakdown_.other_wall_seconds +=
          std::max(0.0, iteration_timer.GetWallTime() - measured_wall_seconds);
      ++runtime_breakdown_.iterations;
    };
    if (checkpoint_observer_ != nullptr && accepted_physical_refinement &&
        checkpoint_observer_->Observe(pending_checkpoint_) ==
            CheckpointDecision::kChangeTopology) {
      // Only the decision is taken here. The engines are still up, so the
      // mutation itself happens after the loop returns and they are closed.
      converged_at_checkpoint_ = converged;
      finish_iteration_timing();
      return false;
    }
    finish_iteration_timing();
    if (converged) break;
  }
  return true;
}

/**
 * Hands the host the interval in which it may change the netlist.
 *
 * Expects every topology-sized engine to be closed already: the optimizer's
 * matrix and the spreader's grid are both sized by the component count, so a
 * netlist edit while either exists corrupts them. Closing them first is the
 * entire purpose of splitting the mutation out of the iteration loop, and the
 * boundary carries the placer so the caller can assert it rather than assume it.
 *
 * The circuit itself stays alive and keeps its coordinates; only the engines
 * built around it are rebuilt afterwards.
 */
TopologyMutationStatus GlobalPlacer::InvokeTopologyMutation() {
  DaliExpects(!ArePlacementEnginesOpen(),
              "Topology may only change while placement engines are closed");
  if (checkpoint_observer_ == nullptr) {
    return TopologyMutationStatus::kNoChange;
  }

  const size_t components_before = ckt_ptr_->Components().size();
  const size_t nets_before = ckt_ptr_->Nets().size();

  TopologyCheckpointContext context;
  context.checkpoint_iteration = pending_checkpoint_.iteration;
  context.resume_iteration = cur_iter_ + 1;
  context.component_count = components_before;
  context.net_count = nets_before;
  context.component_headroom =
      ckt_ptr_->Components().capacity() - components_before;
  context.net_headroom = ckt_ptr_->Nets().capacity() - nets_before;

  TopologyMutationResult result =
      checkpoint_observer_->RequestTopologyChange(pending_checkpoint_, context);

  if (result.status == TopologyMutationStatus::kFailed) {
    LOG(error) << "  CHECKPOINT_FAILED iteration "
               << pending_checkpoint_.iteration << ": " << result.message
               << "\n";
    return TopologyMutationStatus::kFailed;
  }
  if (result.status == TopologyMutationStatus::kNoChange ||
      result.delta.IsEmpty()) {
    LOG(info) << "  CHECKPOINT_NO_CHANGE iteration "
              << pending_checkpoint_.iteration << " resume_iteration "
              << context.resume_iteration << "\n";
    return TopologyMutationStatus::kNoChange;
  }

  // Validated in full before the first component is added, so the application
  // below cannot fail partway and leave a circuit nobody can repair.
  std::string error_message;
  if (!ValidateTopologyDelta(*ckt_ptr_, result.delta, &error_message)) {
    LOG(error) << "  CHECKPOINT_FAILED iteration "
               << pending_checkpoint_.iteration
               << ": rejected topology delta: " << error_message << "\n";
    return TopologyMutationStatus::kFailed;
  }
  ApplyTopologyDelta(*ckt_ptr_, result.delta);
  InvalidateTopologySizedCaches(components_before);

  LOG(info) << "  CHECKPOINT_TAKEN iteration " << pending_checkpoint_.iteration
            << " resume_iteration " << context.resume_iteration
            << " components " << components_before << " -> "
            << ckt_ptr_->Components().size() << " nets " << nets_before
            << " -> " << ckt_ptr_->Nets().size() << "\n";
  return TopologyMutationStatus::kApplied;
}

/** Feed a rough-legalization result into the density model as added pressure. */
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

/**
 * Fold a rough-legalization result back into the placement as anchor targets.
 *
 * After the gridded refiner produces a nearly legal placement, this steers the
 * next lower-bound solve toward it by anchoring components -- all of them, or a
 * selected subset -- to where legalization put them. `placement_before_refinement`
 * is kept so the feedback can be rolled back if the iteration regresses.
 */
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
        /** Collect the translation-invariant Y offsets requested between components. */
        BuildRelativeYConstraints(component_rows, keep_component_y);
    LOG(debug) << "    relative Y feedback: " << constraints.size()
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
  LOG(debug) << "    legalization feedback: "
            << RefinementFeedbackModeName(refinement_feedback_mode_) << ", "
            << selected_movable_count << " selected components\n";
}

/** Collect the requested translation-invariant Y offsets between components. */
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

/**
 * Decide which components' Y feedback to accept, as a transaction.
 *
 * A candidate is accepted only if the group it belongs to stays coherent once
 * accepted, so a set of row moves is taken or dropped together rather than
 * leaving the placement half-updated. Returns the accepted mask.
 */
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
  LOG(debug) << "    transactional Y feedback: " << candidate_count
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

/** Log how far the refiner moved cells this iteration. */
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

  InitializeRunState();
  PreparePlacement();
  cur_iter_ = 0;
  // Global placement runs as one or more segments separated by topology
  // changes. Without an observer that ever asks for one there is exactly one
  // segment, and this is the pre-checkpoint flow unchanged.
  //
  // Ordering inside the loop is the point of the whole design: the engines for
  // the old topology are closed before the host is given its window, and the
  // new ones are built only after the netlist has stopped changing. The
  // circuit, the trajectory, the accepted coordinates, and the absolute
  // iteration counter all live across every segment.
  while (!RunPlacementIterations()) {
    // Captured before the engines go, so an unchanged topology can resume with
    // the anchors it had rather than with none.
    const size_t components_before = ckt_ptr_->Components().size();
    HpwlOptimizer::AnchorState anchors = optimizer_->ExportAnchorState();
    ClosePlacementEngines();
    const TopologyMutationStatus status = InvokeTopologyMutation();
    if (status == TopologyMutationStatus::kFailed) {
      // Placement failed. Nothing is finalized, so no caller can mistake a
      // partial run for a result and export it.
      PrintEndStatement("Global placement", false);
      return false;
    }
    if (status == TopologyMutationStatus::kApplied) {
      ++checkpoint_restarts_;
      // The netlist is not the one the convergence decision was about, so that
      // decision is discarded and placement continues. Stated in the log,
      // because "kept going" and "forgot to ask" look identical afterwards.
      LOG(info) << "  Resuming after a topology change: the objective changed, "
                   "so convergence at iteration "
                << pending_checkpoint_.iteration << " no longer applies\n";
    } else if (converged_at_checkpoint_) {
      // Nothing changed, so the decision the checkpointed iteration reached
      // still stands. Resuming here would add an iteration that an
      // uninterrupted run would not have run.
      LOG(info) << "  Placement had converged at iteration "
                << pending_checkpoint_.iteration
                << " and the host changed nothing; finishing there\n";
      break;
    }
    InitializePlacementEngines();
    // Anchors are one target per component, so they only describe the netlist
    // they were taken from. After a change they are dropped and the next solve
    // anchors afresh, which is the same state a run reaches at its first
    // iteration.
    if (ckt_ptr_->Components().size() == components_before) {
      optimizer_->ImportAnchorState(anchors);
    } else if (anchors.is_set) {
      LOG(info) << "  Dropping anchor state: it describes " << anchors.x.size()
                << " components, the circuit now has "
                << ckt_ptr_->Components().size() << "\n";
    }
    ++cur_iter_;
  }
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
 * Convergence for the gridded flow: the rough-legalized upper bound has stopped
 * improving.
 *
 * Only that series is consulted. It is what legalization will actually produce,
 * whereas the lower/upper gap here measures legalization cost rather than
 * convergence and never shrinks. The series is kept apart from the accepted
 * upper bounds because those fall back to the unlegalized placement whenever the
 * refiner reports infeasible.
 */
bool GlobalPlacer::IsGriddedPlacementConverged() const {
  return HasUpperBoundHpwlStalled(physical_upper_bound_hpwl_);
}

/**
 * Convergence for flows without rough legalization: every upper bound is the
 * spread placement, so criterion 1 tests that series for a stall and criterion 2
 * tests the lower/upper gap against `convergence_gap_threshold_`.
 */
bool GlobalPlacer::IsStandardCellPlacementConverged() {
  if (!HasCurrentConvergenceUpperBound()) return false;

  auto& lower_bound_hpwl = optimizer_->GetHpwls();
  auto& upper_bound_hpwl = accepted_upper_bound_hpwl_;
  if (convergence_criteria_ == 1) {
    return !upper_bound_hpwl.empty() &&
           HasUpperBoundHpwlStalled(upper_bound_hpwl);
  }
  if (convergence_criteria_ == 2) {
    if (lower_bound_hpwl.empty()) return false;
    double lower_bound = lower_bound_hpwl.back();
    double upper_bound = upper_bound_hpwl.back();
    return (lower_bound > 1e-10) && (lower_bound < upper_bound) &&
           (upper_bound / lower_bound - 1 < convergence_gap_threshold_);
  }
  DaliExpects(false, "Unknown Convergence Criteria!");
  return false;
}

/** True when the gridded flow rough-legalizes on every iteration. */
bool GlobalPlacer::UsesGriddedRoughLegalization() const {
  return upper_bound_refiner_ != nullptr;
}

bool GlobalPlacer::IsPlacementConverged() {
  if (cur_iter_ + 1 < min_iter_) return false;
  // Convergence is a stall in the upper-bound HPWL series, measured across two
  // windows of `upper_bound_improvement_patience_` iterations. A delay-line
  // shape change invalidates every sample taken before it, so a stall declared
  // across that boundary is comparing two different geometries and reading the
  // difference as convergence. Worse, it lets placement stop on the iteration
  // after a shape arrives, handing legalization a geometry the solve never
  // absorbed -- which is how a separation that legalizes perfectly well from a
  // settled placement came to fail well legalization by 0.6 um.
  //
  // Bounded by max_iter_, which ends the loop regardless, so a line that keeps
  // being retuned cannot extend placement indefinitely.
  if (last_delay_line_shape_change_iteration_ >= 0 &&
      cur_iter_ - last_delay_line_shape_change_iteration_ <
          2 * upper_bound_improvement_patience_) {
    return false;
  }
  return UsesGriddedRoughLegalization() ? IsGriddedPlacementConverged()
                                        : IsStandardCellPlacementConverged();
}

/****
 * @brief A helper function to format and print HPWL in each iteration.
 */
/**
 * Names the quantity the accepted upper bound holds, for the iteration log.
 *
 * Only meaningful when a refiner is in use: the upper bound is then normally a
 * rough-legalized placement, but falls back to the merely spread placement when
 * the refiner reports the result infeasible, and the two are otherwise
 * indistinguishable in the log. Without a refiner every upper bound is the
 * spread placement by definition, so there is nothing to disambiguate.
 */
const char* GlobalPlacer::UpperBoundKindLabel() const {
  if (upper_bound_refiner_ == nullptr) return "";
  return current_upper_bound_is_physical_ ? " rough-legal" : " spread";
}

/** Log the iteration's lower/upper bounds, tagged by upper-bound kind. */
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
               "  iter-%-3d: lower %.4e, upper %.4e, gap %.4e (%.2f%%)%s\n",
               cur_iter_, lo_hpwl, hi_hpwl, hpwl_gap, hpwl_gap_percent,
               UpperBoundKindLabel());
  buffer.resize(written_length);
  LOG(info) << buffer;
  LOG(debug) << "            lower X/Y " << optimizer_->GetHpwlsX().back()
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
  // The engines are already gone when placement ends at a topology checkpoint
  // whose mutation was declined, so their totals are reported only if they are
  // still there to ask.
  if (optimizer_ != nullptr) {
    LOG(debug) << "  Lower bound: " << optimizer_->GetHpwls() << "\n";
    LOG(debug) << "  Lower bound X: " << optimizer_->GetHpwlsX() << "\n";
    LOG(debug) << "  Lower bound Y: " << optimizer_->GetHpwlsY() << "\n";
  }
  LOG(debug) << "  Upper bound: " << accepted_upper_bound_hpwl_ << "\n";
  LOG(debug) << "  Upper bound X: " << accepted_upper_bound_hpwl_x_ << "\n";
  LOG(debug) << "  Upper bound Y: " << accepted_upper_bound_hpwl_y_ << "\n";
  if (optimizer_ != nullptr && spreader_ != nullptr) {
    LOG(debug) << "cg time: " << optimizer_->GetTime()
               << "s, lal time: " << spreader_->GetTime() << "s";
  }
  if (upper_bound_refiner_) {
    LOG(debug) << ", upper-bound refinement time: "
               << upper_bound_refiner_->GetTime() << "s";
  }
  LOG(debug) << "\n";
  Placer::PrintEndStatement(name_of_process, is_success);
}

}  // namespace dali
