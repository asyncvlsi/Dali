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
#include "gridded_cell_well_legalizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>

#include "dali/common/act_config.h"
#include "dali/common/elapsed_time.h"
#include "dali/common/helper.h"
#include "dali/common/placement_metrics.h"
#include "dali/placer/well_legalizer/banded_stripe_assigner.h"
#include "dali/placer/well_legalizer/gridded_stripe_balancer.h"
#include "dali/placer/well_legalizer/ortools_gridded_row_optimizer.h"
#include "dali/placer/well_legalizer/stripe_boundary_coordinate_optimizer.h"
#include "dali/placer/well_legalizer/stripe_helper.h"
#include "dali/placer/well_legalizer/well_geometry.h"
#include "dali/placer/well_legalizer/well_geometry_exporter.h"

namespace dali {

GriddedCellWellLegalizer::GriddedCellWellLegalizer() {
  max_unplug_length_ = 0;
  well_tap_width_ = 0;
}

void GriddedCellWellLegalizer::SetSnapshotCallback(
    SnapshotCallback snapshot_callback) {
  snapshot_callback_ = std::move(snapshot_callback);
}

void GriddedCellWellLegalizer::LoadConf(std::string const& config_file) {
  config_read(config_file.c_str());
  DaliExpects(false, "Not implemented");
}

void GriddedCellWellLegalizer::CheckWellStatus() {
  auto& components = ckt_ptr_->Components();
  for (Component& component : components) {
    if (component.IsMovable()) {
      DaliExpects(component.MacroPtr()->HasWellInfo(),
                  "Cannot find well info for component: " << component.Name());
    }
  }
}

void GriddedCellWellLegalizer::FetchNpWellParams() {
  if (physical_parameter_circuit_ == ckt_ptr_) {
    return;
  }
  Tech& tech = ckt_ptr_->tech();
  WellLayer& n_well_layer = tech.NwellLayer();
  double grid_value_x = ckt_ptr_->GridValueX();
  int same_well_spacing = std::ceil(n_well_layer.Spacing() / grid_value_x);
  int op_well_spacing =
      std::ceil(n_well_layer.OppositeSpacing() / grid_value_x);
  well_spacing_ = std::max(same_well_spacing, op_well_spacing);
  max_unplug_length_ =
      (int)std::floor(n_well_layer.MaxPlugDist() / grid_value_x);
  DaliExpects(!ckt_ptr_->tech().WellTapMacroIds().empty(),
              "Cannot find the definition of well tap cell, well legalization "
              "cannot proceed\n");
  int well_tap_macro_id = ckt_ptr_->tech().WellTapMacroIds()[0];
  well_tap_macro_ = &(ckt_ptr_->tech().Macros()[well_tap_macro_id]);
  well_tap_width_ = well_tap_macro_->Width();

  LOG(info) << "  Well max plug distance: " << n_well_layer.MaxPlugDist()
            << "um, " << max_unplug_length_ << " \n";
  LOG(info) << "  GridValueX: " << ckt_ptr_->GridValueX() << " um\n";
  LOG(info) << "  Well spacing: " << n_well_layer.Spacing() << "um, "
            << well_spacing_ << "\n";
  LOG(info) << "  Well tap cell width: " << well_tap_width_ << "\n";

  if (enable_end_cap_cell_) {
    pre_end_cap_min_width_ = ckt_ptr_->tech().PreEndCapMinWidth();
    LOG(info) << "  pre_end_cap_min_width: " << pre_end_cap_min_width_ << "\n";

    pre_end_cap_min_p_height_ = ckt_ptr_->tech().PreEndCapMinPHeight();
    LOG(info) << "  pre_end_cap_min_p_height: " << pre_end_cap_min_p_height_
              << "\n";

    pre_end_cap_min_n_height_ = ckt_ptr_->tech().PreEndCapMinNHeight();
    LOG(info) << "  pre_end_cap_min_n_height: " << pre_end_cap_min_n_height_
              << "\n";

    post_end_cap_min_width_ = ckt_ptr_->tech().PostEndCapMinWidth();
    LOG(info) << "  post_end_cap_min_width: " << post_end_cap_min_width_
              << "\n";

    post_end_cap_min_p_height_ = ckt_ptr_->tech().PostEndCapMinPHeight();
    LOG(info) << "  post_end_cap_min_p_height: " << post_end_cap_min_p_height_
              << "\n";

    post_end_cap_min_n_height_ = ckt_ptr_->tech().PostEndCapMinNHeight();
    LOG(info) << "  post_end_cap_min_n_height: " << post_end_cap_min_n_height_
              << "\n";

    EnsureUsableEndCapWidths();
  }

  well_tap_p_height_ = well_tap_macro_->FirstPwellHeight();
  well_tap_n_height_ = well_tap_macro_->FirstNwellHeight();
  physical_parameter_circuit_ = ckt_ptr_;
}

GriddedCapacityConfig GriddedCellWellLegalizer::BuildGriddedCapacityConfig(
    double target_density) {
  CheckWellStatus();
  FetchNpWellParams();

  GriddedCapacityConfig config;
  config.reserved_width = PhysicalCompletionReservedWidth();
  config.target_density = target_density;
  if (!disable_welltap_) {
    config.minimum_p_well_height = well_tap_p_height_;
    config.minimum_n_well_height = well_tap_n_height_;
  }
  if (enable_end_cap_cell_) {
    config.minimum_p_well_height = std::max(
        config.minimum_p_well_height,
        std::max(pre_end_cap_min_p_height_, post_end_cap_min_p_height_));
    config.minimum_n_well_height = std::max(
        config.minimum_n_well_height,
        std::max(pre_end_cap_min_n_height_, post_end_cap_min_n_height_));
  }
  return config;
}

double GriddedCellWellLegalizer::EstimateGriddedDemandNormalization(
    const GriddedCapacityConfig& config) const {
  DaliExpects(ckt_ptr_ != nullptr,
              "Cannot calibrate gridded capacity without a circuit");

  int requested_row_width =
      max_row_width_ > 0
          ? max_row_width_
          : static_cast<int>(std::round(2.0 * max_unplug_length_));
  int column_pitch = requested_row_width + well_spacing_;
  int column_count = std::max(
      1, static_cast<int>(std::ceil(RegionWidth() / double(column_pitch))));
  int representative_row_width =
      std::max(1, RegionWidth() / column_count - well_spacing_);

  std::vector<Component*> movable_components;
  movable_components.reserve(ckt_ptr_->Components().size());
  unsigned long long raw_component_area = 0;
  for (Component& component : ckt_ptr_->Components()) {
    if (!component.IsMovable()) continue;
    movable_components.push_back(&component);
    raw_component_area += component.Area();
  }
  if (raw_component_area == 0) return 1.0;

  double raw_utilization = ckt_ptr_->WhiteSpaceUsage();
  unsigned long long whitespace_area = static_cast<unsigned long long>(
      std::llround(raw_component_area / raw_utilization));
  GriddedCapacityEstimate estimate = GriddedCapacityEstimator(config).Estimate(
      movable_components, representative_row_width, RegionHeight(),
      whitespace_area);
  if (estimate.available_gridded_area == 0) return 1.0;

  double raw_pressure =
      raw_component_area /
      (static_cast<double>(whitespace_area) * config.target_density);
  double gridded_pressure =
      estimate.required_gridded_area /
      static_cast<double>(estimate.available_gridded_area);
  if (raw_pressure <= 0.0 || gridded_pressure <= 0.0) return 1.0;

  double normalization = gridded_pressure / raw_pressure;
  LOG(info) << "  Gridded capacity calibration:\n"
            << "    representative row width   : " << representative_row_width
            << "\n"
            << "    raw pressure                : " << raw_pressure << "\n"
            << "    gridded pressure            : " << gridded_pressure << "\n"
            << "    demand normalization        : " << normalization << "\n";
  return normalization;
}

void GriddedCellWellLegalizer::SaveInitialComponentLocation() {
  component_init_locations_ = CaptureComponentPlacement();
}

std::vector<GriddedCellWellLegalizer::ComponentPlacementSnapshot>
GriddedCellWellLegalizer::CaptureComponentPlacement() const {
  std::vector<ComponentPlacementSnapshot> component_snapshots;
  const std::vector<Component>& component_list = ckt_ptr_->Components();
  component_snapshots.reserve(component_list.size());
  for (auto& component : component_list) {
    ComponentPlacementSnapshot snapshot;
    snapshot.lx = component.LLX();
    snapshot.ly = component.LLY();
    snapshot.orient = component.Orient();
    component_snapshots.push_back(snapshot);
  }
  return component_snapshots;
}

void GriddedCellWellLegalizer::RestoreComponentPlacement(
    const std::vector<ComponentPlacementSnapshot>& component_snapshots) {
  DaliExpects(component_snapshots.size() == ckt_ptr_->Components().size(),
              "Cannot restore component placement: component count changed");

  std::vector<Component>& component_list = ckt_ptr_->Components();
  for (size_t i = 0; i < component_list.size(); ++i) {
    const ComponentPlacementSnapshot& snapshot = component_snapshots[i];
    component_list[i].SetLLX(snapshot.lx);
    component_list[i].SetLLY(snapshot.ly);
    component_list[i].SetOrient(snapshot.orient);
  }
}

void GriddedCellWellLegalizer::RestoreInitialComponentLocation() {
  RestoreComponentPlacement(component_init_locations_);
}

void GriddedCellWellLegalizer::SetMaxRowWidth(double max_row_width_microns) {
  if (max_row_width_microns < 0) {
    max_row_width_ = -1;
    return;
  }
  DaliExpects(ckt_ptr_ != nullptr, "Circuit must be set before row width");
  max_row_width_ = std::floor(max_row_width_microns / ckt_ptr_->GridValueX());
  LOG(info) << "Max row width in grid unit : " << max_row_width_ << "\n";
}

void GriddedCellWellLegalizer::InitializeWellLegalizer(
    int cluster_width, bool apply_banded_assignment) {
  if (disable_welltap_) {
    well_tap_count_per_cluster_ = 0;
    LOG(info) << "set number of tap cells to 0, since well tap is disabled\n";
  }

  CheckWellStatus();

  // fetch parameters related to N/P-well
  FetchNpWellParams();

  space_partitioner_.SetCircuit(ckt_ptr_);
  space_partitioner_.SetOutput(&col_list_);
  if (disable_welltap_) {
    space_partitioner_.SetReservedSpaceToBoundaries(0, 0, 0, 0);
  } else {
    space_partitioner_.SetReservedSpaceToBoundaries(well_spacing_,
                                                    well_spacing_, 1, 1);
  }
  space_partitioner_.SetPartitionMode(stripe_mode_);
  space_partitioner_.SetAdaptiveStripeBoundaries(
      enable_adaptive_stripe_boundaries_, BuildGriddedCapacityConfig(1.0));
  space_partitioner_.SetAdaptiveBoundaryBlend(adaptive_boundary_blend_);
  space_partitioner_.SetColumnBoundaries(stripe_boundaries_override_);
  if (cluster_width >= 0) {
    space_partitioner_.SetMaxRowWidth(cluster_width);
  } else {
    space_partitioner_.SetMaxRowWidth(max_row_width_);
  }
  space_partitioner_.StartPartitioning();

  if (enable_banded_stripe_assignment_ && apply_banded_assignment) {
    BandedStripeAssignmentConfig config;
    config.band_count = banded_stripe_assignment_band_count_;
    config.minimum_projected_hpwl_improvement =
        banded_stripe_assignment_min_hpwl_improvement_;
    config.net_ignore_threshold = ortools_net_ignore_threshold_;
    config.capacity = BuildGriddedCapacityConfig(1.0);
    const BandedStripeAssignmentResult result =
        BandedStripeAssigner(ckt_ptr_, config).Assign(&col_list_);
    LOG(info) << "  Banded stripe assignment:\n"
              << "    configured bands           : " << config.band_count
              << "\n"
              << "    populated bands            : "
              << result.populated_band_count << "\n"
              << "    minimum projected HPWL gain: "
              << config.minimum_projected_hpwl_improvement << "um\n"
              << "    assigned components        : "
              << result.assigned_component_count << "\n"
              << "    proposed stripe moves      : "
              << result.proposed_move_count << "\n"
              << "    changed stripe columns     : "
              << result.moved_component_count << "\n"
              << "    rejected by HPWL/capacity  : "
              << result.rejected_hpwl_move_count << " / "
              << result.rejected_capacity_move_count << "\n"
              << "    initially overloaded targets: "
              << result.initially_overloaded_target_count << "\n"
              << "    projected HPWL improvement : "
              << result.projected_hpwl_improvement << "um\n"
              << "    average column displacement: "
              << result.average_column_displacement << "\n"
              << "    maximum column displacement: "
              << result.maximum_column_displacement << "\n";
    RecordPlacementMetric("well_legalization.banded_assignment.moved",
                          result.moved_component_count);
    RecordPlacementMetric("well_legalization.banded_assignment.proposed",
                          result.proposed_move_count);
    RecordPlacementMetric("well_legalization.banded_assignment.rejected_hpwl",
                          result.rejected_hpwl_move_count);
    RecordPlacementMetric(
        "well_legalization.banded_assignment.initially_overloaded_targets",
        result.initially_overloaded_target_count);
    RecordPlacementMetric(
        "well_legalization.banded_assignment.rejected_capacity",
        result.rejected_capacity_move_count);
    RecordPlacementMetric(
        "well_legalization.banded_assignment.projected_hpwl_improvement",
        result.projected_hpwl_improvement);
    RecordPlacementMetric(
        "well_legalization.banded_assignment.average_column_displacement",
        result.average_column_displacement);
    RecordPlacementMetric(
        "well_legalization.banded_assignment.maximum_column_displacement",
        result.maximum_column_displacement);
  }

  index_loc_list_.resize(ckt_ptr_->Components().size());
}

double GriddedCellWellLegalizer::ProvisionalOverflowArea() const {
  double overflow_area = 0.0;
  for (const StripeColumn& column : col_list_) {
    for (const Stripe& stripe : column.stripe_list_) {
      int overflow_height = std::max(0, stripe.used_height_ - stripe.Height());
      overflow_area += static_cast<double>(overflow_height) * stripe.Width();
    }
  }
  return overflow_area;
}

std::vector<std::vector<int>>
GriddedCellWellLegalizer::CollectProvisionalComponentRows() const {
  std::vector<std::vector<int>> component_rows;
  for (const StripeColumn& column : col_list_) {
    for (const Stripe& stripe : column.stripe_list_) {
      for (const GriddedRow& row : stripe.gridded_rows_) {
        if (row.Components().size() < 2) continue;
        std::vector<const Component*> ordered_components(
            row.Components().begin(), row.Components().end());
        std::sort(ordered_components.begin(), ordered_components.end(),
                  [](const Component* first, const Component* second) {
                    if (first->LLX() != second->LLX()) {
                      return first->LLX() < second->LLX();
                    }
                    return first->Id() < second->Id();
                  });
        std::vector<int> component_ids;
        component_ids.reserve(ordered_components.size());
        for (const Component* component : ordered_components) {
          component_ids.push_back(component->Id());
        }
        component_rows.push_back(std::move(component_ids));
      }
    }
  }
  return component_rows;
}

void GriddedCellWellLegalizer::ApplyProvisionalRowOrientations() {
  if (!disable_cell_flip_) {
    UpdateClusterOrient();
  }
}

void GriddedCellWellLegalizer::RefineProvisionalRowLocations(
    double hpwl_before_orientation) {
  const double hpwl_after_orientation = WeightedHPWL();

  GriddedRowLocationResult row_location;
  if (enable_row_location_optimization_) {
    row_location = GriddedRowLocationOptimizer(ckt_ptr_).Optimize(&col_list_);
  }
  LOG(info) << "    provisional row geometry: orientation "
            << (disable_cell_flip_ ? "disabled" : "applied")
            << ", row groups moved " << row_location.groups_moved << ", HPWL "
            << hpwl_before_orientation << " -> " << hpwl_after_orientation
            << " -> " << WeightedHPWL() << "um\n";
}

ProvisionalGriddedPlacementResult
GriddedCellWellLegalizer::RunProvisionalPlacement(
    bool enable_overflow_balancing, bool refine_row_geometry) {
  const std::vector<ComponentPlacementSnapshot> incoming_placement =
      CaptureComponentPlacement();
  const int configured_stripe_mode = stripe_mode_;
  const bool configured_adaptive_boundaries =
      enable_adaptive_stripe_boundaries_;
  // Final legalization selects adaptive geometry using exact legal HPWL. Keep
  // provisional upper bounds uniform so rejected geometry cannot feed back
  // into the analytical placement.
  enable_adaptive_stripe_boundaries_ = false;

  auto run_clustering = [this]() {
    // Keep the first experiment isolated to final legalization. Applying the
    // nonlinear map during global feedback is a separate scheduling question.
    InitializeWellLegalizer(-1, false);
    return ComponentClusteringLoose();
  };

  ProvisionalGriddedPlacementResult result;
  result.feasible = run_clustering();
  result.overflow = ProvisionalOverflowArea();
  result.violations = last_clustering_violations_;
  result.initial_overflow = result.overflow;
  result.initial_violations = result.violations;
  result.feasible = result.feasible && result.overflow == 0.0;

  if (!result.feasible && enable_overflow_balancing) {
    result.feasible =
        TryBalanceProvisionalPlacement(incoming_placement, &result);
  }

  if (!result.feasible &&
      configured_stripe_mode != int(WellPartitionMode::kScavenge)) {
    RestoreComponentPlacement(incoming_placement);
    stripe_mode_ = int(WellPartitionMode::kScavenge);
    result.used_scavenge = true;
    result.feasible = run_clustering();
    result.overflow = ProvisionalOverflowArea();
    result.violations = last_clustering_violations_;
    result.feasible = result.feasible && result.overflow == 0.0;
  }

  stripe_mode_ = configured_stripe_mode;
  enable_adaptive_stripe_boundaries_ = configured_adaptive_boundaries;
  if (result.feasible) {
    const double hpwl_before_orientation = WeightedHPWL();
    ApplyProvisionalRowOrientations();
    if (refine_row_geometry) {
      RefineProvisionalRowLocations(hpwl_before_orientation);
    }
    result.hpwl = WeightedHPWL();
    result.component_rows = CollectProvisionalComponentRows();
  } else {
    RestoreComponentPlacement(incoming_placement);
  }
  return result;
}

bool GriddedCellWellLegalizer::TryBalanceProvisionalPlacement(
    const std::vector<ComponentPlacementSnapshot>& incoming_placement,
    ProvisionalGriddedPlacementResult* result) {
  DaliExpects(result != nullptr,
              "Cannot save provisional balancing into a null result");
  GriddedCapacityConfig capacity_config = BuildGriddedCapacityConfig(1.0);
  unsigned long long previous_overflow =
      std::numeric_limits<unsigned long long>::max();
  std::vector<int> moved_component_ids;
  int max_rounds = std::max(1, static_cast<int>(col_list_.size()));
  for (int round = 0; round < max_rounds; ++round) {
    RestoreComponentPlacement(incoming_placement);
    GriddedStripeBalanceResult balance =
        GriddedStripeBalancer(ckt_ptr_, capacity_config)
            .BalanceObservedOverflow(&col_list_);
    LOG(info) << "    provisional stripe balancing round " << round + 1
              << ": moved " << balance.moved_component_count
              << " components, overflow " << balance.overflow_area_before
              << " -> " << balance.overflow_area_after << "\n";
    result->balanced_component_count += balance.moved_component_count;
    moved_component_ids.insert(moved_component_ids.end(),
                               balance.moved_component_ids.begin(),
                               balance.moved_component_ids.end());
    if (balance.moved_component_count == 0 ||
        balance.overflow_area_before >= previous_overflow) {
      return false;
    }
    previous_overflow = balance.overflow_area_before;

    result->feasible = ComponentClusteringLoose();
    result->overflow = ProvisionalOverflowArea();
    result->violations = last_clustering_violations_;
    result->feasible = result->feasible && result->overflow == 0.0;
    if (result->feasible) {
      std::sort(moved_component_ids.begin(), moved_component_ids.end());
      moved_component_ids.erase(
          std::unique(moved_component_ids.begin(), moved_component_ids.end()),
          moved_component_ids.end());
      result->balanced_component_ids = std::move(moved_component_ids);
      return true;
    }
  }
  return false;
}

void GriddedCellWellLegalizer::ClearProvisionalState() {
  col_list_.clear();
  index_loc_list_.clear();
}

int GriddedCellWellLegalizer::PhysicalCompletionReservedWidth() const {
  return PhysicalCompletionLeftMargin() + PhysicalCompletionRightMargin();
}

int GriddedCellWellLegalizer::PhysicalCompletionLeftMargin() const {
  int reserved_width = 0;
  if (!disable_welltap_) {
    reserved_width = well_tap_width_ + space_to_well_tap_;
  }
  if (enable_end_cap_cell_) {
    reserved_width += pre_end_cap_min_width_;
  }
  return reserved_width;
}

int GriddedCellWellLegalizer::PhysicalCompletionRightMargin() const {
  int reserved_width = 0;
  if (!disable_welltap_) {
    reserved_width = well_tap_width_ + space_to_well_tap_;
  }
  if (enable_end_cap_cell_) {
    reserved_width += post_end_cap_min_width_;
  }
  return reserved_width;
}

void GriddedCellWellLegalizer::ReservePhysicalCompletionSpace(
    GriddedRow* row, bool grows_upward) {
  if (row == nullptr) {
    return;
  }

  row->SetBoundaryMargins(PhysicalCompletionLeftMargin(),
                          PhysicalCompletionRightMargin());

  if (grows_upward) {
    row->UpdateWellHeightUpward(well_tap_p_height_, well_tap_n_height_);
    if (enable_end_cap_cell_) {
      row->UpdateWellHeightUpward(
          std::max(pre_end_cap_min_p_height_, post_end_cap_min_p_height_),
          std::max(pre_end_cap_min_n_height_, post_end_cap_min_n_height_));
    }
  } else {
    row->UpdateWellHeightDownward(well_tap_p_height_, well_tap_n_height_);
    if (enable_end_cap_cell_) {
      row->UpdateWellHeightDownward(
          std::max(pre_end_cap_min_p_height_, post_end_cap_min_p_height_),
          std::max(pre_end_cap_min_n_height_, post_end_cap_min_n_height_));
    }
  }
}

void GriddedCellWellLegalizer::EnsureUsableEndCapWidths() {
  if (pre_end_cap_min_width_ > 0 && post_end_cap_min_width_ > 0) {
    return;
  }

  int fallback_width = well_tap_width_;
  if (pre_end_cap_min_width_ <= 0) {
    LOG(warning) << "  pre-end-cap width is not provided; use "
                 << fallback_width
                 << " grid units for generated end-cap cells\n";
    pre_end_cap_min_width_ = fallback_width;
  }
  if (post_end_cap_min_width_ <= 0) {
    LOG(warning) << "  post-end-cap width is not provided; use "
                 << fallback_width
                 << " grid units for generated end-cap cells\n";
    post_end_cap_min_width_ = fallback_width;
  }
}

int GriddedCellWellLegalizer::LeftTapLx(const Stripe& stripe) const {
  int end_cap_width = enable_end_cap_cell_ ? pre_end_cap_min_width_ : 0;
  return stripe.LLX() + end_cap_width;
}

int GriddedCellWellLegalizer::LeftTapUx(const Stripe& stripe) const {
  return LeftTapLx(stripe) + well_tap_width_;
}

int GriddedCellWellLegalizer::RightTapUx(const Stripe& stripe) const {
  int end_cap_width = enable_end_cap_cell_ ? post_end_cap_min_width_ : 0;
  return stripe.URX() - end_cap_width;
}

int GriddedCellWellLegalizer::RightTapLx(const Stripe& stripe) const {
  return RightTapUx(stripe) - well_tap_width_;
}

WellRowCompletionConfig GriddedCellWellLegalizer::BuildRowCompletionConfig()
    const {
  WellRowCompletionConfig config;
  config.well_tap_macro = well_tap_macro_;
  config.well_tap_count_per_row = well_tap_count_per_cluster_;
  config.space_to_well_tap = space_to_well_tap_;
  if (enable_end_cap_cell_) {
    config.pre_end_cap_width = pre_end_cap_min_width_;
    config.post_end_cap_width = post_end_cap_min_width_;
  }
  return config;
}

void GriddedCellWellLegalizer::CreateClusterAndAppendSingleWellComponent(
    Stripe& stripe, Component& component) {
  stripe.gridded_rows_.emplace_back();
  GriddedRow* front_row = &(stripe.gridded_rows_.back());
  front_row->Components().reserve(stripe.max_component_capacity_per_cluster_);
  front_row->AddComponent(&component);

  int width = component.Width();
  int init_y = (int)std::round(component.LLY());
  init_y = std::max(init_y, stripe.contour_);

  int p_well_height = component.MacroPtr()->FirstPwellHeight();
  int n_well_height = component.MacroPtr()->FirstNwellHeight();

  front_row->SetUsedSize(PhysicalCompletionReservedWidth() + width);
  ReservePhysicalCompletionSpace(front_row, true);
  // row height should be able to accommodate ordinary cell
  front_row->UpdateWellHeightUpward(p_well_height, n_well_height);
  front_row->SetLLY(init_y);
  front_row->SetLLX(stripe.LLX());
  front_row->SetWidth(stripe.Width());

  stripe.front_row_ = front_row;
  stripe.cluster_count_ += 1;
  stripe.used_height_ += front_row->Height();
  stripe.contour_ = front_row->URY();
}

void GriddedCellWellLegalizer::AppendSingleWellComponentToFrontCluster(
    Stripe& stripe, Component& component) {
  int width = component.Width();
  int p_well_height = component.MacroPtr()->FirstPwellHeight();
  int n_well_height = component.MacroPtr()->FirstNwellHeight();

  GriddedRow* front_row = stripe.front_row_;
  front_row->AddComponent(&component);
  front_row->UseSpace(width);
  if (p_well_height > front_row->PHeight() ||
      n_well_height > front_row->NHeight()) {
    int old_height = front_row->Height();
    front_row->UpdateWellHeightUpward(p_well_height, n_well_height);
    stripe.used_height_ += front_row->Height() - old_height;
  }
  stripe.contour_ = front_row->URY();
}

void GriddedCellWellLegalizer::AppendComponentToColBottomUp(
    Stripe& stripe, Component& component) {
  bool is_no_row_in_col = (stripe.contour_ == stripe.LLY());
  bool is_new_row_needed = is_no_row_in_col;
  if (!is_new_row_needed) {
    GriddedRow* front_row = stripe.front_row_;
    bool is_not_in_top_row = stripe.contour_ <= component.LLY();
    bool is_top_row_full =
        front_row->UsedSize() + component.Width() > stripe.width_;
    is_new_row_needed = is_not_in_top_row || is_top_row_full;
  }

  if (is_new_row_needed) {
    CreateClusterAndAppendSingleWellComponent(stripe, component);
  } else {
    AppendSingleWellComponentToFrontCluster(stripe, component);
  }
}

void GriddedCellWellLegalizer::AppendComponentToColTopDown(
    Stripe& stripe, Component& component) {
  bool is_no_row = stripe.gridded_rows_.empty();
  bool is_new_row_needed = is_no_row;
  if (!is_new_row_needed) {
    bool is_not_in_top_row = stripe.contour_ >= component.URY();
    bool is_top_row_full =
        stripe.front_row_->UsedSize() + component.Width() > stripe.width_;
    is_new_row_needed = is_not_in_top_row || is_top_row_full;
  }

  int width = component.Width();
  int init_y = (int)std::round(component.URY());
  init_y = std::min(init_y, stripe.contour_);

  GriddedRow* front_row;
  int p_well_height = component.MacroPtr()->FirstPwellHeight();
  int n_well_height = component.MacroPtr()->FirstNwellHeight();
  if (is_new_row_needed) {
    stripe.gridded_rows_.emplace_back();
    front_row = &(stripe.gridded_rows_.back());
    front_row->Components().reserve(stripe.max_component_capacity_per_cluster_);
    front_row->AddComponent(&component);
    front_row->SetUsedSize(PhysicalCompletionReservedWidth() + width);
    ReservePhysicalCompletionSpace(front_row, false);
    front_row->UpdateWellHeightDownward(p_well_height, n_well_height);
    front_row->SetURY(init_y);
    front_row->SetLLX(stripe.LLX());
    front_row->SetWidth(stripe.Width());

    stripe.front_row_ = front_row;
    stripe.cluster_count_ += 1;
    stripe.used_height_ += front_row->Height();
  } else {
    front_row = stripe.front_row_;
    front_row->AddComponent(&component);
    front_row->UseSpace(width);
    if (p_well_height > front_row->PHeight() ||
        n_well_height > front_row->NHeight()) {
      int old_height = front_row->Height();
      front_row->UpdateWellHeightDownward(p_well_height, n_well_height);
      stripe.used_height_ += front_row->Height() - old_height;
    }
  }
  stripe.contour_ = front_row->LLY();
}

void GriddedCellWellLegalizer::AppendComponentToColBottomUpCompact(
    Stripe& stripe, Component& component) {
  bool is_new_cluster_needed = (stripe.contour_ == stripe.LLY());
  if (!is_new_cluster_needed) {
    bool is_top_cluster_full =
        stripe.front_row_->UsedSize() + component.Width() > stripe.width_;
    is_new_cluster_needed = is_top_cluster_full;
  }

  int width = component.Width();
  int init_y = (int)std::round(component.LLY());
  init_y = std::max(init_y, stripe.contour_);

  GriddedRow* front_cluster;
  int p_well_height = component.MacroPtr()->FirstPwellHeight();
  int n_well_height = component.MacroPtr()->FirstNwellHeight();
  if (is_new_cluster_needed) {
    stripe.gridded_rows_.emplace_back();
    front_cluster = &(stripe.gridded_rows_.back());
    front_cluster->Components().reserve(
        stripe.max_component_capacity_per_cluster_);
    front_cluster->AddComponent(&component);
    front_cluster->SetUsedSize(PhysicalCompletionReservedWidth() + width);
    ReservePhysicalCompletionSpace(front_cluster, true);
    front_cluster->SetLLY(init_y);
    front_cluster->SetLLX(stripe.LLX());
    front_cluster->SetWidth(stripe.Width());
    front_cluster->UpdateWellHeightUpward(p_well_height, n_well_height);

    stripe.front_row_ = front_cluster;
    stripe.cluster_count_ += 1;
    stripe.used_height_ += front_cluster->Height();
  } else {
    front_cluster = stripe.front_row_;
    front_cluster->AddComponent(&component);
    front_cluster->UseSpace(width);
    if (p_well_height > front_cluster->PHeight() ||
        n_well_height > front_cluster->NHeight()) {
      int old_height = front_cluster->Height();
      front_cluster->UpdateWellHeightUpward(p_well_height, n_well_height);
      stripe.used_height_ += front_cluster->Height() - old_height;
    }
  }
  stripe.contour_ = front_cluster->URY();
}

void GriddedCellWellLegalizer::AppendComponentToColTopDownCompact(
    Stripe& stripe, Component& component) {
  bool is_new_cluster_needed = (stripe.contour_ == stripe.URY());
  if (!is_new_cluster_needed) {
    bool is_top_cluster_full =
        stripe.front_row_->UsedSize() + component.Width() > stripe.width_;
    is_new_cluster_needed = is_top_cluster_full;
  }

  int width = component.Width();
  int init_y = (int)std::round(component.URY());
  init_y = std::min(init_y, stripe.contour_);

  GriddedRow* front_cluster;
  int p_well_height = component.MacroPtr()->FirstPwellHeight();
  int n_well_height = component.MacroPtr()->FirstNwellHeight();
  if (is_new_cluster_needed) {
    stripe.gridded_rows_.emplace_back();
    front_cluster = &(stripe.gridded_rows_.back());
    front_cluster->Components().reserve(
        stripe.max_component_capacity_per_cluster_);
    front_cluster->AddComponent(&component);
    front_cluster->SetUsedSize(PhysicalCompletionReservedWidth() + width);
    ReservePhysicalCompletionSpace(front_cluster, false);
    front_cluster->UpdateWellHeightDownward(p_well_height, n_well_height);
    front_cluster->SetURY(init_y);
    front_cluster->SetLLX(stripe.LLX());
    front_cluster->SetWidth(stripe.Width());

    stripe.front_row_ = front_cluster;
    stripe.cluster_count_ += 1;
    stripe.used_height_ += front_cluster->Height();
  } else {
    front_cluster = stripe.front_row_;
    front_cluster->AddComponent(&component);
    front_cluster->UseSpace(width);
    if (p_well_height > front_cluster->PHeight() ||
        n_well_height > front_cluster->NHeight()) {
      int old_height = front_cluster->Height();
      front_cluster->UpdateWellHeightDownward(p_well_height, n_well_height);
      stripe.used_height_ += front_cluster->Height() - old_height;
    }
  }
  stripe.contour_ = front_cluster->LLY();
}

bool GriddedCellWellLegalizer::StripeLegalizationBottomUp(Stripe& stripe) {
  stripe.gridded_rows_.clear();
  stripe.contour_ = stripe.LLY();
  stripe.used_height_ = 0;
  stripe.cluster_count_ = 0;
  stripe.front_row_ = nullptr;
  stripe.is_bottom_up_ = true;

  std::sort(stripe.component_ptrs_vec_.begin(),
            stripe.component_ptrs_vec_.end(),
            [](const Component* lhs, const Component* rhs) {
              return (lhs->LLY() < rhs->LLY()) ||
                     (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });
  for (auto& component_ptr : stripe.component_ptrs_vec_) {
    if (component_ptr->IsFixed()) continue;
    AppendComponentToColBottomUp(stripe, *component_ptr);
  }

  for (auto& gridded_row : stripe.gridded_rows_) {
    gridded_row.UpdateComponentLocY();
  }

  return stripe.HasNoRowsSpillingOut();
}

bool GriddedCellWellLegalizer::StripeLegalizationTopDown(Stripe& stripe) {
  stripe.gridded_rows_.clear();
  stripe.contour_ = stripe.URY();
  stripe.used_height_ = 0;
  stripe.cluster_count_ = 0;
  stripe.front_row_ = nullptr;
  stripe.is_bottom_up_ = false;

  std::sort(stripe.component_ptrs_vec_.begin(),
            stripe.component_ptrs_vec_.end(),
            [](const Component* lhs, const Component* rhs) {
              return (lhs->URY() > rhs->URY()) ||
                     (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
            });
  for (auto& component_ptr : stripe.component_ptrs_vec_) {
    if (component_ptr->IsFixed()) continue;
    AppendComponentToColTopDown(stripe, *component_ptr);
  }

  for (auto& gridded_row : stripe.gridded_rows_) {
    gridded_row.UpdateComponentLocY();
  }

  /*LOG(info)   << "Reverse clustering: ";
  if (stripe.contour_ >= RegionLLY()) {
    LOG(info)   << "success\n";
  } else {
    LOG(info)   << "fail\n";
  }*/

  return stripe.HasNoRowsSpillingOut();
}

bool GriddedCellWellLegalizer::StripeLegalizationBottomUpCompact(
    Stripe& stripe) {
  stripe.gridded_rows_.clear();
  stripe.contour_ = RegionBottom();
  stripe.used_height_ = 0;
  stripe.cluster_count_ = 0;
  stripe.front_row_ = nullptr;
  stripe.is_bottom_up_ = true;

  std::sort(stripe.component_ptrs_vec_.begin(),
            stripe.component_ptrs_vec_.end(),
            [](const Component* lhs, const Component* rhs) {
              return (lhs->LLY() < rhs->LLY()) ||
                     (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });
  for (auto& component_ptr : stripe.component_ptrs_vec_) {
    if (component_ptr->IsFixed()) continue;
    AppendComponentToColBottomUpCompact(stripe, *component_ptr);
  }

  for (auto& cluster : stripe.gridded_rows_) {
    cluster.UpdateComponentLocY();
  }

  return stripe.contour_ <= RegionTop();
}

bool GriddedCellWellLegalizer::StripeLegalizationTopDownCompact(
    Stripe& stripe) {
  stripe.gridded_rows_.clear();
  stripe.contour_ = stripe.URY();
  stripe.used_height_ = 0;
  stripe.cluster_count_ = 0;
  stripe.front_row_ = nullptr;
  stripe.is_bottom_up_ = false;

  std::sort(stripe.component_ptrs_vec_.begin(),
            stripe.component_ptrs_vec_.end(),
            [](const Component* lhs, const Component* rhs) {
              return (lhs->URY() > rhs->URY()) ||
                     (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
            });
  for (auto& component_ptr : stripe.component_ptrs_vec_) {
    if (component_ptr->IsFixed()) continue;
    AppendComponentToColTopDownCompact(stripe, *component_ptr);
  }

  for (auto& cluster : stripe.gridded_rows_) {
    cluster.UpdateComponentLocY();
  }

  /*LOG(info)   << "Reverse clustering: ";
  if (stripe.contour_ >= RegionLLY()) {
    LOG(info)   << "success\n";
  } else {
    LOG(info)   << "fail\n";
  }*/

  return stripe.contour_ >= RegionBottom();
}

bool GriddedCellWellLegalizer::ComponentClustering() {
  /****
   * Clustering components in each stripe
   * After clustering, close pack clusters from bottom to top
   ****/
  bool res = true;
  for (auto& col : col_list_) {
    bool is_success = true;
    for (auto& stripe : col.stripe_list_) {
      for (int i = 0; i < max_iter_; ++i) {
        is_success = StripeLegalizationBottomUp(stripe);
        if (!is_success) {
          is_success = StripeLegalizationTopDown(stripe);
        }
      }
      res = res && is_success;

      // closely pack clusters from bottom to top
      stripe.contour_ = stripe.LLY();
      if (stripe.is_bottom_up_) {
        for (auto& cluster : stripe.gridded_rows_) {
          stripe.contour_ += cluster.Height();
          cluster.SetLLY(stripe.contour_);
          cluster.UpdateComponentLocY();
          cluster.LegalizeCompactX();
        }
      } else {
        int sz = static_cast<int>(stripe.gridded_rows_.size());
        for (int i = sz - 1; i >= 0; --i) {
          auto& cluster = stripe.gridded_rows_[i];
          stripe.contour_ += cluster.Height();
          cluster.SetLLY(stripe.contour_);
          cluster.UpdateComponentLocY();
          cluster.LegalizeCompactX();
        }
      }
    }
  }
  return res;
}

/****
 * Clustering components in each stripe
 * After clustering, leave clusters as they are
 * ****/
bool GriddedCellWellLegalizer::ComponentClusteringLoose() {
  int step = 50;
  int count = 0;
  bool res = true;
  int failed_stripe_count = 0;
  last_clustering_violations_.clear();
  for (int col_id = 0; col_id < static_cast<int>(col_list_.size()); ++col_id) {
    auto& col = col_list_[col_id];
    bool is_success = true;
    for (int stripe_id = 0;
         stripe_id < static_cast<int>(col.stripe_list_.size()); ++stripe_id) {
      auto& stripe = col.stripe_list_[stripe_id];
      int i = 0;
      bool is_from_bottom = true;
      for (i = 0; i < max_iter_; ++i) {
        if (is_from_bottom) {
          is_success = StripeLegalizationBottomUp(stripe);
        } else {
          is_success = StripeLegalizationTopDown(stripe);
        }
        if (!is_success) {
          is_success = TrialClusterLegalization(stripe);
        }
        is_from_bottom = !is_from_bottom;
        if (is_success) {
          break;
        }
      }
      res = res && is_success;
      if (!is_success) {
        ++failed_stripe_count;
        RecordStripeLegalizationFailure(stripe);
        LogStripeLegalizationFailure(col, stripe, col_id, stripe_id);
      }
      /*if (is_success) {
        LOG(info)  <<"stripe legalization success, %d\n", i);
      } else {
        LOG(info)  <<"stripe legalization fail, %d\n", i);
      }*/

      for (auto& row : stripe.gridded_rows_) {
        row.UpdateComponentLocY();
        row.MinDisplacementLegalization();
        if (is_dump) {
          if (count % step == 0) {
            std::string tmp_file_name =
                "wlg_result_" + std::to_string(dump_count) + ".txt";
            ckt_ptr_->GenMATLABTable(tmp_file_name);
            ++dump_count;
          }
          ++count;
        }
      }
      stripe.MinDisplacementAdjustment();
      if (is_success && stripe.used_height_ > stripe.Height()) {
        res = false;
        ++failed_stripe_count;
        RecordStripeLegalizationFailure(stripe);
        LogStripeLegalizationFailure(col, stripe, col_id, stripe_id);
      }
    }
  }

  const size_t overlap_count = CountComponentOverlapsInRows();
  LogComponentClusteringSummary(failed_stripe_count);
  if (overlap_count > 0) res = false;
  return res;
}

void GriddedCellWellLegalizer::RecordStripeLegalizationFailure(
    const Stripe& stripe) {
  ProvisionalGriddedPlacementViolation violation;
  violation.lx = stripe.LLX();
  violation.ly = stripe.LLY();
  violation.ux = stripe.URX();
  violation.uy = stripe.URY();
  violation.overflow_height =
      std::max(0, stripe.used_height_ - stripe.Height());
  violation.component_ids.reserve(stripe.component_ptrs_vec_.size());
  for (const Component* component : stripe.component_ptrs_vec_) {
    violation.component_ids.push_back(component->Id());
  }
  last_clustering_violations_.push_back(std::move(violation));
}

void GriddedCellWellLegalizer::LogStripeLegalizationFailure(
    const StripeColumn& col, const Stripe& stripe, int column_index,
    int stripe_index) const {
  int lowest_row_y = std::numeric_limits<int>::max();
  int highest_row_y = std::numeric_limits<int>::min();
  for (const auto& row : stripe.gridded_rows_) {
    lowest_row_y = std::min(lowest_row_y, row.LLY());
    highest_row_y = std::max(highest_row_y, row.URY());
  }
  if (stripe.gridded_rows_.empty()) {
    lowest_row_y = stripe.LLY();
    highest_row_y = stripe.LLY();
  }

  int lower_overflow = std::max(0, stripe.LLY() - lowest_row_y);
  int upper_overflow = std::max(0, highest_row_y - stripe.URY());
  int height_overflow = std::max(0, stripe.used_height_ - stripe.Height());
  double grid_y = ckt_ptr_->GridValueY();

  LOG(warning) << "  stripe legalization failed:"
               << " col=" << column_index << " stripe=" << stripe_index
               << " col_x=[" << col.LLX() << ", " << col.URX() << ")"
               << " stripe_box=[" << stripe.LLX() << ", " << stripe.LLY()
               << "]-[" << stripe.URX() << ", " << stripe.URY() << ")"
               << " components=" << stripe.component_ptrs_vec_.size()
               << " rows=" << stripe.gridded_rows_.size()
               << " used_height=" << stripe.used_height_
               << " capacity_height=" << stripe.Height()
               << " overflow_height=" << height_overflow << " row_y=["
               << lowest_row_y << ", " << highest_row_y << ")"
               << " lower_overflow=" << lower_overflow
               << " upper_overflow=" << upper_overflow << " grid units, "
               << "overflow_height=" << height_overflow * grid_y << "um\n";
}

void GriddedCellWellLegalizer::LogComponentClusteringSummary(
    int failed_stripe_count) const {
  size_t overlap_count = CountComponentOverlapsInRows();
  if (failed_stripe_count == 0) {
    LOG(info) << "  component clustering: all stripes legalized\n";
    if (overlap_count > 0) {
      LOG(warning) << "  component clustering produced " << overlap_count
                   << " overlapping component pair(s)\n";
    }
    return;
  }
  LOG(warning) << "  component clustering failed in " << failed_stripe_count
               << " stripe(s)\n";
  if (overlap_count > 0) {
    LOG(warning) << "  component clustering has " << overlap_count
                 << " overlapping component pair(s)\n";
  }
}

size_t GriddedCellWellLegalizer::CountComponentOverlapsInRows() const {
  size_t overlap_count = 0;
  for (const auto& col : col_list_) {
    for (const auto& stripe : col.stripe_list_) {
      for (const auto& row : stripe.gridded_rows_) {
        overlap_count += row.CountComponentOverlaps();
      }
    }
  }
  return overlap_count;
}

bool GriddedCellWellLegalizer::ValidateFinalPlacement() const {
  GriddedPlacementValidationConfig config;
  config.check_component_orientation = !disable_cell_flip_;
  config.expect_well_taps = !disable_welltap_;
  config.expect_end_caps = enable_end_cap_cell_;
  config.space_to_well_tap = space_to_well_tap_;
  if (enable_end_cap_cell_) {
    config.pre_end_cap_width = pre_end_cap_min_width_;
    config.post_end_cap_width = post_end_cap_min_width_;
  }
  const GriddedPlacementLegalityReport report =
      GriddedPlacementValidator(ckt_ptr_, &col_list_, config).Validate();

  LOG(info)
      << "Final gridded placement legality:\n"
      << "  legal                         : " << report.IsLegal() << "\n"
      << "  total violations              : " << report.TotalViolationCount()
      << "\n"
      << "  movable / assigned components: " << report.movable_component_count
      << " / " << report.assigned_component_count << "\n"
      << "  unassigned / duplicate        : "
      << report.unassigned_component_count << " / "
      << report.duplicate_assignment_count << "\n"
      << "  invalid references            : "
      << report.invalid_component_reference_count << "\n"
      << "  row boundary / overlap        : "
      << report.row_boundary_violation_count << " / "
      << report.row_overlap_count << "\n"
      << "  component boundary / overlap  : "
      << report.component_boundary_violation_count << " / "
      << report.component_overlap_count << "\n"
      << "  component Y / orientation     : "
      << report.component_y_violation_count << " / "
      << report.component_orientation_violation_count << "\n"
      << "  physical completion           : "
      << report.physical_completion_violation_count << "\n"
      << "    missing taps / tap geometry : " << report.missing_well_tap_count
      << " / " << report.well_tap_geometry_violation_count << "\n"
      << "    tap spacing                 : "
      << report.well_tap_spacing_violation_count << "\n"
      << "    missing caps / cap geometry : " << report.missing_end_cap_count
      << " / " << report.end_cap_geometry_violation_count << "\n"
      << "    cap/tap overlap / count     : "
      << report.end_cap_tap_overlap_count << " / "
      << report.physical_component_count_violation_count << "\n";
  RecordPlacementMetric("well_legalization.legality.legal",
                        report.IsLegal() ? 1.0 : 0.0);
  RecordPlacementMetric("well_legalization.legality.total_violations",
                        report.TotalViolationCount());
  RecordPlacementMetric("well_legalization.legality.unassigned_components",
                        report.unassigned_component_count);
  RecordPlacementMetric("well_legalization.legality.duplicate_assignments",
                        report.duplicate_assignment_count);
  RecordPlacementMetric("well_legalization.legality.row_overlaps",
                        report.row_overlap_count);
  RecordPlacementMetric("well_legalization.legality.component_overlaps",
                        report.component_overlap_count);
  RecordPlacementMetric(
      "well_legalization.legality.physical_completion_violations",
      report.physical_completion_violation_count);
  return report.IsLegal();
}

bool GriddedCellWellLegalizer::ComponentClusteringCompact() {
  /****
   * Clustering components in each stripe in a compact way
   * After clustering, leave clusters as they are
   * ****/

  bool res = true;
  for (auto& col : col_list_) {
    bool is_success = true;
    for (auto& stripe : col.stripe_list_) {
      for (int i = 0; i < max_iter_; ++i) {
        is_success = StripeLegalizationBottomUpCompact(stripe);
        if (!is_success) {
          is_success = StripeLegalizationTopDownCompact(stripe);
        }
      }
      res = res && is_success;

      for (auto& cluster : stripe.gridded_rows_) {
        cluster.UpdateComponentLocY();
        cluster.LegalizeLooseX();
      }
    }
  }

  return res;
}

bool GriddedCellWellLegalizer::TrialClusterLegalization(Stripe& stripe) {
  /****
   * Legalize the location of all clusters using extended Tetris legalization
   * algorithm in columns where usage does not exceed capacity Closely pack the
   * column from bottom to top if its usage exceeds its capacity
   * ****/

  bool res = true;

  // sort clusters in each column based on the lower left coordinate
  std::vector<GriddedRow*> cluster_list;
  cluster_list.resize(stripe.cluster_count_, nullptr);
  for (int i = 0; i < stripe.cluster_count_; ++i) {
    cluster_list[i] = &stripe.gridded_rows_[i];
  }

  // LOG(info)   << "used height/RegionHeight(): " <<
  // col.used_height_ / (double) RegionHeight() << "\n";
  if (stripe.used_height_ <= RegionHeight()) {
    if (stripe.is_bottom_up_) {
      std::sort(cluster_list.begin(), cluster_list.end(),
                [](const GriddedRow* lhs, const GriddedRow* rhs) {
                  return (lhs->URY() > rhs->URY());
                });
      int cluster_contour = stripe.URY();
      int res_y;
      int init_y;
      for (auto& cluster : cluster_list) {
        init_y = cluster->URY();
        res_y = std::min(cluster_contour, cluster->URY());
        cluster->SetURY(res_y);
        cluster_contour = cluster->LLY();
        cluster->ShiftComponentY(res_y - init_y);
      }
    } else {
      std::sort(cluster_list.begin(), cluster_list.end(),
                [](const GriddedRow* lhs, const GriddedRow* rhs) {
                  return (lhs->LLY() < rhs->LLY());
                });
      int cluster_contour = stripe.LLY();
      int res_y;
      int init_y;
      for (auto& cluster : cluster_list) {
        init_y = cluster->LLY();
        res_y = std::max(cluster_contour, cluster->LLY());
        cluster->SetLLY(res_y);
        cluster_contour = cluster->URY();
        cluster->ShiftComponentY(res_y - init_y);
      }
    }
  } else {
    std::sort(cluster_list.begin(), cluster_list.end(),
              [](const GriddedRow* lhs, const GriddedRow* rhs) {
                return (lhs->LLY() < rhs->LLY());
              });
    int cluster_contour = RegionBottom();
    int res_y;
    int init_y;
    for (auto& cluster : cluster_list) {
      init_y = cluster->LLY();
      res_y = cluster_contour;
      cluster->SetLLY(res_y);
      cluster_contour += cluster->Height();
      cluster->ShiftComponentY(res_y - init_y);
    }
    res = false;
  }

  return res;
}

/*
void GriddedCellWellLegalizer::SingleSegmentClusteringOptimization() {
  LOG(info) << "Start single segment clustering\n";

  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      for (auto &cluster: stripe.cluster_list_) {
        int old_cluster_component_count = cluster.components_.size();
        std::vector<ComponentSegment> old_cluster(old_cluster_component_count);
        for (int i = 0; i < old_cluster_component_count; ++i) {
          old_cluster[i].component_index.push_back(i);
          old_cluster[i].circuit_ptr_ = circuit_ptr_;
          old_cluster[i].cluster_ = &cluster;
          old_cluster[i].UpdateBoundList();
          old_cluster[i].SortBounds();
          old_cluster[i].UpdateLLX();
        }

        bool is_overlap = false;
        do {
          std::vector<ComponentSegment> new_cluster;
          new_cluster.push_back(old_cluster[0]);
          int j = 0;
          while (j + 1 < old_cluster_component_count) {
            if (new_cluster.back().Overlap(old_cluster[j + 1])) {
              new_cluster.back().Merge(old_cluster[j + 1]);
            } else {
              new_cluster.push_back(old_cluster[j + 1]);
            }
            j += 1;
          }
          int new_count = new_cluster.size();
          old_cluster_component_count = new_count;
          for (int i = 0; i < new_count; ++i) {
            old_cluster[i].CopyFrom(new_cluster[i]);
          }

          is_overlap = false;
          for (int i = 0; i < new_count - 1; ++i) {
            if (old_cluster[i].IsNotOnLeft(old_cluster[i + 1])) {
              is_overlap = true;
              break;
            }
          }

          //LOG(info)   << is_overlap << "\n";

        } while (is_overlap);

        for (int i = 0; i < old_cluster_component_count; ++i) {
          old_cluster[i].UpdateComponentLocation();
        }
      }
    }
  }

}
 */

void GriddedCellWellLegalizer::UpdateClusterOrient() {
  for (auto& col : col_list_) {
    ApplyColumnOrientationPhase(&col, is_first_row_orient_N_);
  }
}

void GriddedCellWellLegalizer::ApplyColumnOrientationPhase(
    StripeColumn* column, bool first_row_orient_n) {
  DaliExpects(column != nullptr, "Cannot orient a null stripe column");
  bool orient_n = first_row_orient_n;
  for (Stripe& stripe : column->stripe_list_) {
    stripe.is_first_row_orient_N_ = orient_n;
    if (stripe.is_bottom_up_) {
      for (GriddedRow& row : stripe.gridded_rows_) {
        row.SetOrient(orient_n);
        orient_n = !orient_n;
      }
    } else {
      for (int i = static_cast<int>(stripe.gridded_rows_.size()) - 1; i >= 0;
           --i) {
        stripe.gridded_rows_[i].SetOrient(orient_n);
        orient_n = !orient_n;
      }
    }
  }
}

double GriddedCellWellLegalizer::OptimizeColumnOrientationPhases() {
  constexpr int kMaxOrientationSweeps = 4;
  constexpr double kMinHpwlImprovement = 1e-9;
  std::vector<bool> first_row_orient_n;
  first_row_orient_n.reserve(col_list_.size());
  for (const StripeColumn& column : col_list_) {
    first_row_orient_n.push_back(
        column.stripe_list_.empty()
            ? is_first_row_orient_N_
            : column.stripe_list_.front().is_first_row_orient_N_);
  }
  double best_hpwl = WeightedHPWL();
  int flipped_column_count = 0;
  int completed_sweeps = 0;

  for (int sweep = 0; sweep < kMaxOrientationSweeps; ++sweep) {
    bool improved = false;
    for (size_t column_id = 0; column_id < col_list_.size(); ++column_id) {
      bool candidate_phase = !first_row_orient_n[column_id];
      ApplyColumnOrientationPhase(&col_list_[column_id], candidate_phase);
      double candidate_hpwl = WeightedHPWL();
      if (candidate_hpwl + kMinHpwlImprovement < best_hpwl) {
        first_row_orient_n[column_id] = candidate_phase;
        best_hpwl = candidate_hpwl;
        ++flipped_column_count;
        improved = true;
      } else {
        ApplyColumnOrientationPhase(&col_list_[column_id],
                                    first_row_orient_n[column_id]);
      }
    }
    ++completed_sweeps;
    if (!improved) break;
  }

  LOG(info) << "  Orientation phase optimization:\n"
            << "    completed sweeps : " << completed_sweeps << "\n"
            << "    accepted flips   : " << flipped_column_count << "\n"
            << "    optimized HPWL   : " << best_hpwl << "um\n";
  return best_hpwl;
}

void GriddedCellWellLegalizer::ClearCachedData() {
  for (auto& component : ckt_ptr_->Components()) {
    component.SetOrient(N);
  }

  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      stripe.contour_ = stripe.LLY();
      stripe.used_height_ = 0;
      stripe.cluster_count_ = 0;
      stripe.gridded_rows_.clear();
      stripe.front_row_ = nullptr;
    }
  }

  // cluster_list_.clear();
}

bool GriddedCellWellLegalizer::WellLegalize() {
  bool is_success = true;
  InitializeWellLegalizer();
  is_success = ComponentClusteringLoose();
  // ComponentClusteringCompact();
  ReportHPWL();

  if (is_success) {
    LOG(info) << "\033[0;36m"
              << "Standard Cluster Well Legalization complete!\n"
              << "\033[0m";
  } else {
    LOG(info) << "\033[0;36m"
              << "Standard Cluster Well Legalization fail!\n"
              << "\033[0m";
  }

  return is_success;
}

bool GriddedCellWellLegalizer::RunComponentClusteringStage() {
  LOG(info) << "Form component clustering\n";
  bool is_success = ComponentClusteringLoose();
  ReportHPWL();
  RecordPlacementHpwlMetrics("well_legalization.component_clustering",
                             *ckt_ptr_);
  EmitSnapshot("component_clustering", "After Component Clustering",
               "legalization", "component_clustering");
  return is_success;
}

bool GriddedCellWellLegalizer::RunBandedAssignmentPreviewStage() {
  DaliExpects(enable_banded_stripe_assignment_,
              "Banded preview requires banded assignment");
  DaliExpects(enable_detailed_placement_,
              "Banded preview requires gridded detailed placement");

  struct PreviewResult {
    bool uses_banded_assignment = false;
    bool feasible = false;
    double legalized_hpwl = std::numeric_limits<double>::infinity();
    double preview_hpwl = std::numeric_limits<double>::infinity();
  };

  std::vector<PreviewResult> previews = {{false}, {true}};
  {
    ScopedPlacementMetricSuppression suppress_preview_metrics;
    suppress_snapshots_ = true;
    gridded_detailed_placer_.SetMaxRounds(1);
    for (PreviewResult& preview : previews) {
      RestoreInitialComponentLocation();
      InitializeWellLegalizer(-1, preview.uses_banded_assignment);
      preview.feasible = ComponentClusteringLoose();
      if (!preview.feasible) continue;

      preview.legalized_hpwl = WeightedHPWL();
      // Use the same quality-changing stages as the selected final flow. The
      // detailed placer is limited to one round, and read-only solver analysis
      // is omitted because it cannot affect candidate ranking.
      RunPostClusteringStages(true, false);
      preview.preview_hpwl = WeightedHPWL();
    }
    gridded_detailed_placer_.SetMaxRounds(detailed_placement_max_rounds_);
    suppress_snapshots_ = false;
  }

  const PreviewResult* selected = nullptr;
  for (const PreviewResult& preview : previews) {
    if (preview.feasible && (selected == nullptr ||
                             preview.preview_hpwl < selected->preview_hpwl)) {
      selected = &preview;
    }
  }
  DaliExpects(selected != nullptr,
              "Banded assignment preview found no legal placement");

  LOG(info) << "Banded stripe assignment detailed preview:\n";
  for (const PreviewResult& preview : previews) {
    LOG(info) << "  "
              << (preview.uses_banded_assignment ? "banded" : "geometric")
              << ": ";
    if (preview.feasible) {
      LOG(info) << "legalized HPWL=" << preview.legalized_hpwl
                << "um, one-round HPWL=" << preview.preview_hpwl << "um\n";
    } else {
      LOG(info) << "infeasible\n";
    }
  }
  LOG(info) << "  selected ownership: "
            << (selected->uses_banded_assignment ? "banded" : "geometric")
            << "\n";

  RestoreInitialComponentLocation();
  InitializeWellLegalizer(-1, selected->uses_banded_assignment);
  bool is_success = RunComponentClusteringStage();
  DaliExpects(is_success,
              "Selected banded preview placement is not reproducibly legal");
  RecordPlacementMetric("well_legalization.banded_assignment.preview.selected",
                        selected->uses_banded_assignment ? 1.0 : 0.0);
  RecordPlacementMetric(
      "well_legalization.banded_assignment.preview.geometric_hpwl",
      previews.front().feasible ? previews.front().preview_hpwl : -1.0);
  RecordPlacementMetric(
      "well_legalization.banded_assignment.preview.banded_hpwl",
      previews.back().feasible ? previews.back().preview_hpwl : -1.0);
  RunPostClusteringStages(is_success);
  return is_success;
}

bool GriddedCellWellLegalizer::RunVerticalHpwlRowAssignmentPreviewStage() {
  DaliExpects(enable_vertical_hpwl_row_assignment_,
              "Row-assignment preview requires CP-SAT row assignment");
  DaliExpects(enable_detailed_placement_,
              "Row-assignment preview requires gridded detailed placement");

  struct PreviewResult {
    const char* label = "baseline assignment";
    bool uses_row_assignment = false;
    int row_stride = 2;
    int first_row_offset = 0;
    bool feasible = false;
    double legalized_hpwl = std::numeric_limits<double>::infinity();
    double preview_hpwl = std::numeric_limits<double>::infinity();
  };

  // Immediate legal HPWL cannot reliably select a row structure: detailed
  // placement can unlock different relocation, swap, and ordering moves from
  // the same global placement. The complementary pair partitions expose row
  // moves across different boundaries, while the overlapping schedule tests
  // their sequential combination. Rebuild every candidate before previewing.
  std::vector<PreviewResult> previews = {
      {"baseline assignment", false},
      {"CP-SAT even row pairs", true, 2, 0},
      {"CP-SAT odd row pairs", true, 2, 1},
      {"CP-SAT overlapping row pairs", true, 1, 0},
  };
  const bool assignment_enabled = enable_vertical_hpwl_row_assignment_;
  const GriddedVerticalHpwlRowOptimizerConfig assignment_config =
      vertical_hpwl_row_optimizer_config_;
  {
    ScopedPlacementMetricSuppression suppress_preview_metrics;
    suppress_snapshots_ = true;
    gridded_detailed_placer_.SetMaxRounds(1);
    for (PreviewResult& preview : previews) {
      enable_vertical_hpwl_row_assignment_ = preview.uses_row_assignment;
      vertical_hpwl_row_optimizer_config_ = assignment_config;
      vertical_hpwl_row_optimizer_config_.row_stride = preview.row_stride;
      vertical_hpwl_row_optimizer_config_.first_row_offset =
          preview.first_row_offset;
      RestoreInitialComponentLocation();
      InitializeWellLegalizer();
      preview.feasible = ComponentClusteringLoose();
      if (!preview.feasible) continue;

      preview.legalized_hpwl = WeightedHPWL();
      RunPostClusteringStages(true, false);
      preview.preview_hpwl = WeightedHPWL();
    }
    gridded_detailed_placer_.SetMaxRounds(detailed_placement_max_rounds_);
    suppress_snapshots_ = false;
  }
  enable_vertical_hpwl_row_assignment_ = assignment_enabled;
  vertical_hpwl_row_optimizer_config_ = assignment_config;

  const PreviewResult* selected = nullptr;
  int selected_index = -1;
  for (int index = 0; index < static_cast<int>(previews.size()); ++index) {
    const PreviewResult& preview = previews[index];
    if (preview.feasible && (selected == nullptr ||
                             preview.preview_hpwl < selected->preview_hpwl)) {
      selected = &preview;
      selected_index = index;
    }
  }
  DaliExpects(selected != nullptr,
              "Row-assignment preview found no legal placement");

  LOG(info) << "Vertical-HPWL row-assignment detailed preview:\n";
  for (const PreviewResult& preview : previews) {
    LOG(info) << "  " << preview.label << ": ";
    if (preview.feasible) {
      LOG(info) << "legalized HPWL=" << preview.legalized_hpwl
                << "um, one-round HPWL=" << preview.preview_hpwl << "um\n";
    } else {
      LOG(info) << "infeasible\n";
    }
  }
  LOG(info) << "  selected row assignment: " << selected->label << "\n";

  enable_vertical_hpwl_row_assignment_ = selected->uses_row_assignment;
  vertical_hpwl_row_optimizer_config_ = assignment_config;
  vertical_hpwl_row_optimizer_config_.row_stride = selected->row_stride;
  vertical_hpwl_row_optimizer_config_.first_row_offset =
      selected->first_row_offset;
  RestoreInitialComponentLocation();
  InitializeWellLegalizer();
  bool is_success = RunComponentClusteringStage();
  DaliExpects(is_success,
              "Selected row-assignment preview is not reproducibly legal");
  RecordPlacementMetric(
      "well_legalization.vertical_hpwl_row_assignment.preview.selected",
      selected_index);
  for (int index = 0; index < static_cast<int>(previews.size()); ++index) {
    RecordPlacementMetric(
        "well_legalization.vertical_hpwl_row_assignment.preview.candidate_" +
            std::to_string(index) + ".hpwl",
        previews[index].feasible ? previews[index].preview_hpwl : -1.0);
  }
  RunPostClusteringStages(is_success);
  return is_success;
}

bool GriddedCellWellLegalizer::RunBestBoundaryClusteringStage() {
  DaliExpects(enable_adaptive_stripe_boundaries_,
              "Boundary selection requires adaptive stripes to be enabled");

  enable_adaptive_stripe_boundaries_ = false;
  stripe_boundaries_override_.clear();
  RestoreInitialComponentLocation();
  InitializeWellLegalizer();
  const std::vector<int> uniform_boundaries = CollectColumnBoundaries();

  enable_adaptive_stripe_boundaries_ = true;
  RestoreInitialComponentLocation();
  InitializeWellLegalizer();
  const std::vector<int> adaptive_boundaries = CollectColumnBoundaries();
  enable_adaptive_stripe_boundaries_ = false;

  int max_component_width = 0;
  for (const Component& component : ckt_ptr_->Components()) {
    if (component.IsMovable()) {
      max_component_width = std::max(max_component_width, component.Width());
    }
  }
  int average_pitch = (uniform_boundaries.back() - uniform_boundaries.front()) /
                      static_cast<int>(uniform_boundaries.size() - 1);
  StripeBoundaryCoordinateConfig search_config;
  // A one-grid move changes ownership only for components immediately beside
  // the cutline. Cell-width moves were too disruptive on test_case_3 and had
  // no improving candidate in either direction.
  search_config.step = 1;
  search_config.minimum_pitch =
      max_component_width + well_spacing_ + PhysicalCompletionReservedWidth();
  search_config.maximum_pitch = average_pitch * 3 / 2;
  search_config.minimum_improvement = 1e-6;

  auto evaluator = [this](const std::vector<int>& boundaries) {
    RestoreInitialComponentLocation();
    stripe_boundaries_override_ = boundaries;
    InitializeWellLegalizer();
    bool feasible = ComponentClusteringLoose();
    return StripeBoundaryEvaluation{feasible, feasible ? WeightedHPWL() : 0.0};
  };

  const StripeBoundaryEvaluation uniform_evaluation =
      evaluator(uniform_boundaries);
  StripeBoundaryEvaluation adaptive_evaluation = uniform_evaluation;
  if (adaptive_boundaries != uniform_boundaries) {
    adaptive_evaluation = evaluator(adaptive_boundaries);
  }
  const bool use_adaptive_seed =
      adaptive_evaluation.feasible &&
      (!uniform_evaluation.feasible ||
       adaptive_evaluation.cost < uniform_evaluation.cost);
  const std::vector<int>& initial_boundaries =
      use_adaptive_seed ? adaptive_boundaries : uniform_boundaries;

  StripeBoundaryCoordinateResult search_result =
      StripeBoundaryCoordinateOptimizer(search_config)
          .Optimize(initial_boundaries, evaluator);

  RestoreInitialComponentLocation();
  stripe_boundaries_override_ =
      search_result.feasible ? search_result.boundaries : initial_boundaries;
  InitializeWellLegalizer();
  bool is_success = ComponentClusteringLoose();
  DaliExpects(
      !search_result.feasible || is_success,
      "Selected stripe boundary search result is not reproducibly legal");

  LOG(info) << "Form component clustering\n"
            << "  local stripe-boundary search:\n"
            << "    candidates evaluated : "
            << search_result.evaluated_candidates << "\n"
            << "    accepted moves       : " << search_result.accepted_moves
            << "\n"
            << "    uniform seed         : "
            << (uniform_evaluation.feasible
                    ? std::to_string(uniform_evaluation.cost) + "um"
                    : "infeasible")
            << "\n"
            << "    adaptive seed        : "
            << (adaptive_evaluation.feasible
                    ? std::to_string(adaptive_evaluation.cost) + "um"
                    : "infeasible")
            << "\n"
            << "    selected seed        : "
            << (use_adaptive_seed ? "adaptive" : "uniform") << "\n"
            << "    initial feasible     : " << search_result.feasible << "\n";
  if (search_result.feasible) {
    LOG(info) << "    initial HPWL         : " << search_result.initial_cost
              << "um\n"
              << "    selected HPWL        : " << search_result.final_cost
              << "um\n";
  }
  ReportHPWL();
  RecordPlacementHpwlMetrics("well_legalization.component_clustering",
                             *ckt_ptr_);
  EmitSnapshot("component_clustering", "After Component Clustering",
               "legalization", "component_clustering");
  return is_success;
}

std::vector<int> GriddedCellWellLegalizer::CollectColumnBoundaries() const {
  DaliExpects(!col_list_.empty(),
              "Cannot collect boundaries from an empty stripe partition");
  std::vector<int> boundaries(col_list_.size() + 1);
  for (size_t column = 0; column < col_list_.size(); ++column) {
    boundaries[column] = col_list_[column].LLX();
  }
  boundaries.back() = col_list_.back().URX() + well_spacing_;
  return boundaries;
}

void GriddedCellWellLegalizer::RunClusterOrientationStage() {
  if (disable_cell_flip_) {
    LOG(info) << "Skip flipping cluster orientation\n";
    return;
  }
  LOG(info) << "Flip cluster orientation\n";
  UpdateClusterOrient();
  double fixed_phase_hpwl = WeightedHPWL();
  RecordPlacementHpwlMetrics("well_legalization.orientation.fixed_phase",
                             *ckt_ptr_);
  double optimized_hpwl = OptimizeColumnOrientationPhases();
  LOG(info) << "  orientation phase improvement: "
            << fixed_phase_hpwl - optimized_hpwl << "um\n";
  ReportHPWL();
  RecordPlacementHpwlMetrics("well_legalization.orientation", *ckt_ptr_);
  EmitSnapshot("orientation", "After Cluster Orientation", "legalization",
               "orientation");
}

std::vector<GriddedRow*> GriddedCellWellLegalizer::CollectGriddedRows() {
  std::vector<GriddedRow*> rows;
  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      for (auto& row : stripe.gridded_rows_) {
        rows.push_back(&row);
      }
    }
  }
  return rows;
}

void GriddedCellWellLegalizer::SynchronizeComponentLocationsWithRows() {
  // Detailed placement changes row membership transactionally. Recompute Y
  // coordinates from the final row geometry so every component lies on its
  // assigned row's P/N well edge before physical cells are materialized.
  for (GriddedRow* row : CollectGriddedRows()) {
    row->UpdateComponentLocY();
  }
}

void GriddedCellWellLegalizer::RunGriddedDetailedPlacementStage() {
  LOG(info) << (enable_detailed_placement_ ? "Run gridded detailed placement\n"
                                           : "Run gridded local reorder\n");
  gridded_detailed_placer_.CopyPlacementContextFrom(this);
  gridded_detailed_placer_.SetRows(CollectGriddedRows());
  gridded_detailed_placer_.SetSnapshotCallback(
      [this](const std::string& id, const std::string& label,
             const std::string& subgroup, int iteration) {
        EmitSnapshot(id, label, "detailed_placement", subgroup, iteration);
      });
  EmitSnapshot("gridded.start", "Before Gridded Detailed Placement",
               "detailed_placement", "start");
  if (enable_detailed_placement_) {
    gridded_detailed_placer_.StartPlacement();
    RecordPlacementHpwlMetrics("well_legalization.gridded_detailed", *ckt_ptr_);
  } else {
    gridded_detailed_placer_.StartLocalReorder();
    RecordPlacementHpwlMetrics("well_legalization.local_reorder", *ckt_ptr_);
  }
  EmitSnapshot("gridded.final", "After Gridded Detailed Placement",
               "detailed_placement", "final");
}

void GriddedCellWellLegalizer::RunRowLocationOptimizationStage() {
  LOG(info) << "Optimize gridded row Y locations\n";
  ElapsedTime timer;
  timer.RecordStartTime();
  GriddedRowLocationResult result =
      GriddedRowLocationOptimizer(ckt_ptr_).Optimize(&col_list_);
  timer.RecordEndTime();
  LOG(info) << "  sweep results:\n";
  for (size_t sweep = 0; sweep < result.sweep_results.size(); ++sweep) {
    LOG(info) << "    " << sweep + 1
              << ": groups moved=" << result.sweep_results[sweep].groups_moved
              << ", HPWL=" << result.sweep_results[sweep].hpwl << "um\n";
  }
  LOG(info) << "  completed sweeps  : " << result.sweeps << "\n"
            << "  groups considered : " << result.groups_considered << "\n"
            << "  groups moved      : " << result.groups_moved << "\n"
            << "  HPWL improvement  : "
            << result.hpwl_before - result.hpwl_after << "um\n"
            << "  wall time         : " << timer.GetWallTime() << "s\n";
  RecordPlacementHpwlMetrics("well_legalization.row_location", *ckt_ptr_);
  RecordPlacementMetric("time.well_legalization.row_location.wall_s",
                        timer.GetWallTime());
  RecordPlacementMetric("time.well_legalization.row_location.cpu_s",
                        timer.GetCpuTime());
  EmitSnapshot("row_location", "After Gridded Row Location Optimization",
               "legalization", "row_location");
}

void GriddedCellWellLegalizer::RunOrToolsRowOptimizationStage() {
  LOG(info) << "Optimize gridded row X locations with OR-Tools CP-SAT\n";
  ElapsedTime timer;
  timer.RecordStartTime();

  OrToolsGriddedRowOptimizerConfig config;
  config.net_ignore_threshold = ortools_net_ignore_threshold_;
  // Keep this first integration deterministic. Per-model and total-stage time
  // limits bound the additional runtime when the pass is used on large designs.
  config.number_of_workers = 1;
  config.maximum_time_seconds_per_model = 0.05;
  config.maximum_total_time_seconds = 10.0;
  config.target_components_per_model = 128;
  OrToolsGriddedRowOptimizerResult result =
      OrToolsGriddedRowOptimizer(ckt_ptr_, config).Optimize(&col_list_);
  timer.RecordEndTime();

  if (!result.available) {
    LOG(warning) << "  OR-Tools support is unavailable; keep existing row "
                    "locations\n";
  }
  LOG(info) << "  CP-SAT row optimization:\n"
            << "    attempted models : " << result.attempted_models << "\n"
            << "    solved models    : " << result.solved_models << "\n"
            << "    accepted models  : " << result.accepted_models << "\n"
            << "    improved models  : " << result.improved_models << "\n"
            << "    time budget hit  : " << result.time_budget_exhausted << "\n"
            << "    HPWL before       : " << result.hpwl_before << "um\n"
            << "    HPWL after        : " << result.hpwl_after << "um\n"
            << "    HPWL improvement  : "
            << result.hpwl_before - result.hpwl_after << "um\n"
            << "    solver wall time  : " << result.solver_wall_time_seconds
            << "s\n"
            << "    stage wall time   : " << timer.GetWallTime() << "s\n";
  RecordPlacementHpwlMetrics("well_legalization.ortools_row", *ckt_ptr_);
  RecordPlacementMetric("time.well_legalization.ortools_row.wall_s",
                        timer.GetWallTime());
  RecordPlacementMetric("time.well_legalization.ortools_row.cpu_s",
                        timer.GetCpuTime());
  RecordPlacementMetric("well_legalization.ortools_row.attempted",
                        result.attempted_models);
  RecordPlacementMetric("well_legalization.ortools_row.solved",
                        result.solved_models);
  RecordPlacementMetric("well_legalization.ortools_row.accepted",
                        result.accepted_models);
  RecordPlacementMetric("well_legalization.ortools_row.improved",
                        result.improved_models);
  EmitSnapshot("ortools_row", "After OR-Tools Row Optimization", "legalization",
               "ortools_row");
}

void GriddedCellWellLegalizer::RunVerticalHpwlRowAssignmentStage() {
  LOG(info) << "Run experimental vertical-HPWL row assignment\n";
  ElapsedTime timer;
  timer.RecordStartTime();
  const GriddedVerticalHpwlRowOptimizerResult result =
      GriddedVerticalHpwlRowOptimizer(ckt_ptr_,
                                      vertical_hpwl_row_optimizer_config_)
          .Optimize(&col_list_);
  timer.RecordEndTime();

  if (!result.available) {
    LOG(warning) << "  OR-Tools support is unavailable; keep existing row "
                    "assignments\n";
  }
  LOG(info) << "  vertical-HPWL row assignment:\n"
            << "    completed sweeps       : " << result.completed_sweeps
            << "\n"
            << "    attempted windows      : " << result.attempted_windows
            << "\n"
            << "    skipped closure windows: " << result.skipped_closure_windows
            << "\n"
            << "    solved windows         : " << result.solved_windows << "\n"
            << "    accepted windows       : " << result.accepted_windows
            << "\n"
            << "    rejected after closure : "
            << result.closure_rejected_windows << "\n"
            << "    reassigned components  : " << result.reassigned_components
            << "\n"
            << "    HPWL before            : " << result.hpwl_before << "um\n"
            << "    HPWL after             : " << result.hpwl_after << "um\n"
            << "    HPWL improvement       : "
            << result.hpwl_before - result.hpwl_after << "um\n"
            << "    local closure gain     : baseline="
            << result.local_closure_baseline_gain
            << "um, candidate=" << result.local_closure_candidate_gain << "um\n"
            << "    solver wall time       : "
            << result.solver_wall_time_seconds << "s\n"
            << "    stage wall time        : " << timer.GetWallTime() << "s\n"
            << "    time budget hit        : " << result.time_budget_exhausted
            << "\n";
  RecordPlacementHpwlMetrics("well_legalization.vertical_hpwl_row_assignment",
                             *ckt_ptr_);
  RecordPlacementMetric(
      "time.well_legalization.vertical_hpwl_row_assignment.optimizer_wall_s",
      result.solver_wall_time_seconds);
  RecordPlacementMetric(
      "time.well_legalization.vertical_hpwl_row_assignment.wall_s",
      timer.GetWallTime());
  RecordPlacementMetric(
      "well_legalization.vertical_hpwl_row_assignment.attempted",
      result.attempted_windows);
  RecordPlacementMetric("well_legalization.vertical_hpwl_row_assignment.solved",
                        result.solved_windows);
  RecordPlacementMetric(
      "well_legalization.vertical_hpwl_row_assignment.accepted",
      result.accepted_windows);
  RecordPlacementMetric(
      "well_legalization.vertical_hpwl_row_assignment.reassigned_components",
      result.reassigned_components);
  RecordPlacementMetric(
      "well_legalization.vertical_hpwl_row_assignment.closure_rejected",
      result.closure_rejected_windows);
  EmitSnapshot("vertical_hpwl_row_assignment",
               "After Vertical-HPWL Row Assignment", "legalization",
               "vertical_hpwl_row_assignment");
}

void GriddedCellWellLegalizer::RunExactLegalizationAnalysisStage() {
  LOG(info) << "Analyze bounded exact gridded legalization windows\n";
  ElapsedTime timer;
  timer.RecordStartTime();

  ExactGriddedWindowAnalyzerConfig config = exact_legalization_analysis_config_;
  const GriddedCapacityConfig capacity = BuildGriddedCapacityConfig(1.0);
  config.minimum_p_well_height = capacity.minimum_p_well_height;
  config.minimum_n_well_height = capacity.minimum_n_well_height;
  ExactGriddedWindowAnalysis analysis =
      ExactGriddedLegalizationWindowAnalyzer(ckt_ptr_, config)
          .Analyze(&col_list_);
  timer.RecordEndTime();

  if (!analysis.available) {
    LOG(warning) << "  OR-Tools support is unavailable; skip exact "
                    "legalization analysis\n";
    return;
  }

  LOG(info)
      << "  exact legalization analysis:\n"
      << "    solver backend          : "
      << (config.use_compact_solver ? "compact" : "exact") << "\n"
      << "    minimum rows/window     : " << config.minimum_rows_per_window
      << "\n"
      << "    maximum row displacement: " << config.maximum_row_displacement
      << "\n"
      << "    maximum row changes     : "
      << config.maximum_row_assignment_changes << "\n"
      << "    fixed row geometry      : " << config.fix_row_geometry << "\n"
      << "    fixed component X       : " << config.fix_cell_x << "\n"
      << "    fixed component orient. : " << config.fix_cell_orientation << "\n"
      << "    overlapping row windows : " << config.overlap_row_windows << "\n"
      << "    candidate windows       : " << analysis.candidate_windows << "\n"
      << "    oversized windows       : " << analysis.oversized_windows << "\n"
      << "    attempted windows       : " << analysis.attempted_windows << "\n"
      << "    solved windows          : " << analysis.solved_windows << "\n"
      << "    optimal windows         : " << analysis.optimal_windows << "\n"
      << "    improved windows        : " << analysis.improved_windows << "\n"
      << "    reassigned components   : " << analysis.reassigned_components
      << "\n"
      << "    orientation changes     : " << analysis.orientation_changes
      << "\n"
      << "    component X changes     : " << analysis.x_location_changes << "\n"
      << "    row activation changes  : " << analysis.row_activation_changes
      << "\n"
      << "    row location changes    : " << analysis.row_location_changes
      << "\n"
      << "    well height changes     : " << analysis.well_height_changes
      << "\n"
      << "    positive-bound windows  : " << analysis.positive_bound_windows
      << "\n"
      << "    feasible solution hints : " << analysis.feasible_hint_windows
      << "\n"
      << "    best-known windows      : " << analysis.best_known_windows << "\n"
      << "    hinted model HPWL       : " << analysis.hinted_hpwl_sum << "um\n"
      << "    solved current HPWL     : " << analysis.solved_current_hpwl_sum
      << "um\n"
      << "    solver solution HPWL    : " << analysis.solver_solution_hpwl_sum
      << "um\n"
      << "    best-known HPWL         : " << analysis.best_known_hpwl_sum
      << "um\n"
      << "    bounded current HPWL    : " << analysis.bounded_current_hpwl_sum
      << "um\n"
      << "    positive lower bound    : " << analysis.positive_lower_bound_sum
      << "um\n"
      << "    solver wall time        : " << analysis.solver_wall_time_seconds
      << "s\n"
      << "    hint validation time    : "
      << analysis.hint_validation_wall_time_seconds << "s\n"
      << "    stage wall time         : " << timer.GetWallTime() << "s\n";
  for (size_t index = 0; index < analysis.windows.size(); ++index) {
    const ExactGriddedWindowResult& window = analysis.windows[index];
    std::ostringstream message;
    message << "    window " << index + 1 << ": column " << window.column_index
            << ", stripe " << window.stripe_index << ", rows "
            << window.first_row_index << "-" << window.last_row_index << ", "
            << window.component_count << " components, " << window.net_count
            << " nets, " << window.row_assignment_choice_count
            << " row choices, status "
            << ExactGriddedLegalizationStatusName(window.status)
            << ", current HPWL " << window.current_weighted_hpwl << "um"
            << ", hint "
            << ExactGriddedLegalizationStatusName(
                   window.hint_validation_status);
    if (window.hint_validation_status ==
            ExactGriddedLegalizationStatus::kFeasible ||
        window.hint_validation_status ==
            ExactGriddedLegalizationStatus::kOptimal) {
      message << " (" << window.hinted_weighted_hpwl << "um)";
    } else if (!window.hint_validation_message.empty()) {
      message << " (" << window.hint_validation_message << ")";
    }
    if (window.status == ExactGriddedLegalizationStatus::kFeasible ||
        window.status == ExactGriddedLegalizationStatus::kOptimal) {
      message << ", solver solution " << window.solved_weighted_hpwl
              << "um, best known " << window.best_known_weighted_hpwl
              << "um, bound " << window.best_objective_bound << "um, gap "
              << window.relative_gap << ", reassigned "
              << window.reassigned_component_count << ", reoriented "
              << window.orientation_change_count << ", X moves "
              << window.x_location_change_count << ", row activation changes "
              << window.row_activation_change_count << ", row moves "
              << window.row_location_change_count << ", well height changes "
              << window.well_height_change_count;
    } else if (window.best_objective_bound > 0.0) {
      message << ", solver solution unavailable, bound "
              << window.best_objective_bound << "um";
    } else {
      message << ", solver solution unavailable, bound unavailable";
    }
    message << ", time " << window.wall_time_seconds << "s\n";
    LOG(info) << message.str();
  }

  RecordPlacementMetric("exact_legalization.candidate_windows",
                        analysis.candidate_windows);
  RecordPlacementMetric("exact_legalization.oversized_windows",
                        analysis.oversized_windows);
  RecordPlacementMetric("exact_legalization.attempted_windows",
                        analysis.attempted_windows);
  RecordPlacementMetric("exact_legalization.solved_windows",
                        analysis.solved_windows);
  RecordPlacementMetric("exact_legalization.optimal_windows",
                        analysis.optimal_windows);
  RecordPlacementMetric("exact_legalization.improved_windows",
                        analysis.improved_windows);
  RecordPlacementMetric("exact_legalization.reassigned_components",
                        analysis.reassigned_components);
  RecordPlacementMetric("exact_legalization.orientation_changes",
                        analysis.orientation_changes);
  RecordPlacementMetric("exact_legalization.x_location_changes",
                        analysis.x_location_changes);
  RecordPlacementMetric("exact_legalization.row_activation_changes",
                        analysis.row_activation_changes);
  RecordPlacementMetric("exact_legalization.row_location_changes",
                        analysis.row_location_changes);
  RecordPlacementMetric("exact_legalization.well_height_changes",
                        analysis.well_height_changes);
  RecordPlacementMetric("exact_legalization.positive_bound_windows",
                        analysis.positive_bound_windows);
  RecordPlacementMetric("exact_legalization.feasible_hint_windows",
                        analysis.feasible_hint_windows);
  RecordPlacementMetric("exact_legalization.best_known_windows",
                        analysis.best_known_windows);
  RecordPlacementMetric("exact_legalization.hinted_hpwl",
                        analysis.hinted_hpwl_sum);
  RecordPlacementMetric("exact_legalization.solved_current_hpwl",
                        analysis.solved_current_hpwl_sum);
  RecordPlacementMetric("exact_legalization.solver_solution_hpwl",
                        analysis.solver_solution_hpwl_sum);
  RecordPlacementMetric("exact_legalization.best_known_hpwl",
                        analysis.best_known_hpwl_sum);
  RecordPlacementMetric("exact_legalization.bounded_current_hpwl",
                        analysis.bounded_current_hpwl_sum);
  RecordPlacementMetric("exact_legalization.positive_lower_bound",
                        analysis.positive_lower_bound_sum);
  RecordPlacementMetric("time.exact_legalization.solver_wall_s",
                        analysis.solver_wall_time_seconds);
  RecordPlacementMetric("time.exact_legalization.hint_validation.wall_s",
                        analysis.hint_validation_wall_time_seconds);
  RecordPlacementMetric("time.exact_legalization.wall_s", timer.GetWallTime());
  RecordPlacementMetric("time.exact_legalization.cpu_s", timer.GetCpuTime());
}

void GriddedCellWellLegalizer::RunWholeDesignExactLegalizationStage() {
  ElapsedTime timer;
  timer.RecordStartTime();
  const GriddedCapacityConfig capacity = BuildGriddedCapacityConfig(1.0);
  ExactGriddedWholeDesignBuilderConfig builder_config;
  builder_config.net_ignore_threshold =
      whole_design_exact_net_ignore_threshold_;
  builder_config.minimum_p_well_height = capacity.minimum_p_well_height;
  builder_config.minimum_n_well_height = capacity.minimum_n_well_height;
  // Start the 16k-cell experiment with fixed stripe membership and the
  // current row count. Both restrictions are explicit in the report below;
  // broader domains can be enabled after measuring this model's scaling.
  builder_config.allow_cross_stripe_moves = false;
  builder_config.use_full_row_slot_capacity = false;
  ExactGriddedWholeDesignBuildResult build =
      ExactGriddedWholeDesignModelBuilder(ckt_ptr_, builder_config)
          .Build(&col_list_);

  LOG(info)
      << "Solve whole-design exact gridded legalization\n"
      << "  domain                             : current stripe and row count\n"
      << "  maximum row displacement           : "
      << whole_design_exact_legalization_config_.maximum_row_displacement
      << "\n"
      << "  use production solution hint       : "
      << whole_design_exact_legalization_config_.use_solution_hint << "\n"
      << "  components                         : "
      << build.stats.component_count << "\n"
      << "  nets                               : " << build.stats.net_count
      << "\n"
      << "  ignored net fanout threshold       : "
      << whole_design_exact_net_ignore_threshold_ << "\n"
      << "  stripes                            : " << build.stats.stripe_count
      << "\n"
      << "  current rows                       : "
      << build.stats.active_row_count << "\n"
      << "  physical row slots                 : " << build.stats.row_slot_count
      << "\n"
      << "  row/orientation choice upper bound : "
      << build.stats.enumerated_placement_choice_upper_bound << "\n";

  ExactGriddedLegalizationResult solution =
      OrToolsCompactGriddedLegalizer().Solve(
          build.model, whole_design_exact_legalization_config_);
  timer.RecordEndTime();
  LOG(info)
      << "  compact variables                  : "
      << solution.model_variable_count << "\n"
      << "  compact constraints                : "
      << solution.model_constraint_count << "\n"
      << "  compact row choices                : "
      << solution.row_assignment_choice_count << "\n"
      << "  current hint status                : "
      << ExactGriddedLegalizationStatusName(solution.hint_validation_status)
      << "\n"
      << "  current hinted HPWL                : "
      << solution.hinted_weighted_hpwl << "um\n"
      << "  hint validation wall time          : "
      << solution.hint_validation_wall_time_seconds << "s\n"
      << "  solve status                       : "
      << ExactGriddedLegalizationStatusName(solution.status) << "\n"
      << "  incumbent HPWL                     : " << solution.weighted_hpwl
      << "um\n"
      << "  best objective bound               : "
      << solution.best_objective_bound << "\n"
      << "  relative gap                       : " << solution.relative_gap
      << "\n"
      << "  solver wall time                   : " << solution.wall_time_seconds
      << "s\n"
      << "  total stage wall time              : " << timer.GetWallTime()
      << "s\n";
  if (!solution.hint_validation_message.empty()) {
    LOG(warning) << "  hint validation detail: "
                 << solution.hint_validation_message << "\n";
  }
  if (!solution.message.empty()) {
    LOG(info) << "  solver detail: " << solution.message << "\n";
  }

  RecordPlacementMetric("exact_legalization.whole_design.components",
                        build.stats.component_count);
  RecordPlacementMetric("exact_legalization.whole_design.row_slots",
                        build.stats.row_slot_count);
  RecordPlacementMetric("exact_legalization.whole_design.variables",
                        solution.model_variable_count);
  RecordPlacementMetric("exact_legalization.whole_design.constraints",
                        solution.model_constraint_count);
  RecordPlacementMetric("exact_legalization.whole_design.row_choices",
                        solution.row_assignment_choice_count);
  RecordPlacementMetric("exact_legalization.whole_design.hinted_hpwl",
                        solution.hinted_weighted_hpwl);
  RecordPlacementMetric("exact_legalization.whole_design.incumbent_hpwl",
                        solution.weighted_hpwl);
  RecordPlacementMetric("exact_legalization.whole_design.best_bound",
                        solution.best_objective_bound);
  RecordPlacementMetric("exact_legalization.whole_design.relative_gap",
                        solution.relative_gap);
  RecordPlacementMetric("time.exact_legalization.whole_design.solver_wall_s",
                        solution.wall_time_seconds);
  RecordPlacementMetric(
      "time.exact_legalization.whole_design.hint_validation_wall_s",
      solution.hint_validation_wall_time_seconds);
  RecordPlacementMetric("time.exact_legalization.whole_design.wall_s",
                        timer.GetWallTime());
}

OrToolsGriddedStripeOptimizerResult
GriddedCellWellLegalizer::RunExactStripeOptimizationPhase(
    const std::string& label, const std::string& metric_prefix,
    const OrToolsGriddedStripeOptimizerConfig& config) {
  ElapsedTime timer;
  timer.RecordStartTime();
  OrToolsGriddedStripeOptimizerResult result =
      OrToolsGriddedStripeOptimizer(ckt_ptr_, config).Optimize(&col_list_);
  timer.RecordEndTime();

  LOG(info)
      << label << ":\n"
      << "  available              : " << result.available << "\n"
      << "  maximum row displacement: " << config.maximum_row_displacement
      << "\n"
      << "  maximum row changes    : " << config.maximum_row_assignment_changes
      << "\n"
      << "  displacement weight    : " << config.displacement_weight << "\n"
      << "  completed sweeps       : " << result.completed_sweeps << "\n"
      << "  attempted models       : " << result.attempted_models << "\n"
      << "  solved models          : " << result.solved_models << "\n"
      << "  accepted models        : " << result.accepted_models << "\n"
      << "  accepted row moves     : " << result.accepted_reassignment_models
      << " models, " << result.accepted_reassigned_components << " components\n"
      << "  HPWL before            : " << result.hpwl_before << "um\n"
      << "  HPWL after             : " << result.hpwl_after << "um\n"
      << "  improvement            : " << result.hpwl_before - result.hpwl_after
      << "um\n"
      << "    fixed-row X gain     : " << result.fixed_row_hpwl_improvement
      << "um\n"
      << "    row-reassignment gain: " << result.reassignment_hpwl_improvement
      << "um\n"
      << "  accepted displacement  : " << result.accepted_physical_displacement
      << "um\n"
      << "  solver wall time       : " << result.solver_wall_time_seconds
      << "s\n"
      << "  total stage wall time  : " << timer.GetWallTime() << "s\n"
      << "  time budget exhausted  : " << result.time_budget_exhausted << "\n";
  for (const OrToolsGriddedStripeSolveResult& stripe : result.stripes) {
    LOG(info) << "  sweep " << stripe.sweep << ", column "
              << stripe.column_index << ", stripe " << stripe.stripe_index
              << ", rows " << stripe.first_row_index << "-"
              << stripe.last_row_index << ": status="
              << ExactGriddedLegalizationStatusName(stripe.status)
              << ", cells=" << stripe.component_count
              << ", nets=" << stripe.net_count
              << ", modeled HPWL=" << stripe.modeled_hpwl_before << " -> "
              << stripe.modeled_hpwl_after << "um"
              << ", affected HPWL=" << stripe.affected_hpwl_before << " -> "
              << stripe.affected_hpwl_after << "um"
              << ", reassigned=" << stripe.reassigned_component_count
              << ", displacement=" << stripe.physical_displacement << "um"
              << ", accepted=" << stripe.accepted
              << ", gap=" << stripe.relative_gap
              << ", wall=" << stripe.solver_wall_time_seconds << "s\n";
  }

  RecordPlacementMetric(metric_prefix + ".attempted", result.attempted_models);
  RecordPlacementMetric(metric_prefix + ".solved", result.solved_models);
  RecordPlacementMetric(metric_prefix + ".accepted", result.accepted_models);
  RecordPlacementMetric(metric_prefix + ".accepted_reassignment_models",
                        result.accepted_reassignment_models);
  RecordPlacementMetric(metric_prefix + ".accepted_reassigned_components",
                        result.accepted_reassigned_components);
  RecordPlacementMetric(metric_prefix + ".hpwl.before", result.hpwl_before);
  RecordPlacementMetric(metric_prefix + ".hpwl.after", result.hpwl_after);
  RecordPlacementMetric(metric_prefix + ".hpwl.fixed_row_improvement",
                        result.fixed_row_hpwl_improvement);
  RecordPlacementMetric(metric_prefix + ".hpwl.reassignment_improvement",
                        result.reassignment_hpwl_improvement);
  RecordPlacementMetric(metric_prefix + ".accepted_displacement_um",
                        result.accepted_physical_displacement);
  RecordPlacementMetric("time." + metric_prefix + ".solver_wall_s",
                        result.solver_wall_time_seconds);
  RecordPlacementMetric("time." + metric_prefix + ".wall_s",
                        timer.GetWallTime());
  return result;
}

void GriddedCellWellLegalizer::RunExactStripeOptimizationStage() {
  const GriddedCapacityConfig capacity = BuildGriddedCapacityConfig(1.0);
  exact_stripe_optimizer_config_.minimum_p_well_height =
      capacity.minimum_p_well_height;
  exact_stripe_optimizer_config_.minimum_n_well_height =
      capacity.minimum_n_well_height;

  int accepted_models = 0;
  if (exact_stripe_fixed_row_prepass_ &&
      exact_stripe_optimizer_config_.maximum_row_displacement > 0) {
    OrToolsGriddedStripeOptimizerConfig prepass_config =
        exact_stripe_optimizer_config_;
    prepass_config.maximum_row_displacement = 0;
    prepass_config.maximum_row_assignment_changes = -1;
    const OrToolsGriddedStripeOptimizerResult prepass =
        RunExactStripeOptimizationPhase(
            "Exact gridded stripe fixed-row prepass", "exact_stripe.prepass",
            prepass_config);
    accepted_models += prepass.accepted_models;
  }

  const OrToolsGriddedStripeOptimizerResult refinement =
      RunExactStripeOptimizationPhase("Exact gridded stripe optimization",
                                      "exact_stripe",
                                      exact_stripe_optimizer_config_);
  accepted_models += refinement.accepted_models;
  if (accepted_models > 0) {
    EmitSnapshot("exact_stripe", "After Exact Stripe Optimization",
                 "legalization", "exact_stripe", 0);
  }
}

void GriddedCellWellLegalizer::RunExactBoundaryOptimizationStage() {
  const GriddedCapacityConfig capacity = BuildGriddedCapacityConfig(1.0);
  exact_boundary_refiner_config_.minimum_p_well_height =
      capacity.minimum_p_well_height;
  exact_boundary_refiner_config_.minimum_n_well_height =
      capacity.minimum_n_well_height;

  ElapsedTime timer;
  timer.RecordStartTime();
  const OrToolsGriddedBoundaryRefinerResult result =
      OrToolsGriddedBoundaryRefiner(ckt_ptr_, exact_boundary_refiner_config_)
          .Optimize(&col_list_);
  timer.RecordEndTime();

  LOG(info) << "Exact gridded boundary optimization:\n"
            << "  available               : " << result.available << "\n"
            << "  candidate windows       : " << result.candidate_windows
            << "\n"
            << "  oversized windows       : " << result.oversized_windows
            << "\n"
            << "  attempted models        : " << result.attempted_models << "\n"
            << "  solved models           : " << result.solved_models << "\n"
            << "  accepted models         : " << result.accepted_models << "\n"
            << "  accepted crossing models: "
            << result.accepted_cross_stripe_models << "\n"
            << "  cross-stripe components : "
            << result.accepted_cross_stripe_components << "\n"
            << "  HPWL before             : " << result.hpwl_before << "um\n"
            << "  HPWL after              : " << result.hpwl_after << "um\n"
            << "  improvement             : "
            << result.hpwl_before - result.hpwl_after << "um\n"
            << "    local-window gain     : " << result.local_hpwl_improvement
            << "um\n"
            << "    cross-stripe gain     : "
            << result.cross_stripe_hpwl_improvement << "um\n"
            << "  solver wall time        : " << result.solver_wall_time_seconds
            << "s\n"
            << "  total stage wall time   : " << timer.GetWallTime() << "s\n"
            << "  time budget exhausted   : " << result.time_budget_exhausted
            << "\n";
  for (const OrToolsGriddedBoundaryWindowResult& window : result.windows) {
    if (!window.optimization.accepted) continue;
    LOG(info) << "  accepted boundary " << window.first_column << "-"
              << window.second_column << ", rows " << window.first_row << "-"
              << window.first_last_row << " / " << window.second_row << "-"
              << window.second_last_row
              << ": components=" << window.optimization.component_count
              << ", cross-stripe="
              << window.optimization.cross_stripe_component_count
              << ", affected HPWL=" << window.optimization.affected_hpwl_before
              << " -> " << window.optimization.affected_hpwl_after << "um\n";
  }

  RecordPlacementMetric("exact_boundary.candidates", result.candidate_windows);
  RecordPlacementMetric("exact_boundary.oversized", result.oversized_windows);
  RecordPlacementMetric("exact_boundary.attempted", result.attempted_models);
  RecordPlacementMetric("exact_boundary.solved", result.solved_models);
  RecordPlacementMetric("exact_boundary.accepted", result.accepted_models);
  RecordPlacementMetric("exact_boundary.accepted_cross_stripe_models",
                        result.accepted_cross_stripe_models);
  RecordPlacementMetric("exact_boundary.cross_stripe_components",
                        result.accepted_cross_stripe_components);
  RecordPlacementMetric("exact_boundary.hpwl.before", result.hpwl_before);
  RecordPlacementMetric("exact_boundary.hpwl.after", result.hpwl_after);
  RecordPlacementMetric("exact_boundary.hpwl.local_improvement",
                        result.local_hpwl_improvement);
  RecordPlacementMetric("exact_boundary.hpwl.cross_stripe_improvement",
                        result.cross_stripe_hpwl_improvement);
  RecordPlacementMetric("time.exact_boundary.solver_wall_s",
                        result.solver_wall_time_seconds);
  RecordPlacementMetric("time.exact_boundary.wall_s", timer.GetWallTime());
  if (result.accepted_models > 0) {
    EmitSnapshot("exact_boundary", "After Exact Boundary Optimization",
                 "legalization", "exact_boundary", 0);
  }
}

void GriddedCellWellLegalizer::RunJointOrientationAndRowLocationOptimization() {
  constexpr int kMaximumRounds = 4;
  constexpr double kMinimumRelativeImprovement = 1e-5;

  ElapsedTime timer;
  timer.RecordStartTime();
  const double initial_hpwl = WeightedHPWL();
  double current_hpwl = initial_hpwl;
  int completed_rounds = 0;
  for (int round = 0; round < kMaximumRounds; ++round) {
    const double hpwl_before = current_hpwl;
    const double hpwl_after_orientation = OptimizeColumnOrientationPhases();
    RunRowLocationOptimizationStage();
    current_hpwl = WeightedHPWL();
    ++completed_rounds;

    LOG(info) << "  Joint orientation/row-location round " << round + 1 << ":\n"
              << "    HPWL before        : " << hpwl_before << "um\n"
              << "    after orientation  : " << hpwl_after_orientation << "um\n"
              << "    after row movement : " << current_hpwl << "um\n"
              << "    improvement        : " << hpwl_before - current_hpwl
              << "um\n";
    EmitSnapshot("orientation_row_location",
                 "After Joint Orientation and Row Location", "legalization",
                 "orientation_row_location", round);
    const double relative_improvement =
        (hpwl_before - current_hpwl) / std::max(1.0, hpwl_before);
    if (relative_improvement <= kMinimumRelativeImprovement) {
      break;
    }
  }
  timer.RecordEndTime();

  LOG(info) << "  Joint orientation/row-location summary:\n"
            << "    completed rounds : " << completed_rounds << "\n"
            << "    initial HPWL      : " << initial_hpwl << "um\n"
            << "    final HPWL        : " << current_hpwl << "um\n"
            << "    improvement       : " << initial_hpwl - current_hpwl
            << "um\n"
            << "    wall time         : " << timer.GetWallTime() << "s\n";
  RecordPlacementHpwlMetrics("well_legalization.orientation_row_location",
                             *ckt_ptr_);
  RecordPlacementMetric(
      "time.well_legalization.orientation_row_location.wall_s",
      timer.GetWallTime());
  RecordPlacementMetric("time.well_legalization.orientation_row_location.cpu_s",
                        timer.GetCpuTime());
}

void GriddedCellWellLegalizer::RunPostClusteringStages(
    bool clustering_succeeded, bool run_read_only_analysis) {
  RunClusterOrientationStage();
  if (clustering_succeeded && enable_row_location_optimization_) {
    RunJointOrientationAndRowLocationOptimization();
  }
  if (clustering_succeeded && enable_vertical_hpwl_row_assignment_) {
    RunVerticalHpwlRowAssignmentStage();
  }
  if (clustering_succeeded && enable_ortools_row_optimization_) {
    RunOrToolsRowOptimizationStage();
  }
  if (clustering_succeeded && enable_exact_stripe_optimization_ &&
      exact_stripe_before_detailed_placement_) {
    RunExactStripeOptimizationStage();
  }
  if (clustering_succeeded && enable_exact_boundary_optimization_ &&
      exact_boundary_before_detailed_placement_) {
    RunExactBoundaryOptimizationStage();
  }
  if (clustering_succeeded &&
      (enable_local_reorder_ || enable_detailed_placement_)) {
    RunGriddedDetailedPlacementStage();
  }
  if (clustering_succeeded && enable_exact_stripe_optimization_ &&
      !exact_stripe_before_detailed_placement_) {
    RunExactStripeOptimizationStage();
  }
  if (clustering_succeeded && enable_exact_boundary_optimization_ &&
      !exact_boundary_before_detailed_placement_) {
    RunExactBoundaryOptimizationStage();
  }
  if (clustering_succeeded && run_read_only_analysis &&
      enable_exact_legalization_analysis_) {
    RunExactLegalizationAnalysisStage();
  }
  if (clustering_succeeded && run_read_only_analysis &&
      enable_whole_design_exact_legalization_) {
    RunWholeDesignExactLegalizationStage();
  }
}

bool GriddedCellWellLegalizer::RunMovableCellLegalizationStages() {
  bool is_success = RunComponentClusteringStage();
  RunPostClusteringStages(is_success);
  if (is_success) {
    SynchronizeComponentLocationsWithRows();
  }
  return is_success;
}

bool GriddedCellWellLegalizer::RetryMovableCellLegalizationWithScavenging() {
  if (stripe_mode_ == int(WellPartitionMode::kScavenge)) {
    return false;
  }
  LOG(warning) << "Strict well legalization failed; retry with scavenge mode\n";
  int previous_stripe_mode = stripe_mode_;
  stripe_mode_ = int(WellPartitionMode::kScavenge);
  ++snapshot_attempt_;
  RestoreInitialComponentLocation();
  InitializeWellLegalizer();
  bool is_success = RunMovableCellLegalizationStages();
  stripe_mode_ = previous_stripe_mode;
  return is_success;
}

bool GriddedCellWellLegalizer::RetryMovableCellLegalizationWithBalancing() {
  if (!enable_stripe_balancing_) return false;

  LOG(warning) << "Strict well legalization failed; rebalance neighboring "
                  "stripes using observed row overflow\n";
  GriddedCapacityConfig capacity_config = BuildGriddedCapacityConfig(1.0);
  unsigned long long previous_overflow =
      std::numeric_limits<unsigned long long>::max();
  int max_rounds = std::max(1, static_cast<int>(col_list_.size()));
  for (int round = 0; round < max_rounds; ++round) {
    RestoreInitialComponentLocation();
    GriddedStripeBalanceResult result =
        GriddedStripeBalancer(ckt_ptr_, capacity_config)
            .BalanceObservedOverflow(&col_list_);
    LOG(info) << "  Gridded stripe balancing round " << round + 1 << ":\n"
              << "    moved components            : "
              << result.moved_component_count << "\n"
              << "    overflowing stripes before : "
              << result.overflowing_stripes_before << "\n"
              << "    overflowing stripes after  : "
              << result.overflowing_stripes_after << "\n"
              << "    overflow area before/after : "
              << result.overflow_area_before << " / "
              << result.overflow_area_after << "\n";
    if (result.moved_component_count == 0 ||
        result.overflow_area_before >= previous_overflow) {
      return false;
    }
    previous_overflow = result.overflow_area_before;
    ++snapshot_attempt_;
    if (RunMovableCellLegalizationStages()) return true;
  }
  return false;
}

void GriddedCellWellLegalizer::RunWellTapStage() {
  if (disable_welltap_) {
    LOG(info) << "Skip inserting well tap cells\n";
  } else {
    LOG(info) << "Insert well tap cells\n";
    WellRowCompleter(ckt_ptr_, &col_list_, BuildRowCompletionConfig())
        .InsertWellTaps();
  }
  RecordPlacementHpwlMetrics("well_legalization.well_tap", *ckt_ptr_);
  EmitSnapshot("well_tap", "After Well Tap Insertion", "legalization",
               "well_tap");
}

void GriddedCellWellLegalizer::RunEndCapStage() {
  if (enable_end_cap_cell_) {
    LOG(info) << "Create end cap cells\n";
    WellRowCompleter(ckt_ptr_, &col_list_, BuildRowCompletionConfig())
        .InsertEndCaps();
  } else {
    LOG(info) << "Skip creating end cap cells\n";
  }
  EmitSnapshot("end_cap", "After End Cap Insertion", "legalization", "end_cap");
}

void GriddedCellWellLegalizer::RunPhysicalCompletionStages() {
  RunWellTapStage();
  RunEndCapStage();
}

void GriddedCellWellLegalizer::EmitSnapshot(const std::string& id,
                                            const std::string& label,
                                            const std::string& group,
                                            const std::string& subgroup,
                                            int iteration) {
  if (suppress_snapshots_ || !snapshot_callback_) return;
  snapshot_callback_("attempt_" + std::to_string(snapshot_attempt_) + "." + id,
                     label, group, subgroup, iteration);
}

bool GriddedCellWellLegalizer::StartPlacement() {
  PrintStartStatement("standard cluster well legalization");

  snapshot_attempt_ = 0;
  SaveInitialComponentLocation();
  bool is_success = false;
  if (enable_adaptive_stripe_boundaries_) {
    is_success = RunBestBoundaryClusteringStage();
    LogEstimatedGriddedCapacity();
    RunPostClusteringStages(is_success);
  } else if (enable_banded_stripe_assignment_ && enable_detailed_placement_) {
    is_success = RunBandedAssignmentPreviewStage();
    LogEstimatedGriddedCapacity();
  } else if (enable_vertical_hpwl_row_assignment_preview_ &&
             enable_detailed_placement_) {
    is_success = RunVerticalHpwlRowAssignmentPreviewStage();
    LogEstimatedGriddedCapacity();
  } else {
    InitializeWellLegalizer();
    LogEstimatedGriddedCapacity();
    is_success = RunMovableCellLegalizationStages();
  }
  if (!is_success) {
    is_success = RetryMovableCellLegalizationWithBalancing();
  }
  if (!is_success) {
    is_success = RetryMovableCellLegalizationWithScavenging();
  }
  if (!is_success) {
    LOG(error) << "Skip well tap, end cap, and well shape insertion because "
                  "movable-cell well legalization failed\n";
    PrintEndStatement("Standard Cluster Well Legalization", false);
    return false;
  }
  LogActualGriddedUtilization();
  RunPhysicalCompletionStages();
  is_success = ValidateFinalPlacement();

  PrintEndStatement("Standard Cluster Well Legalization", is_success);

  return is_success;
}

void GriddedCellWellLegalizer::LogEstimatedGriddedCapacity() {
  GriddedCapacityConfig config = BuildGriddedCapacityConfig(PlacementDensity());
  unsigned long long raw_component_area = 0;
  unsigned long long required_gridded_area = 0;
  unsigned long long available_gridded_area = 0;
  unsigned long long predicted_overflow_area = 0;
  int stripe_count = 0;
  int overflowing_stripe_count = 0;
  int estimated_row_count = 0;
  int unplaceable_component_count = 0;
  int single_region_fallback_count = 0;
  for (const StripeColumn& column : col_list_) {
    const GriddedStripeCapacitySummary summary =
        GriddedStripeCapacityModel(config).Estimate(column);
    stripe_count += static_cast<int>(summary.entries.size());
    overflowing_stripe_count += summary.overflowing_stripe_count;
    raw_component_area += summary.raw_component_area;
    required_gridded_area += summary.required_gridded_area;
    available_gridded_area += summary.available_gridded_area;
    predicted_overflow_area += summary.predicted_overflow_area;
    estimated_row_count += summary.estimated_row_count;
    unplaceable_component_count += summary.unplaceable_component_count;
    single_region_fallback_count += summary.single_region_fallback_count;
  }

  double raw_utilization =
      available_gridded_area == 0
          ? 0.0
          : raw_component_area / static_cast<double>(available_gridded_area);
  double gridded_utilization =
      available_gridded_area == 0
          ? 0.0
          : required_gridded_area / static_cast<double>(available_gridded_area);
  LOG(info)
      << "  Estimated gridded capacity:\n"
      << "    stripes                     : " << stripe_count << "\n"
      << "    predicted overflowing       : " << overflowing_stripe_count
      << "\n"
      << "    estimated rows              : " << estimated_row_count << "\n"
      << "    reserved width per row      : " << config.reserved_width << "\n"
      << "    target row density          : " << config.target_density << "\n"
      << "    raw component area          : " << raw_component_area << "\n"
      << "    required gridded area       : " << required_gridded_area << "\n"
      << "    available gridded area      : " << available_gridded_area << "\n"
      << "    predicted overflow area     : " << predicted_overflow_area << "\n"
      << "    unplaceable components      : " << unplaceable_component_count
      << "\n"
      << "    single-region fallbacks     : " << single_region_fallback_count
      << "\n"
      << "    raw/gridded utilization     : " << raw_utilization << " / "
      << gridded_utilization << "\n";
}

void GriddedCellWellLegalizer::LogActualGriddedUtilization() const {
  unsigned long long occupied_row_area = 0;
  unsigned long long allocated_row_area = 0;
  int row_count = 0;
  for (const StripeColumn& column : col_list_) {
    for (const Stripe& stripe : column.stripe_list_) {
      for (const GriddedRow& row : stripe.gridded_rows_) {
        int component_width = 0;
        for (const Component* component : row.Components()) {
          component_width += component->Width();
        }
        ++row_count;
        occupied_row_area +=
            static_cast<unsigned long long>(component_width) * row.Height();
        allocated_row_area +=
            static_cast<unsigned long long>(row.Width()) * row.Height();
      }
    }
  }
  LOG(info) << "  Actual gridded utilization:\n"
            << "    legalized rows              : " << row_count << "\n"
            << "    occupied row area           : " << occupied_row_area << "\n"
            << "    allocated row area          : " << allocated_row_area
            << "\n";
}

void GriddedCellWellLegalizer::GenMatlabClusterTable(
    std::string const& name_of_file) {
  std::string frame_file = name_of_file + "_outline.txt";
  ckt_ptr_->GenMATLABTable(frame_file);
  GenClusterTable(name_of_file, col_list_);
}

void GriddedCellWellLegalizer::GenMATLABWellTable(
    std::string const& name_of_file, int well_emit_mode) {
  ckt_ptr_->GenMATLABWellTable(name_of_file, false);

  GenMATLABWellFillingTable(name_of_file, col_list_, RegionBottom(),
                            RegionTop(), well_emit_mode);

  GenPPNP(name_of_file);
}

void GriddedCellWellLegalizer::GenPPNP(const std::string& name_of_file) {
  std::string np_file = name_of_file + "_np.txt";
  std::ofstream ostnp(np_file.c_str());
  DaliExpects(ostnp.is_open(), "Cannot open output file: " + np_file);

  std::string pp_file = name_of_file + "_pp.txt";
  std::ofstream ostpp(pp_file.c_str());
  DaliExpects(ostpp.is_open(), "Cannot open output file: " + pp_file);

  for (auto& col : col_list_) {
    for (auto& stripe : col.stripe_list_) {
      // draw NP and PP shapes from N/P-edge to N/P-edge
      std::vector<int> pn_edge_list;
      pn_edge_list.reserve(stripe.gridded_rows_.size() + 2);
      if (stripe.is_bottom_up_) {
        pn_edge_list.push_back(RegionBottom());
      } else {
        pn_edge_list.push_back(RegionTop());
      }
      for (auto& cluster : stripe.gridded_rows_) {
        pn_edge_list.push_back(cluster.LLY() + cluster.PNEdge());
      }
      if (stripe.is_bottom_up_) {
        pn_edge_list.push_back(RegionTop());
      } else {
        pn_edge_list.push_back(RegionBottom());
        std::reverse(pn_edge_list.begin(), pn_edge_list.end());
      }

      bool is_p_well_rect = stripe.is_first_row_orient_N_;
      int active_lx = LeftTapUx(stripe);
      int active_ux = RightTapLx(stripe);
      int ly;
      int uy;
      int rect_count = (int)pn_edge_list.size() - 1;
      for (int i = 0; i < rect_count; ++i) {
        ly = pn_edge_list[i];
        uy = pn_edge_list[i + 1];
        if (uy > ly && active_ux > active_lx) {
          if (is_p_well_rect) {
            ostnp << active_lx << "\t" << active_ux << "\t" << active_ux << "\t"
                  << active_lx << "\t" << ly << "\t" << ly << "\t" << uy << "\t"
                  << uy << "\n";
          } else {
            ostpp << active_lx << "\t" << active_ux << "\t" << active_ux << "\t"
                  << active_lx << "\t" << ly << "\t" << ly << "\t" << uy << "\t"
                  << uy << "\n";
          }
        }
        is_p_well_rect = !is_p_well_rect;
      }

      // draw NP and PP shapes from well-tap cell to well-tap cell
      std::vector<int> well_tap_top_bottom_list;
      well_tap_top_bottom_list.reserve(stripe.gridded_rows_.size() + 2);
      if (stripe.is_bottom_up_) {
        well_tap_top_bottom_list.push_back(RegionBottom());
      } else {
        well_tap_top_bottom_list.push_back(RegionTop());
      }
      for (auto& cluster : stripe.gridded_rows_) {
        Component* well_tap = cluster.WellTapCell();
        DaliExpects(well_tap != nullptr,
                    "Cannot emit P+/N+ tap regions without a well tap cell");
        if (stripe.is_bottom_up_) {
          well_tap_top_bottom_list.push_back(well_tap->LLY());
          well_tap_top_bottom_list.push_back(well_tap->URY());
        } else {
          well_tap_top_bottom_list.push_back(well_tap->URY());
          well_tap_top_bottom_list.push_back(well_tap->LLY());
        }
      }
      if (stripe.is_bottom_up_) {
        well_tap_top_bottom_list.push_back(RegionTop());
      } else {
        well_tap_top_bottom_list.push_back(RegionBottom());
        std::reverse(well_tap_top_bottom_list.begin(),
                     well_tap_top_bottom_list.end());
      }
      DaliExpects(well_tap_top_bottom_list.size() % 2 == 0,
                  "Impossible to get an even number of well tap cell edges");

      is_p_well_rect = stripe.is_first_row_orient_N_;
      int lx0 = LeftTapLx(stripe);
      int ux0 = LeftTapUx(stripe);
      int lx1 = RightTapLx(stripe);
      int ux1 = RightTapUx(stripe);
      rect_count = (int)well_tap_top_bottom_list.size() - 1;
      for (int i = 0; i < rect_count; i += 2) {
        ly = well_tap_top_bottom_list[i];
        uy = well_tap_top_bottom_list[i + 1];
        if (uy > ly) {
          if (is_p_well_rect) {
            ostpp << lx0 << "\t" << ux0 << "\t" << ux0 << "\t" << lx0 << "\t"
                  << ly << "\t" << ly << "\t" << uy << "\t" << uy << "\n";
            ostpp << lx1 << "\t" << ux1 << "\t" << ux1 << "\t" << lx1 << "\t"
                  << ly << "\t" << ly << "\t" << uy << "\t" << uy << "\n";
          } else {
            ostnp << lx0 << "\t" << ux0 << "\t" << ux0 << "\t" << lx0 << "\t"
                  << ly << "\t" << ly << "\t" << uy << "\t" << uy << "\n";
            ostnp << lx1 << "\t" << ux1 << "\t" << ux1 << "\t" << lx1 << "\t"
                  << ly << "\t" << ly << "\t" << uy << "\t" << uy << "\n";
          }
        }
        is_p_well_rect = !is_p_well_rect;
      }
    }
  }
  ostnp.close();
  ostpp.close();
}

/****
 * Emit three files:
 * 1. rect file including all N/P well rectangles
 * 2. rect file including all NP/PP rectangles
 * 3. cluster file including all cluster shapes
 *
 * @param well_mode
 * 0: emit both N-well and P-well
 * 1: emit N-well only
 * 2: emit P-well only
 *
 * @param enable_emitting_cluster
 * True: emit cluster file
 * Fale: do not emit this file
 * ****/
void GriddedCellWellLegalizer::EmitDEFWellFile(std::string const& name_of_file,
                                               int well_emit_mode,
                                               bool enable_emitting_cluster) {
  EmitPPNPRect(name_of_file + "ppnp.rect");
  EmitWellRect(name_of_file + "well.rect", well_emit_mode);
  if (enable_emitting_cluster) {
    EmitClusterRect(name_of_file + "_router.cluster");
  }
}

void GriddedCellWellLegalizer::EmitPPNPRect(std::string const& name_of_file) {
  std::vector<WellGeometryRect> geometry =
      WellGeometryBuilder(col_list_, RegionBottom(), RegionTop()).Build(true);
  WellGeometryExporter(
      ckt_ptr_, RectI(RegionLeft(), RegionBottom(), RegionRight(), RegionTop()),
      geometry)
      .EmitImplantRectFile(name_of_file);
}

void GriddedCellWellLegalizer::ExportPpNpToPhyDB(phydb::PhyDB* phydb_ptr) {
  if (disable_welltap_) {
    LOG(info) << "Skip export Pplus/Nplus fillings to PhyDB "
                 "since well tap is disabled\n";
    return;
  }
  std::vector<WellGeometryRect> geometry =
      WellGeometryBuilder(col_list_, RegionBottom(), RegionTop()).Build(true);
  WellGeometryExporter(
      ckt_ptr_, RectI(RegionLeft(), RegionBottom(), RegionRight(), RegionTop()),
      geometry)
      .ExportImplantsToPhyDB(phydb_ptr);
}

void GriddedCellWellLegalizer::EmitWellRect(std::string const& name_of_file,
                                            int well_emit_mode) {
  std::vector<WellGeometryRect> geometry =
      WellGeometryBuilder(col_list_, RegionBottom(), RegionTop()).Build(false);
  WellGeometryExporter(
      ckt_ptr_, RectI(RegionLeft(), RegionBottom(), RegionRight(), RegionTop()),
      geometry)
      .EmitWellRectFile(name_of_file, well_emit_mode);
}

void GriddedCellWellLegalizer::ExportWellToPhyDB(phydb::PhyDB* phydb_ptr,
                                                 int well_emit_mode) {
  if (disable_welltap_) {
    LOG(info) << "Skip export wells to PhyDB since well tap is disabled\n";
    return;
  }
  std::vector<WellGeometryRect> geometry =
      WellGeometryBuilder(col_list_, RegionBottom(), RegionTop()).Build(false);
  WellGeometryExporter(
      ckt_ptr_, RectI(RegionLeft(), RegionBottom(), RegionRight(), RegionTop()),
      geometry)
      .ExportWellsToPhyDB(phydb_ptr, well_emit_mode);
}

std::vector<PlacementWellRect>
GriddedCellWellLegalizer::CollectWellVisualizationRects() {
  WellGeometryBuilder builder(col_list_, RegionBottom(), RegionTop());
  std::vector<WellGeometryRect> geometry =
      builder.Build(!disable_welltap_ && well_tap_macro_ != nullptr);

  std::vector<PlacementWellRect> result;
  result.reserve(geometry.size());
  for (const WellGeometryRect& geometry_rect : geometry) {
    PlacementWellLayer layer = PlacementWellLayer::kPwell;
    switch (geometry_rect.layer) {
      case WellGeometryLayer::kPwell:
        layer = PlacementWellLayer::kPwell;
        break;
      case WellGeometryLayer::kNwell:
        layer = PlacementWellLayer::kNwell;
        break;
      case WellGeometryLayer::kPplus:
        layer = PlacementWellLayer::kPplus;
        break;
      case WellGeometryLayer::kNplus:
        layer = PlacementWellLayer::kNplus;
        break;
    }
    const RectI& rect = geometry_rect.bounds;
    result.push_back({static_cast<float>(rect.LLX() * ckt_ptr_->GridValueX()),
                      static_cast<float>(rect.LLY() * ckt_ptr_->GridValueY()),
                      static_cast<float>(rect.URX() * ckt_ptr_->GridValueX()),
                      static_cast<float>(rect.URY() * ckt_ptr_->GridValueY()),
                      layer});
  }
  return result;
}

/****
 * Emits a rect file for power routing
 * ****/
void GriddedCellWellLegalizer::EmitClusterRect(
    std::string const& name_of_file) {
  LOG(info) << "Writing cluster rect file: " << name_of_file << "\n";
  std::ofstream ost(name_of_file.c_str());
  DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);

  double factor_x = ckt_ptr_->DistanceScaleFactorX();
  double factor_y = ckt_ptr_->DistanceScaleFactorY();
  for (size_t i = 0; i < col_list_.size(); ++i) {
    std::string column_name = "column" + std::to_string(i);
    ost << "STRIP " << column_name << "\n";

    auto& col = col_list_[i];
    for (auto& stripe : col.stripe_list_) {
      ost << "  "
          << (int)(stripe.LLX() * factor_x) +
                 ckt_ptr_->design().DieAreaOffsetX()
          << "  "
          << (int)(stripe.URX() * factor_x) +
                 ckt_ptr_->design().DieAreaOffsetX()
          << "  ";
      if (stripe.is_first_row_orient_N_) {
        ost << "GND\n";
      } else {
        ost << "Vdd\n";
      }

      if (stripe.is_bottom_up_) {
        for (auto& cluster : stripe.gridded_rows_) {
          ost << "  "
              << (int)(cluster.LLY() * factor_y) +
                     ckt_ptr_->design().DieAreaOffsetY()
              << "  "
              << (int)(cluster.URY() * factor_y) +
                     ckt_ptr_->design().DieAreaOffsetY()
              << "\n";
        }
      } else {
        int sz = stripe.gridded_rows_.size();
        for (int j = sz - 1; j >= 0; --j) {
          auto& cluster = stripe.gridded_rows_[j];
          ost << "  "
              << (int)(cluster.LLY() * factor_y) +
                     ckt_ptr_->design().DieAreaOffsetY()
              << "  "
              << (int)(cluster.URY() * factor_y) +
                     ckt_ptr_->design().DieAreaOffsetY()
              << "\n";
        }
      }

      ost << "END " << column_name << "\n\n";
    }
  }
  ost.close();
}

}  // namespace dali
