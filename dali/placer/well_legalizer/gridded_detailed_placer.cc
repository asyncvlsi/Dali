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
#include "dali/placer/well_legalizer/gridded_detailed_placer.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <map>
#include <set>
#include <unordered_set>
#include <utility>

#include "dali/common/elapsed_time.h"
#include "dali/common/logging.h"
#include "dali/common/placement_metrics.h"
#include "dali/placer/well_legalizer/gridded_row_assignment_transaction.h"

namespace dali {

void GriddedDetailedPlacer::MoveStats::Add(const MoveStats& other) {
  candidates += other.candidates;
  source_singleton += other.source_singleton;
  width_blocked += other.width_blocked;
  p_well_blocked += other.p_well_blocked;
  n_well_blocked += other.n_well_blocked;
  evaluated += other.evaluated;
  no_hpwl_improvement += other.no_hpwl_improvement;
  accepted += other.accepted;
  ejection_attempts += other.ejection_attempts;
  ejection_evaluated += other.ejection_evaluated;
  ejection_no_hpwl_improvement += other.ejection_no_hpwl_improvement;
  ejection_accepted += other.ejection_accepted;
  cycle_attempts += other.cycle_attempts;
  cycle_evaluated += other.cycle_evaluated;
  cycle_no_hpwl_improvement += other.cycle_no_hpwl_improvement;
  cycle_accepted += other.cycle_accepted;
  cycle_invalidated += other.cycle_invalidated;
  batch_plans += other.batch_plans;
  batch_selected += other.batch_selected;
  batch_accepted += other.batch_accepted;
  batch_passes += other.batch_passes;
  insertion_positions_evaluated += other.insertion_positions_evaluated;
}

void GriddedDetailedPlacer::ClusterStats::Add(const ClusterStats& other) {
  visited_rows += other.visited_rows;
  changed_rows += other.changed_rows;
  accepted_rows += other.accepted_rows;
}

struct GriddedRowSnapshot {
  GriddedRow* row = nullptr;
  std::vector<Component*> component_order;
  std::vector<double> component_lx;
  std::vector<double> component_ly;
  std::vector<ComponentOrient> component_orient;
};

static std::vector<GriddedRowSnapshot> SaveRowState(
    const std::vector<GriddedRow*>& rows) {
  std::vector<GriddedRowSnapshot> snapshots;
  snapshots.reserve(rows.size());
  for (GriddedRow* row : rows) {
    GriddedRowSnapshot snapshot;
    snapshot.row = row;
    snapshot.component_order = row->Components();
    snapshot.component_lx.reserve(row->Components().size());
    snapshot.component_ly.reserve(row->Components().size());
    snapshot.component_orient.reserve(row->Components().size());
    for (Component* component : row->Components()) {
      snapshot.component_lx.push_back(component->LLX());
      snapshot.component_ly.push_back(component->LLY());
      snapshot.component_orient.push_back(component->Orient());
    }
    snapshots.push_back(snapshot);
  }
  return snapshots;
}

static void RestoreRowState(const std::vector<GriddedRowSnapshot>& snapshots) {
  for (const GriddedRowSnapshot& snapshot : snapshots) {
    snapshot.row->Components() = snapshot.component_order;
    for (size_t i = 0; i < snapshot.component_order.size(); ++i) {
      snapshot.component_order[i]->SetLLX(snapshot.component_lx[i]);
      snapshot.component_order[i]->SetLLY(snapshot.component_ly[i]);
      snapshot.component_order[i]->SetOrient(snapshot.component_orient[i]);
    }
  }
}

void GriddedDetailedPlacer::SetRows(std::vector<GriddedRow*> rows) {
  rows_ = std::move(rows);
  BuildRowStripeIndex();
}

void GriddedDetailedPlacer::SetSnapshotCallback(
    SnapshotCallback snapshot_callback) {
  snapshot_callback_ = std::move(snapshot_callback);
}

void GriddedDetailedPlacer::SetMaxRounds(int max_rounds) {
  DaliExpects(max_rounds >= 0,
              "Gridded detailed placement rounds cannot be negative");
  max_rounds_ = max_rounds;
}

void GriddedDetailedPlacer::SetMinRelativeImprovement(
    double min_relative_improvement) {
  DaliExpects(
      min_relative_improvement >= 0.0 && min_relative_improvement <= 1.0,
      "Gridded detailed relative improvement must be in [0, 1]");
  min_relative_improvement_ = min_relative_improvement;
}

void GriddedDetailedPlacer::SetEnableVerticalSwap(bool enable) {
  enable_vertical_swap_ = enable;
}

void GriddedDetailedPlacer::SetEnableRelocation(bool enable) {
  enable_relocation_ = enable;
}

void GriddedDetailedPlacer::SetEnableBatchedAssignmentMoves(bool enable) {
  enable_batched_assignment_moves_ = enable;
}

void GriddedDetailedPlacer::SetExhaustiveInsertionPositions(bool enable) {
  exhaustive_insertion_positions_ = enable;
}

void GriddedDetailedPlacer::SetEnableSafePairMerge(bool enable) {
  enable_safe_pair_merge_ = enable;
}

void GriddedDetailedPlacer::SetWeightedClustering(bool enable) {
  weighted_clustering_ = enable;
}

void GriddedDetailedPlacer::SetMaxCandidateRows(int max_candidate_rows) {
  DaliExpects(max_candidate_rows >= 1,
              "Gridded detailed candidate-row cap must be positive");
  max_candidate_rows_ = max_candidate_rows;
}

void GriddedDetailedPlacer::SetNetIgnoreThreshold(int net_ignore_threshold) {
  DaliExpects(net_ignore_threshold > 1,
              "Net ignore threshold must be greater than one");
  net_ignore_threshold_ = static_cast<size_t>(net_ignore_threshold);
}

double GriddedDetailedPlacer::WireLengthCost(GriddedRow* row, int left_index,
                                             int right_index) const {
  auto& net_list = ckt_ptr_->Nets();
  std::set<Net*> involved_nets;
  for (int i = left_index; i <= right_index; ++i) {
    Component* component = row->Components()[i];
    for (int net_id : component->NetList()) {
      if (net_list[net_id].PinCnt() < net_ignore_threshold_) {
        involved_nets.insert(&net_list[net_id]);
      }
    }
  }

  double hpwl_x = 0;
  double hpwl_y = 0;
  for (Net* net : involved_nets) {
    hpwl_x += net->WeightedHPWLX();
    hpwl_y += net->WeightedHPWLY();
  }

  return hpwl_x * ckt_ptr_->GridValueX() + hpwl_y * ckt_ptr_->GridValueY();
}

void GriddedDetailedPlacer::FindBestLocalOrder(std::vector<Component*>& result,
                                               double& cost, GriddedRow* row,
                                               int current_index,
                                               int left_index, int right_index,
                                               int left_bound, int right_bound,
                                               int gap, int window_size) const {
  if (current_index == right_index) {
    row->Components()[left_index]->SetLLX(left_bound);
    row->Components()[right_index]->SetURX(right_bound);

    int left_contour =
        left_bound + gap + row->Components()[left_index]->Width();
    for (int i = left_index + 1; i < right_index; ++i) {
      Component* component = row->Components()[i];
      component->SetLLX(left_contour);
      left_contour += component->Width() + gap;
    }

    double candidate_cost = WireLengthCost(row, left_index, right_index);
    if (candidate_cost < cost) {
      cost = candidate_cost;
      for (int i = 0; i < window_size; ++i) {
        result[i] = row->Components()[left_index + i];
      }
    }
    return;
  }

  auto& components = row->Components();
  for (int i = current_index; i <= right_index; ++i) {
    std::swap(components[current_index], components[i]);
    FindBestLocalOrder(result, cost, row, current_index + 1, left_index,
                       right_index, left_bound, right_bound, gap, window_size);
    std::swap(components[current_index], components[i]);
  }
}

int GriddedDetailedPlacer::LocalReorderInRow(GriddedRow* row,
                                             int window_size) const {
  int component_count = static_cast<int>(row->Components().size());
  if (component_count < window_size) {
    return 0;
  }

  std::sort(row->Components().begin(), row->Components().end(),
            [](const Component* lhs, const Component* rhs) {
              return lhs->LLX() < rhs->LLX();
            });

  int reorder_count = 0;
  int last_start = component_count - window_size;
  std::vector<Component*> best_order(window_size, nullptr);
  for (int left_index = 0; left_index <= last_start; ++left_index) {
    int total_width = 0;
    std::vector<Component*> original_order(window_size, nullptr);
    for (int i = 0; i < window_size; ++i) {
      best_order[i] = row->Components()[left_index + i];
      original_order[i] = best_order[i];
      total_width += best_order[i]->Width();
    }

    int right_index = left_index + window_size - 1;
    int left_bound = static_cast<int>(row->Components()[left_index]->LLX());
    int right_bound = static_cast<int>(row->Components()[right_index]->URX());
    int gap = (right_bound - left_bound - total_width) / (window_size - 1);
    double best_cost = DBL_MAX;

    FindBestLocalOrder(best_order, best_cost, row, left_index, left_index,
                       right_index, left_bound, right_bound, gap, window_size);
    for (int i = 0; i < window_size; ++i) {
      row->Components()[left_index + i] = best_order[i];
    }
    if (best_order != original_order) {
      ++reorder_count;
    }

    row->Components()[left_index]->SetLLX(left_bound);
    row->Components()[right_index]->SetURX(right_bound);
    int left_contour =
        left_bound + row->Components()[left_index]->Width() + gap;
    for (int i = left_index + 1; i < right_index; ++i) {
      Component* component = row->Components()[i];
      component->SetLLX(left_contour);
      left_contour += component->Width() + gap;
    }
  }

  return reorder_count;
}

int GriddedDetailedPlacer::LocalReorderAllRows() {
  std::sort(rows_.begin(), rows_.end(),
            [](const GriddedRow* lhs, const GriddedRow* rhs) {
              return (lhs->LLY() < rhs->LLY()) ||
                     (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
            });

  int reorder_count = 0;
  for (GriddedRow* row : rows_) {
    reorder_count += LocalReorderInRow(row, kLocalReorderWindowSize);
  }
  return reorder_count;
}

int GriddedDetailedPlacer::RunLocalReorderStage(bool log_progress) {
  double previous_hpwl = WeightedHPWL();
  int total_changed_windows = 0;
  int iteration_count = 0;
  for (int iteration = 0; iteration < kMaxLocalReorderIterations; ++iteration) {
    auto row_state_before_iteration = SaveRowState(rows_);
    int reorder_windows = LocalReorderAllRows();
    double current_hpwl = WeightedHPWL();
    double improvement = previous_hpwl - current_hpwl;
    if (log_progress) {
      LOG(info) << "  local reorder iteration " << iteration
                << ": windows=" << reorder_windows << ", HPWL=" << current_hpwl
                << "um"
                << ", improvement=" << improvement << "um\n";
    }
    if (improvement <= kMinSignificantHpwlImprovement) {
      RestoreRowState(row_state_before_iteration);
      if (log_progress) {
        LOG(info) << "    rejected: no significant HPWL improvement\n";
      }
      break;
    }
    total_changed_windows += reorder_windows;
    RecordPlacementHpwlMetrics("gridded_detailed.local_reorder", *ckt_ptr_);
    ++iteration_count;
    previous_hpwl = current_hpwl;
  }
  RecordPlacementHpwlMetrics("gridded_detailed.local_reorder", *ckt_ptr_);

  if (log_progress) {
    LOG(info) << "  local reorder iterations: " << iteration_count << "\n"
              << "  accepted reorder windows: " << total_changed_windows
              << "\n";
  }
  return total_changed_windows;
}

bool GriddedDetailedPlacer::IsSwapCandidate(Component* component) const {
  return component->IsMovable() && !component->NetList().empty() &&
         component->MacroPtr() != ckt_ptr_->tech().IoDummyMacroPtr();
}

GriddedDetailedPlacer::RowRequirements
GriddedDetailedPlacer::ComputeRowRequirementsAfterAssignment(
    GriddedRow* row, Component* removed, Component* added) const {
  RowRequirements requirements;
  for (Component* component : row->Components()) {
    if (component == removed) {
      continue;
    }
    requirements.used_width += component->Width();
    requirements.p_well_height = std::max(
        requirements.p_well_height, component->MacroPtr()->FirstPwellHeight());
    requirements.n_well_height = std::max(
        requirements.n_well_height, component->MacroPtr()->FirstNwellHeight());
  }
  if (added != nullptr) {
    requirements.used_width += added->Width();
    requirements.p_well_height = std::max(
        requirements.p_well_height, added->MacroPtr()->FirstPwellHeight());
    requirements.n_well_height = std::max(
        requirements.n_well_height, added->MacroPtr()->FirstNwellHeight());
  }
  return requirements;
}

bool GriddedDetailedPlacer::IsNonHeightIncreasingSwap(
    GriddedRow* first_row, Component* first_component, GriddedRow* second_row,
    Component* second_component) const {
  RowRequirements first_requirements = ComputeRowRequirementsAfterAssignment(
      first_row, first_component, second_component);
  RowRequirements second_requirements = ComputeRowRequirementsAfterAssignment(
      second_row, second_component, first_component);
  return first_requirements.used_width <= first_row->UsableWidth() &&
         second_requirements.used_width <= second_row->UsableWidth() &&
         first_requirements.p_well_height <= first_row->PHeight() &&
         first_requirements.n_well_height <= first_row->NHeight() &&
         second_requirements.p_well_height <= second_row->PHeight() &&
         second_requirements.n_well_height <= second_row->NHeight();
}

std::vector<int> GriddedDetailedPlacer::CollectRowNetIds(
    const std::vector<GriddedRow*>& rows) const {
  std::vector<int> net_ids;
  auto collect_row_net_ids = [&net_ids](GriddedRow* row) {
    for (Component* component : row->Components()) {
      for (int net_id : component->NetList()) {
        net_ids.push_back(net_id);
      }
    }
  };
  for (GriddedRow* row : rows) {
    collect_row_net_ids(row);
  }
  std::sort(net_ids.begin(), net_ids.end());
  net_ids.erase(std::unique(net_ids.begin(), net_ids.end()), net_ids.end());
  return net_ids;
}

double GriddedDetailedPlacer::NetWireLengthCost(
    const std::vector<int>& net_ids) const {
  double cost = 0;
  for (int net_id : net_ids) {
    cost += ckt_ptr_->NetWeightedHPWL(net_id);
  }
  return cost;
}

double GriddedDetailedPlacer::DistanceToOptimalRegionX(
    Component* component, const OptimalRegion& region) const {
  if (component->LLX() < region.lx) {
    return region.lx - component->LLX();
  }
  if (component->LLX() > region.ux) {
    return component->LLX() - region.ux;
  }
  return 0;
}

double GriddedDetailedPlacer::DistanceToOptimalRegionY(
    GriddedRow* row, Component* component, const OptimalRegion& region) const {
  double lly = row->LLY();
  if (row->IsOrientN()) {
    lly += row->PHeight() - component->MacroPtr()->FirstPwellHeight();
  } else {
    lly += row->NHeight() - component->MacroPtr()->FirstNwellHeight();
  }

  if (lly < region.ly) {
    return region.ly - lly;
  }
  if (lly > region.uy) {
    return lly - region.uy;
  }
  return 0;
}

double GriddedDetailedPlacer::PhysicalDistanceToOptimalRegion(
    GriddedRow* row, Component* component, const OptimalRegion& region) const {
  return DistanceToOptimalRegionX(component, region) * ckt_ptr_->GridValueX() +
         DistanceToOptimalRegionY(row, component, region) *
             ckt_ptr_->GridValueY();
}

double GriddedDetailedPlacer::DistanceFromRowToOptimalRegionX(
    GriddedRow* row, Component* component, const OptimalRegion& region) const {
  double min_lx = row->LLX() + row->LeftBoundaryMargin();
  double max_lx = row->URX() - row->RightBoundaryMargin() - component->Width();
  if (max_lx < region.lx) {
    return region.lx - max_lx;
  }
  if (min_lx > region.ux) {
    return min_lx - region.ux;
  }
  return 0;
}

double GriddedDetailedPlacer::PhysicalDistanceFromRowToOptimalRegion(
    GriddedRow* row, Component* component, const OptimalRegion& region) const {
  return DistanceFromRowToOptimalRegionX(row, component, region) *
             ckt_ptr_->GridValueX() +
         DistanceToOptimalRegionY(row, component, region) *
             ckt_ptr_->GridValueY();
}

std::vector<GriddedDetailedPlacer::CandidateRow>
GriddedDetailedPlacer::FindCandidateRows(GriddedRow* source_row,
                                         Component* component,
                                         const OptimalRegion& region) const {
  double current_distance =
      PhysicalDistanceToOptimalRegion(source_row, component, region);
  if (current_distance <= kMinSignificantHpwlImprovement) {
    return {};
  }

  std::vector<CandidateRow> candidate_rows;
  candidate_rows.reserve(row_stripes_.size() * kMaxOptimalRegionRowsPerStripe);
  const OptimalRegion n_region = ComputeOptimalRegion(component, N);
  const OptimalRegion fs_region = ComputeOptimalRegion(component, FS);
  for (const RowStripe& stripe : row_stripes_) {
    std::vector<CandidateRow> stripe_candidates;
    for (const OptimalRegion* target_region : {&n_region, &fs_region}) {
      if (!target_region->valid) continue;
      const bool target_orient_n = target_region == &n_region;
      const double target_y = (target_region->ly + target_region->uy) / 2.0;
      auto nearest = std::lower_bound(
          stripe.rows.begin(), stripe.rows.end(), target_y,
          [](const GriddedRow* row, double y) { return row->CenterY() < y; });
      const int nearest_index = static_cast<int>(nearest - stripe.rows.begin());
      const int first_index = std::max(0, nearest_index - 2);
      const int end_index =
          std::min(static_cast<int>(stripe.rows.size()), nearest_index + 2);
      for (int i = first_index; i < end_index; ++i) {
        GriddedRow* row = stripe.rows[i];
        if (row == source_row || row->IsOrientN() != target_orient_n) {
          continue;
        }
        const double row_distance = PhysicalDistanceFromRowToOptimalRegion(
            row, component, *target_region);
        if (row_distance < current_distance) {
          stripe_candidates.push_back({row, row_distance, *target_region});
        }
      }
    }
    std::sort(stripe_candidates.begin(), stripe_candidates.end(),
              [](const CandidateRow& lhs, const CandidateRow& rhs) {
                if (lhs.distance != rhs.distance) {
                  return lhs.distance < rhs.distance;
                }
                if (lhs.row->LLY() != rhs.row->LLY()) {
                  return lhs.row->LLY() < rhs.row->LLY();
                }
                return lhs.row->LLX() < rhs.row->LLX();
              });
    int stripe_limit = std::min(kMaxOptimalRegionRowsPerStripe,
                                static_cast<int>(stripe_candidates.size()));
    candidate_rows.insert(candidate_rows.end(), stripe_candidates.begin(),
                          stripe_candidates.begin() + stripe_limit);
  }
  std::sort(candidate_rows.begin(), candidate_rows.end(),
            [](const CandidateRow& lhs, const CandidateRow& rhs) {
              if (lhs.distance != rhs.distance) {
                return lhs.distance < rhs.distance;
              }
              if (lhs.row->LLY() != rhs.row->LLY()) {
                return lhs.row->LLY() < rhs.row->LLY();
              }
              return lhs.row->LLX() < rhs.row->LLX();
            });
  if (candidate_rows.size() > static_cast<size_t>(max_candidate_rows_)) {
    candidate_rows.resize(max_candidate_rows_);
  }
  return candidate_rows;
}

std::vector<GriddedDetailedPlacer::CandidateRow>
GriddedDetailedPlacer::FindEjectionDestinationRows(GriddedRow* source_row,
                                                   GriddedRow* target_row,
                                                   Component* component) const {
  std::vector<CandidateRow> candidate_rows;
  const OptimalRegion n_region = ComputeOptimalRegion(component, N);
  const OptimalRegion fs_region = ComputeOptimalRegion(component, FS);
  for (const RowStripe& stripe : row_stripes_) {
    for (const OptimalRegion* target_region : {&n_region, &fs_region}) {
      if (!target_region->valid) continue;
      const bool target_orient_n = target_region == &n_region;
      const double target_y = (target_region->ly + target_region->uy) / 2.0;
      auto nearest = std::lower_bound(
          stripe.rows.begin(), stripe.rows.end(), target_y,
          [](const GriddedRow* row, double y) { return row->CenterY() < y; });
      const int nearest_index = static_cast<int>(nearest - stripe.rows.begin());
      const int first_index = std::max(0, nearest_index - 2);
      const int end_index =
          std::min(static_cast<int>(stripe.rows.size()), nearest_index + 2);
      for (int i = first_index; i < end_index; ++i) {
        GriddedRow* row = stripe.rows[i];
        if (row == source_row || row == target_row ||
            row->IsOrientN() != target_orient_n) {
          continue;
        }
        RowRequirements requirements =
            ComputeRowRequirementsAfterAssignment(row, nullptr, component);
        if (requirements.used_width > row->UsableWidth() ||
            requirements.p_well_height > row->PHeight() ||
            requirements.n_well_height > row->NHeight()) {
          continue;
        }
        const double distance = PhysicalDistanceFromRowToOptimalRegion(
            row, component, *target_region);
        candidate_rows.push_back({row, distance, *target_region});
      }
    }
  }
  std::sort(candidate_rows.begin(), candidate_rows.end(),
            [](const CandidateRow& lhs, const CandidateRow& rhs) {
              if (lhs.distance != rhs.distance) {
                return lhs.distance < rhs.distance;
              }
              if (lhs.row->LLY() != rhs.row->LLY()) {
                return lhs.row->LLY() < rhs.row->LLY();
              }
              return lhs.row->LLX() < rhs.row->LLX();
            });
  if (candidate_rows.size() > kMaxEjectionDestinationRows) {
    candidate_rows.resize(kMaxEjectionDestinationRows);
  }
  return candidate_rows;
}

std::vector<GriddedDetailedPlacer::DisplacementCandidate>
GriddedDetailedPlacer::FindDisplacementCandidates(GriddedRow* target_row,
                                                  Component* incoming) const {
  std::vector<DisplacementCandidate> candidates;
  for (Component* displaced : target_row->Components()) {
    if (!IsSwapCandidate(displaced)) {
      continue;
    }
    RowRequirements target_requirements =
        ComputeRowRequirementsAfterAssignment(target_row, displaced, incoming);
    if (target_requirements.used_width > target_row->UsableWidth() ||
        target_requirements.p_well_height > target_row->PHeight() ||
        target_requirements.n_well_height > target_row->NHeight()) {
      continue;
    }
    OptimalRegion region = ComputeOptimalRegion(displaced);
    if (!region.valid) {
      continue;
    }
    double current_distance =
        PhysicalDistanceToOptimalRegion(target_row, displaced, region);
    candidates.push_back({displaced, region, current_distance});
  }
  std::sort(
      candidates.begin(), candidates.end(),
      [](const DisplacementCandidate& lhs, const DisplacementCandidate& rhs) {
        return lhs.current_distance > rhs.current_distance;
      });
  if (candidates.size() > kMaxEjectionComponentsPerTarget) {
    candidates.resize(kMaxEjectionComponentsPerTarget);
  }
  return candidates;
}

double GriddedDetailedPlacer::ComputeMoveTargetX(
    GriddedRow* target_row, Component* component,
    const OptimalRegion& region) const {
  double min_lx = target_row->LLX() + target_row->LeftBoundaryMargin();
  double max_lx = target_row->URX() - target_row->RightBoundaryMargin() -
                  component->Width();
  if (max_lx < region.lx) {
    return max_lx;
  }
  if (min_lx > region.ux) {
    return min_lx;
  }
  double overlap_lx = std::max(min_lx, region.lx);
  double overlap_ux = std::min(max_lx, region.ux);
  return std::clamp(component->LLX(), overlap_lx, overlap_ux);
}

std::pair<double, double> GriddedDetailedPlacer::ComputeWeightedMedianInterval(
    std::vector<std::pair<double, double>> weighted_bounds) {
  DaliExpects(!weighted_bounds.empty(),
              "Cannot compute a weighted median without bounds");
  std::sort(weighted_bounds.begin(), weighted_bounds.end());

  double total_weight = 0;
  for (const auto& bound : weighted_bounds) {
    double weight = bound.second;
    DaliExpects(weight > 0, "Weighted-median bounds must have positive weight");
    total_weight += weight;
  }

  double half_weight = total_weight / 2.0;
  double cumulative_weight = 0;
  double lower = weighted_bounds.back().first;
  double upper = lower;
  bool lower_found = false;
  for (const auto& [value, weight] : weighted_bounds) {
    cumulative_weight += weight;
    if (!lower_found && cumulative_weight >= half_weight) {
      lower = value;
      lower_found = true;
    }
    if (cumulative_weight > half_weight) {
      upper = value;
      break;
    }
  }
  return {lower, upper};
}

GriddedDetailedPlacer::OptimalRegion
GriddedDetailedPlacer::ComputeOptimalRegion(Component* component,
                                            ComponentOrient orientation) const {
  std::vector<std::pair<double, double>> x_bounds;
  std::vector<std::pair<double, double>> y_bounds;
  auto& nets = ckt_ptr_->Nets();
  for (int net_id : component->NetList()) {
    Net& net = nets[net_id];
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) {
      continue;
    }

    bool found_component_pin = false;
    double offset_x = 0;
    double offset_y = 0;
    double min_x = DBL_MAX;
    double max_x = -DBL_MAX;
    double min_y = DBL_MAX;
    double max_y = -DBL_MAX;
    for (NetPin& pin : net.ComponentPins()) {
      if (pin.ComponentPtr() == component) {
        found_component_pin = true;
        offset_x = pin.PinPtr()->OffsetX(orientation);
        offset_y = pin.PinPtr()->OffsetY(orientation);
        continue;
      }
      min_x = std::min(min_x, pin.AbsX());
      max_x = std::max(max_x, pin.AbsX());
      min_y = std::min(min_y, pin.AbsY());
      max_y = std::max(max_y, pin.AbsY());
    }

    if (!found_component_pin || min_x == DBL_MAX) {
      continue;
    }
    double weight = net.Weight();
    if (weight <= 0) {
      continue;
    }
    x_bounds.emplace_back(min_x - offset_x, weight);
    x_bounds.emplace_back(max_x - offset_x, weight);
    y_bounds.emplace_back(min_y - offset_y, weight);
    y_bounds.emplace_back(max_y - offset_y, weight);
  }

  if (x_bounds.empty() || y_bounds.empty()) {
    return {};
  }

  auto [lx, ux] = ComputeWeightedMedianInterval(std::move(x_bounds));
  auto [ly, uy] = ComputeWeightedMedianInterval(std::move(y_bounds));
  return {true, lx, ly, ux, uy};
}

GriddedDetailedPlacer::OptimalRegion
GriddedDetailedPlacer::ComputeOptimalRegion(Component* component) const {
  return ComputeOptimalRegion(component, component->Orient());
}

void GriddedDetailedPlacer::PlaceComponentInRow(GriddedRow* row,
                                                Component* component) const {
  component->SetOrient(row->IsOrientN() ? N : FS);
  double y = row->LLY();
  if (row->IsOrientN()) {
    y += row->PHeight() - component->MacroPtr()->FirstPwellHeight();
  } else {
    y += row->NHeight() - component->MacroPtr()->FirstNwellHeight();
  }
  component->SetLLY(y);
}

void GriddedDetailedPlacer::LegalizeRowsAfterAssignment(
    GriddedRow* first_row, GriddedRow* second_row) {
  for (Component* component : first_row->Components()) {
    PlaceComponentInRow(first_row, component);
  }
  for (Component* component : second_row->Components()) {
    PlaceComponentInRow(second_row, component);
  }
  first_row->LegalizeLooseX();
  second_row->LegalizeLooseX();
}

void GriddedDetailedPlacer::LegalizeRowXInCurrentOrder(GriddedRow* row) const {
  int contour = row->LLX() + row->LeftBoundaryMargin();
  for (Component* component : row->Components()) {
    component->SetLLX(std::max(contour, static_cast<int>(component->LLX())));
    contour = static_cast<int>(component->URX());
  }

  contour = row->URX() - row->RightBoundaryMargin();
  for (auto component = row->Components().rbegin();
       component != row->Components().rend(); ++component) {
    (*component)
        ->SetURX(std::min(contour, static_cast<int>((*component)->URX())));
    contour = static_cast<int>((*component)->LLX());
  }
}

void GriddedDetailedPlacer::ApplyInsertionAssignment(GriddedRow* source_row,
                                                     Component* component,
                                                     GriddedRow* target_row,
                                                     int insertion_position,
                                                     double target_lx) {
  auto component_order = [](const Component* lhs, const Component* rhs) {
    if (lhs->LLX() == rhs->LLX()) return lhs->Id() < rhs->Id();
    return lhs->LLX() < rhs->LLX();
  };

  auto& source_components = source_row->Components();
  auto source_component =
      std::find(source_components.begin(), source_components.end(), component);
  DaliExpects(source_component != source_components.end(),
              "Insertion assignment source must contain the component");
  source_components.erase(source_component);
  std::sort(source_components.begin(), source_components.end(),
            component_order);

  auto& target_components = target_row->Components();
  std::sort(target_components.begin(), target_components.end(),
            component_order);
  insertion_position = std::clamp(insertion_position, 0,
                                  static_cast<int>(target_components.size()));
  target_components.insert(target_components.begin() + insertion_position,
                           component);
  component->SetLLX(target_lx);

  for (GriddedRow* row : {source_row, target_row}) {
    for (Component* row_component : row->Components()) {
      PlaceComponentInRow(row, row_component);
    }
    LegalizeRowXInCurrentOrder(row);
  }
}

void GriddedDetailedPlacer::SynchronizeRowUsedSize(GriddedRow* row) const {
  int used_width = row->LeftBoundaryMargin() + row->RightBoundaryMargin();
  for (Component* component : row->Components()) {
    used_width += component->Width();
  }
  row->SetUsedSize(used_width);
}

void GriddedDetailedPlacer::TransferInitialLocation(
    GriddedRow* source_row, GriddedRow* target_row,
    Component* component) const {
  auto& source_locations = source_row->InitLocations();
  auto location = source_locations.find(component);
  if (location == source_locations.end()) {
    return;
  }
  target_row->InitLocations()[component] = location->second;
  source_locations.erase(location);
}

bool GriddedDetailedPlacer::TrySwap(GriddedRow* first_row, int first_index,
                                    GriddedRow* second_row, int second_index) {
  Component* first_component = first_row->Components()[first_index];
  Component* second_component = second_row->Components()[second_index];
  if (!IsSwapCandidate(first_component) || !IsSwapCandidate(second_component)) {
    return false;
  }
  if (!IsNonHeightIncreasingSwap(first_row, first_component, second_row,
                                 second_component)) {
    return false;
  }

  GriddedRowAssignmentTransaction transaction(ckt_ptr_,
                                              {first_row, second_row});
  std::swap(first_row->Components()[first_index],
            second_row->Components()[second_index]);
  LegalizeRowsAfterAssignment(first_row, second_row);

  if (!transaction.ImprovesHpwl(kMinSignificantHpwlImprovement)) {
    transaction.Restore();
    return false;
  }
  TransferInitialLocation(first_row, second_row, first_component);
  TransferInitialLocation(second_row, first_row, second_component);
  SynchronizeRowUsedSize(first_row);
  SynchronizeRowUsedSize(second_row);
  return true;
}

bool GriddedDetailedPlacer::TryMove(GriddedRow* source_row,
                                    Component* component,
                                    GriddedRow* target_row, double target_lx,
                                    MoveStats* stats, int insertion_position) {
  DaliExpects(stats != nullptr, "Relocation statistics cannot be null");
  if (source_row == target_row || !IsSwapCandidate(component)) {
    return false;
  }
  if (source_row->Components().size() <= 1) {
    ++stats->source_singleton;
    return false;
  }

  RowRequirements requirements =
      ComputeRowRequirementsAfterAssignment(target_row, nullptr, component);
  bool is_blocked = false;
  if (requirements.used_width > target_row->UsableWidth()) {
    ++stats->width_blocked;
    is_blocked = true;
  }
  if (requirements.p_well_height > target_row->PHeight()) {
    ++stats->p_well_blocked;
    is_blocked = true;
  }
  if (requirements.n_well_height > target_row->NHeight()) {
    ++stats->n_well_blocked;
    is_blocked = true;
  }
  if (is_blocked) {
    return false;
  }

  auto source_component = std::find(source_row->Components().begin(),
                                    source_row->Components().end(), component);
  if (source_component == source_row->Components().end()) {
    return false;
  }

  ++stats->evaluated;
  GriddedRowAssignmentTransaction transaction(ckt_ptr_,
                                              {source_row, target_row});
  if (insertion_position >= 0) {
    ApplyInsertionAssignment(source_row, component, target_row,
                             insertion_position, target_lx);
  } else {
    source_row->Components().erase(source_component);
    target_row->Components().push_back(component);
    component->SetLLX(target_lx);
    LegalizeRowsAfterAssignment(source_row, target_row);
  }

  if (!transaction.ImprovesHpwl(kMinSignificantHpwlImprovement)) {
    transaction.Restore();
    ++stats->no_hpwl_improvement;
    return false;
  }
  TransferInitialLocation(source_row, target_row, component);
  SynchronizeRowUsedSize(source_row);
  SynchronizeRowUsedSize(target_row);
  component_rows_[component] = target_row;
  ++stats->accepted;
  return true;
}

bool GriddedDetailedPlacer::EvaluateDirectRelocation(
    GriddedRow* source_row, Component* component, GriddedRow* target_row,
    double target_lx, RelocationPlan* plan, MoveStats* stats) {
  DaliExpects(plan != nullptr, "Relocation plan output cannot be null");
  DaliExpects(stats != nullptr, "Relocation statistics cannot be null");
  ++stats->candidates;
  if (source_row == target_row || !IsSwapCandidate(component)) return false;
  if (source_row->Components().size() <= 1) {
    ++stats->source_singleton;
    return false;
  }

  RowRequirements requirements =
      ComputeRowRequirementsAfterAssignment(target_row, nullptr, component);
  bool blocked = false;
  if (requirements.used_width > target_row->UsableWidth()) {
    ++stats->width_blocked;
    blocked = true;
  }
  if (requirements.p_well_height > target_row->PHeight()) {
    ++stats->p_well_blocked;
    blocked = true;
  }
  if (requirements.n_well_height > target_row->NHeight()) {
    ++stats->n_well_blocked;
    blocked = true;
  }
  if (blocked) return false;

  auto source_component = std::find(source_row->Components().begin(),
                                    source_row->Components().end(), component);
  if (source_component == source_row->Components().end()) return false;

  ++stats->evaluated;
  GriddedRowAssignmentTransaction transaction(ckt_ptr_,
                                              {source_row, target_row});
  source_row->Components().erase(source_component);
  target_row->Components().push_back(component);
  component->SetLLX(target_lx);
  LegalizeRowsAfterAssignment(source_row, target_row);
  double improvement = transaction.HpwlImprovement();
  transaction.Restore();
  if (improvement <= kMinSignificantHpwlImprovement) {
    ++stats->no_hpwl_improvement;
    return false;
  }
  if (improvement > plan->hpwl_improvement) {
    plan->source_row = source_row;
    plan->component = component;
    plan->target_row = target_row;
    plan->target_lx = target_lx;
    plan->insertion_position = -1;
    plan->hpwl_improvement = improvement;
  }
  return true;
}

std::vector<int> GriddedDetailedPlacer::BoundedInsertionPositions(
    Component* component, GriddedRow* target_row, double target_lx,
    const OptimalRegion& region) const {
  const auto& target_components = target_row->Components();
  const int slot_count = static_cast<int>(target_components.size());

  std::vector<double> sorted_lx;
  sorted_lx.reserve(target_components.size());
  for (Component* target_component : target_components) {
    sorted_lx.push_back(target_component->LLX());
  }
  std::sort(sorted_lx.begin(), sorted_lx.end());

  const double min_lx = target_row->LLX() + target_row->LeftBoundaryMargin();
  const double max_lx = target_row->URX() - target_row->RightBoundaryMargin() -
                        component->Width();

  // Anchor the search at X positions that plausibly minimize affected-net
  // HPWL: the clamped optimal target, the optimal-region boundaries, and each
  // low-fanout net's connected-pin X extrema mapped into component LLX space.
  std::vector<double> anchors;
  anchors.push_back(target_lx);
  if (region.valid) {
    anchors.push_back(region.lx);
    anchors.push_back(region.ux);
  }
  auto& nets = ckt_ptr_->Nets();
  const ComponentOrient orientation = component->Orient();
  for (int net_id : component->NetList()) {
    Net& net = nets[net_id];
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) {
      continue;
    }
    bool found_component_pin = false;
    double offset_x = 0;
    double min_x = DBL_MAX;
    double max_x = -DBL_MAX;
    for (NetPin& pin : net.ComponentPins()) {
      if (pin.ComponentPtr() == component) {
        found_component_pin = true;
        offset_x = pin.PinPtr()->OffsetX(orientation);
        continue;
      }
      min_x = std::min(min_x, pin.AbsX());
      max_x = std::max(max_x, pin.AbsX());
    }
    if (!found_component_pin || min_x == DBL_MAX) {
      continue;
    }
    anchors.push_back(min_x - offset_x);
    anchors.push_back(max_x - offset_x);
  }

  // Map each anchor to its natural X-order slot and widen by one slot so a
  // slightly better neighboring order is still reachable. Always include the two
  // extreme slots: prepending or appending disturbs the existing cells on only
  // one side, which is the best insertion when the target row holds cells
  // anchored by heavier nets than the moved cell's own.
  std::set<int> positions;
  positions.insert(0);
  positions.insert(slot_count);
  for (double anchor : anchors) {
    const double clamped = std::clamp(anchor, min_lx, max_lx);
    const int slot = static_cast<int>(
        std::lower_bound(sorted_lx.begin(), sorted_lx.end(), clamped) -
        sorted_lx.begin());
    for (int offset = -kInsertionSlotWindow; offset <= kInsertionSlotWindow;
         ++offset) {
      positions.insert(std::clamp(slot + offset, 0, slot_count));
    }
  }
  return {positions.begin(), positions.end()};
}

void GriddedDetailedPlacer::CollectInsertionDirtyComponents(
    Component* moved, GriddedRow* source_row, GriddedRow* target_row,
    std::unordered_set<Component*>* dirty) const {
  dirty->insert(moved);
  // Components sharing a low-fanout net: the move changed those nets' spans, so
  // the connected cells' optimal regions and insertion costs may have changed.
  auto& nets = ckt_ptr_->Nets();
  for (int net_id : moved->NetList()) {
    Net& net = nets[net_id];
    if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) {
      continue;
    }
    for (NetPin& pin : net.ComponentPins()) {
      Component* neighbor = pin.ComponentPtr();
      if (neighbor != nullptr) {
        dirty->insert(neighbor);
      }
    }
  }
  // Both rows are repacked in fixed order after the move, so every cell in them
  // shifts and must be re-looked.
  for (GriddedRow* row : {source_row, target_row}) {
    for (Component* row_component : row->Components()) {
      dirty->insert(row_component);
    }
  }
}

bool GriddedDetailedPlacer::EvaluateInsertionRelocations(
    GriddedRow* source_row, Component* component, GriddedRow* target_row,
    double target_lx, const OptimalRegion& region, RelocationPlan* plan,
    MoveStats* stats) {
  DaliExpects(plan != nullptr, "Relocation plan output cannot be null");
  DaliExpects(stats != nullptr, "Relocation statistics cannot be null");

  ++stats->candidates;
  if (source_row == target_row || !IsSwapCandidate(component)) return false;
  if (source_row->Components().size() <= 1) {
    ++stats->source_singleton;
    return false;
  }

  RowRequirements requirements =
      ComputeRowRequirementsAfterAssignment(target_row, nullptr, component);
  bool blocked = false;
  if (requirements.used_width > target_row->UsableWidth()) {
    ++stats->width_blocked;
    blocked = true;
  }
  if (requirements.p_well_height > target_row->PHeight()) {
    ++stats->p_well_blocked;
    blocked = true;
  }
  if (requirements.n_well_height > target_row->NHeight()) {
    ++stats->n_well_blocked;
    blocked = true;
  }
  if (blocked) return false;

  ++stats->evaluated;
  std::vector<int> insertion_positions;
  if (exhaustive_insertion_positions_) {
    const int insertion_count =
        static_cast<int>(target_row->Components().size()) + 1;
    insertion_positions.reserve(insertion_count);
    for (int position = 0; position < insertion_count; ++position) {
      insertion_positions.push_back(position);
    }
  } else {
    insertion_positions =
        BoundedInsertionPositions(component, target_row, target_lx, region);
  }

  bool found = false;
  GriddedRowAssignmentTransaction transaction(ckt_ptr_,
                                              {source_row, target_row});
  for (int insertion_position : insertion_positions) {
    ApplyInsertionAssignment(source_row, component, target_row,
                             insertion_position, target_lx);
    ++stats->insertion_positions_evaluated;
    double improvement = transaction.HpwlImprovement();
    transaction.Restore();
    if (improvement <= kMinSignificantHpwlImprovement) continue;
    found = true;
    if (improvement > plan->hpwl_improvement) {
      plan->source_row = source_row;
      plan->component = component;
      plan->target_row = target_row;
      plan->target_lx = target_lx;
      plan->insertion_position = insertion_position;
      plan->hpwl_improvement = improvement;
    }
  }
  if (!found) ++stats->no_hpwl_improvement;
  return found;
}

bool GriddedDetailedPlacer::FindBestDirectRelocation(GriddedRow* source_row,
                                                     Component* component,
                                                     RelocationPlan* plan,
                                                     MoveStats* stats) {
  DaliExpects(plan != nullptr, "Relocation plan output cannot be null");
  OptimalRegion region = ComputeOptimalRegion(component);
  if (!region.valid) return false;

  bool found = false;
  for (const CandidateRow& candidate_row :
       FindCandidateRows(source_row, component, region)) {
    double target_lx =
        ComputeMoveTargetX(candidate_row.row, component, candidate_row.region);
    found = EvaluateDirectRelocation(source_row, component, candidate_row.row,
                                     target_lx, plan, stats) ||
            found;
  }
  return found;
}

bool GriddedDetailedPlacer::FindBestInsertionRelocation(GriddedRow* source_row,
                                                        Component* component,
                                                        RelocationPlan* plan,
                                                        MoveStats* stats) {
  OptimalRegion region = ComputeOptimalRegion(component);
  if (!region.valid) return false;

  bool found = false;
  for (const CandidateRow& candidate_row :
       FindCandidateRows(source_row, component, region)) {
    double target_lx =
        ComputeMoveTargetX(candidate_row.row, component, candidate_row.region);
    found = EvaluateInsertionRelocations(source_row, component,
                                         candidate_row.row, target_lx,
                                         candidate_row.region, plan, stats) ||
            found;
  }
  return found;
}

bool GriddedDetailedPlacer::TryEjectionChain(GriddedRow* source_row,
                                             Component* component,
                                             GriddedRow* target_row,
                                             const OptimalRegion& source_region,
                                             MoveStats* stats) {
  DaliExpects(stats != nullptr, "Relocation statistics cannot be null");
  ++stats->ejection_attempts;
  if (source_row->Components().size() <= 1) {
    return false;
  }

  for (const DisplacementCandidate& ejection :
       FindDisplacementCandidates(target_row, component)) {
    for (const CandidateRow& receiver : FindEjectionDestinationRows(
             source_row, target_row, ejection.component)) {
      ++stats->ejection_evaluated;
      GriddedRowAssignmentTransaction transaction(
          ckt_ptr_, {source_row, target_row, receiver.row});

      auto source_component =
          std::find(source_row->Components().begin(),
                    source_row->Components().end(), component);
      auto displaced_component =
          std::find(target_row->Components().begin(),
                    target_row->Components().end(), ejection.component);
      if (source_component == source_row->Components().end() ||
          displaced_component == target_row->Components().end()) {
        return false;
      }
      source_row->Components().erase(source_component);
      target_row->Components().erase(displaced_component);
      target_row->Components().push_back(component);
      receiver.row->Components().push_back(ejection.component);
      component->SetLLX(
          ComputeMoveTargetX(target_row, component, source_region));
      ejection.component->SetLLX(ComputeMoveTargetX(
          receiver.row, ejection.component, receiver.region));
      LegalizeRowsAfterAssignment(source_row, target_row);
      for (Component* receiver_component : receiver.row->Components()) {
        PlaceComponentInRow(receiver.row, receiver_component);
      }
      receiver.row->LegalizeLooseX();

      if (!transaction.ImprovesHpwl(kMinSignificantHpwlImprovement)) {
        transaction.Restore();
        ++stats->ejection_no_hpwl_improvement;
        continue;
      }

      TransferInitialLocation(source_row, target_row, component);
      TransferInitialLocation(target_row, receiver.row, ejection.component);
      SynchronizeRowUsedSize(source_row);
      SynchronizeRowUsedSize(target_row);
      SynchronizeRowUsedSize(receiver.row);
      component_rows_[component] = target_row;
      component_rows_[ejection.component] = receiver.row;
      ++stats->ejection_accepted;
      return true;
    }
  }
  return false;
}

bool GriddedDetailedPlacer::TryClosedAssignmentCycle(
    GriddedRow* source_row, Component* component, GriddedRow* target_row,
    const OptimalRegion& source_region, MoveStats* stats) {
  ClosedCycleCandidate best_candidate = FindBestClosedAssignmentCycle(
      source_row, component, target_row, source_region, stats);
  if (best_candidate.displaced_component == nullptr) {
    return false;
  }
  return CommitClosedAssignmentCycle(source_row, component, target_row,
                                     source_region, best_candidate, stats);
}

GriddedDetailedPlacer::ClosedCycleCandidate
GriddedDetailedPlacer::FindBestClosedAssignmentCycle(
    GriddedRow* source_row, Component* component, GriddedRow* target_row,
    const OptimalRegion& source_region, MoveStats* stats) {
  DaliExpects(stats != nullptr, "Assignment-cycle statistics cannot be null");
  ++stats->cycle_attempts;
  if (source_row->Components().size() <= 1) {
    return {};
  }

  struct ReturnCandidate {
    Component* component = nullptr;
    OptimalRegion region;
    double score = 0;
  };

  ClosedCycleCandidate best_candidate;
  for (const DisplacementCandidate& displacement :
       FindDisplacementCandidates(target_row, component)) {
    int receiver_count = 0;
    for (const CandidateRow& receiver : FindCandidateRows(
             target_row, displacement.component, displacement.region)) {
      if (receiver.row == source_row || receiver.row == target_row) {
        continue;
      }
      if (receiver_count++ >= kMaxCycleReceiverRows) {
        break;
      }

      std::vector<ReturnCandidate> return_candidates;
      for (Component* returning : receiver.row->Components()) {
        if (!IsSwapCandidate(returning)) {
          continue;
        }
        RowRequirements receiver_requirements =
            ComputeRowRequirementsAfterAssignment(receiver.row, returning,
                                                  displacement.component);
        RowRequirements source_requirements =
            ComputeRowRequirementsAfterAssignment(source_row, component,
                                                  returning);
        if (receiver_requirements.used_width > receiver.row->UsableWidth() ||
            receiver_requirements.p_well_height > receiver.row->PHeight() ||
            receiver_requirements.n_well_height > receiver.row->NHeight() ||
            source_requirements.used_width > source_row->UsableWidth() ||
            source_requirements.p_well_height > source_row->PHeight() ||
            source_requirements.n_well_height > source_row->NHeight()) {
          continue;
        }

        OptimalRegion returning_region =
            ComputeOptimalRegion(returning, source_row->IsOrientN() ? N : FS);
        double return_distance =
            std::fabs(returning->CenterX() - source_row->CenterX()) *
                ckt_ptr_->GridValueX() +
            std::fabs(returning->CenterY() - source_row->CenterY()) *
                ckt_ptr_->GridValueY();
        if (returning_region.valid) {
          return_distance = PhysicalDistanceFromRowToOptimalRegion(
              source_row, returning, returning_region);
        }
        return_candidates.push_back(
            {returning, returning_region, receiver.distance + return_distance});
      }
      std::sort(return_candidates.begin(), return_candidates.end(),
                [](const ReturnCandidate& lhs, const ReturnCandidate& rhs) {
                  return lhs.score < rhs.score;
                });
      if (return_candidates.size() > kMaxCycleComponentsPerReceiver) {
        return_candidates.resize(kMaxCycleComponentsPerReceiver);
      }

      for (const ReturnCandidate& return_candidate : return_candidates) {
        ++stats->cycle_evaluated;
        GriddedRowAssignmentTransaction transaction(
            ckt_ptr_, {source_row, target_row, receiver.row});
        ClosedCycleCandidate candidate{
            displacement.component,     receiver.region,         receiver.row,
            return_candidate.component, return_candidate.region, 0};
        if (!ApplyClosedAssignmentCycle(source_row, component, target_row,
                                        source_region, candidate)) {
          transaction.Restore();
          continue;
        }
        candidate.hpwl_improvement = transaction.HpwlImprovement();
        transaction.Restore();
        if (candidate.hpwl_improvement <= kMinSignificantHpwlImprovement) {
          ++stats->cycle_no_hpwl_improvement;
          continue;
        }
        if (candidate.hpwl_improvement > best_candidate.hpwl_improvement) {
          best_candidate = candidate;
        }
      }
    }
  }

  return best_candidate;
}

bool GriddedDetailedPlacer::CommitClosedAssignmentCycle(
    GriddedRow* source_row, Component* component, GriddedRow* target_row,
    const OptimalRegion& source_region, const ClosedCycleCandidate& candidate,
    MoveStats* stats) {
  DaliExpects(stats != nullptr, "Assignment-cycle statistics cannot be null");
  GriddedRowAssignmentTransaction transaction(
      ckt_ptr_, {source_row, target_row, candidate.receiver_row});
  if (!ApplyClosedAssignmentCycle(source_row, component, target_row,
                                  source_region, candidate) ||
      !transaction.ImprovesHpwl(kMinSignificantHpwlImprovement)) {
    transaction.Restore();
    return false;
  }
  TransferInitialLocation(source_row, target_row, component);
  TransferInitialLocation(target_row, candidate.receiver_row,
                          candidate.displaced_component);
  TransferInitialLocation(candidate.receiver_row, source_row,
                          candidate.returning_component);
  SynchronizeRowUsedSize(source_row);
  SynchronizeRowUsedSize(target_row);
  SynchronizeRowUsedSize(candidate.receiver_row);
  component_rows_[component] = target_row;
  component_rows_[candidate.displaced_component] = candidate.receiver_row;
  component_rows_[candidate.returning_component] = source_row;
  ++stats->cycle_accepted;
  return true;
}

bool GriddedDetailedPlacer::ApplyClosedAssignmentCycle(
    GriddedRow* source_row, Component* component, GriddedRow* target_row,
    const OptimalRegion& source_region, const ClosedCycleCandidate& candidate) {
  auto source_component = std::find(source_row->Components().begin(),
                                    source_row->Components().end(), component);
  auto target_component =
      std::find(target_row->Components().begin(),
                target_row->Components().end(), candidate.displaced_component);
  auto receiver_component =
      std::find(candidate.receiver_row->Components().begin(),
                candidate.receiver_row->Components().end(),
                candidate.returning_component);
  if (source_component == source_row->Components().end() ||
      target_component == target_row->Components().end() ||
      receiver_component == candidate.receiver_row->Components().end()) {
    return false;
  }

  RowRequirements source_requirements = ComputeRowRequirementsAfterAssignment(
      source_row, component, candidate.returning_component);
  RowRequirements target_requirements = ComputeRowRequirementsAfterAssignment(
      target_row, candidate.displaced_component, component);
  RowRequirements receiver_requirements = ComputeRowRequirementsAfterAssignment(
      candidate.receiver_row, candidate.returning_component,
      candidate.displaced_component);
  if (source_requirements.used_width > source_row->UsableWidth() ||
      source_requirements.p_well_height > source_row->PHeight() ||
      source_requirements.n_well_height > source_row->NHeight() ||
      target_requirements.used_width > target_row->UsableWidth() ||
      target_requirements.p_well_height > target_row->PHeight() ||
      target_requirements.n_well_height > target_row->NHeight() ||
      receiver_requirements.used_width >
          candidate.receiver_row->UsableWidth() ||
      receiver_requirements.p_well_height > candidate.receiver_row->PHeight() ||
      receiver_requirements.n_well_height > candidate.receiver_row->NHeight()) {
    return false;
  }

  source_row->Components().erase(source_component);
  target_row->Components().erase(target_component);
  candidate.receiver_row->Components().erase(receiver_component);
  target_row->Components().push_back(component);
  candidate.receiver_row->Components().push_back(candidate.displaced_component);
  source_row->Components().push_back(candidate.returning_component);
  component->SetLLX(ComputeMoveTargetX(target_row, component, source_region));
  candidate.displaced_component->SetLLX(
      ComputeMoveTargetX(candidate.receiver_row, candidate.displaced_component,
                         candidate.displaced_region));
  if (candidate.returning_region.valid) {
    candidate.returning_component->SetLLX(ComputeMoveTargetX(
        source_row, candidate.returning_component, candidate.returning_region));
  }
  for (GriddedRow* row : {source_row, target_row, candidate.receiver_row}) {
    for (Component* row_component : row->Components()) {
      PlaceComponentInRow(row, row_component);
    }
    row->LegalizeLooseX();
  }
  return true;
}

GriddedDetailedPlacer::SwapStats
GriddedDetailedPlacer::TryClosestComponentSwaps(GriddedRow* first_row,
                                                GriddedRow* second_row,
                                                int max_candidates) {
  struct CandidatePair {
    int first_index = -1;
    int second_index = -1;
    double center_distance = 0;
  };

  std::vector<std::pair<double, int>> second_row_centers;
  second_row_centers.reserve(second_row->Components().size());
  for (int i = 0; i < static_cast<int>(second_row->Components().size()); ++i) {
    Component* component = second_row->Components()[i];
    if (IsSwapCandidate(component)) {
      second_row_centers.emplace_back(component->CenterX(), i);
    }
  }
  std::sort(second_row_centers.begin(), second_row_centers.end());

  std::vector<CandidatePair> candidate_pairs;
  for (int i = 0; i < static_cast<int>(first_row->Components().size()); ++i) {
    Component* first_component = first_row->Components()[i];
    if (!IsSwapCandidate(first_component)) {
      continue;
    }

    auto lower =
        std::lower_bound(second_row_centers.begin(), second_row_centers.end(),
                         std::make_pair(first_component->CenterX(), -1));
    if (lower != second_row_centers.end()) {
      candidate_pairs.push_back(
          {i, lower->second,
           std::fabs(first_component->CenterX() - lower->first)});
    }
    if (lower != second_row_centers.begin()) {
      --lower;
      candidate_pairs.push_back(
          {i, lower->second,
           std::fabs(first_component->CenterX() - lower->first)});
    }
  }

  std::sort(candidate_pairs.begin(), candidate_pairs.end(),
            [](const CandidatePair& lhs, const CandidatePair& rhs) {
              return lhs.center_distance < rhs.center_distance;
            });

  SwapStats stats;
  int candidate_limit =
      std::min(max_candidates, static_cast<int>(candidate_pairs.size()));
  for (int i = 0; i < candidate_limit; ++i) {
    ++stats.candidates;
    if (TrySwap(first_row, candidate_pairs[i].first_index, second_row,
                candidate_pairs[i].second_index)) {
      ++stats.accepted;
    }
  }
  return stats;
}

GriddedDetailedPlacer::SwapStats GriddedDetailedPlacer::TryOptimalRegionSwaps(
    GriddedRow* source_row, int source_index) {
  struct CandidateComponent {
    int index = -1;
    double distance = 0;
  };

  Component* source_component = source_row->Components()[source_index];
  if (!IsSwapCandidate(source_component)) {
    return {};
  }

  OptimalRegion region = ComputeOptimalRegion(source_component);
  if (!region.valid) {
    return {};
  }
  std::vector<CandidateRow> candidate_rows =
      FindCandidateRows(source_row, source_component, region);

  SwapStats stats;
  for (const CandidateRow& candidate_row : candidate_rows) {
    GriddedRow* target_row = candidate_row.row;
    std::vector<CandidateComponent> target_components;
    target_components.reserve(target_row->Components().size());
    for (int target_index = 0;
         target_index < static_cast<int>(target_row->Components().size());
         ++target_index) {
      Component* target_component = target_row->Components()[target_index];
      if (!IsSwapCandidate(target_component)) {
        continue;
      }
      target_components.push_back(
          {target_index,
           DistanceToOptimalRegionX(target_component, candidate_row.region)});
    }
    std::sort(target_components.begin(), target_components.end(),
              [](const CandidateComponent& lhs, const CandidateComponent& rhs) {
                return lhs.distance < rhs.distance;
              });

    int component_limit = std::min(kMaxOptimalRegionCandidatesPerRow,
                                   static_cast<int>(target_components.size()));
    for (int candidate_id = 0; candidate_id < component_limit; ++candidate_id) {
      ++stats.candidates;
      if (TrySwap(source_row, source_index, target_row,
                  target_components[candidate_id].index)) {
        ++stats.accepted;
        return stats;
      }
    }
  }
  return stats;
}

GriddedDetailedPlacer::MoveStats GriddedDetailedPlacer::TryOptimalRegionMove(
    GriddedRow* source_row, Component* component, bool enable_ejection,
    std::vector<Component*>* deferred_cycle_components) {
  if (!IsSwapCandidate(component)) {
    return {};
  }
  OptimalRegion region = ComputeOptimalRegion(component);
  if (!region.valid) {
    return {};
  }

  MoveStats stats;
  GriddedRow* ejection_target = nullptr;
  OptimalRegion ejection_target_region;
  for (const CandidateRow& candidate_row :
       FindCandidateRows(source_row, component, region)) {
    ++stats.candidates;
    double target_lx =
        ComputeMoveTargetX(candidate_row.row, component, candidate_row.region);
    int width_blocked_before = stats.width_blocked;
    if (TryMove(source_row, component, candidate_row.row, target_lx, &stats)) {
      return stats;
    }
    if (ejection_target == nullptr &&
        stats.width_blocked > width_blocked_before) {
      ejection_target = candidate_row.row;
      ejection_target_region = candidate_row.region;
    }
  }
  if (enable_ejection && ejection_target != nullptr) {
    if (!TryEjectionChain(source_row, component, ejection_target,
                          ejection_target_region, &stats)) {
      if (deferred_cycle_components == nullptr) {
        TryClosedAssignmentCycle(source_row, component, ejection_target,
                                 ejection_target_region, &stats);
      } else {
        deferred_cycle_components->push_back(component);
      }
    }
  }
  return stats;
}

GriddedDetailedPlacer::MoveStats
GriddedDetailedPlacer::RunBatchedAssignmentCycles(
    const std::vector<Component*>& deferred_components) {
  MoveStats stats;
  std::vector<Component*> pending_components = deferred_components;
  for (int pass = 0;
       pass < kMaxAssignmentBatchPasses && !pending_components.empty();
       ++pass) {
    std::vector<ClosedCyclePlan> plans;
    plans.reserve(pending_components.size());
    for (Component* component : pending_components) {
      auto source = component_rows_.find(component);
      if (source == component_rows_.end()) continue;
      GriddedRow* source_row = source->second;
      if (std::find(source_row->Components().begin(),
                    source_row->Components().end(),
                    component) == source_row->Components().end()) {
        continue;
      }

      OptimalRegion source_region = ComputeOptimalRegion(component);
      if (!source_region.valid) continue;
      GriddedRow* target_row = nullptr;
      for (const CandidateRow& candidate_row :
           FindCandidateRows(source_row, component, source_region)) {
        RowRequirements requirements = ComputeRowRequirementsAfterAssignment(
            candidate_row.row, nullptr, component);
        if (requirements.used_width > candidate_row.row->UsableWidth()) {
          target_row = candidate_row.row;
          source_region = candidate_row.region;
          break;
        }
      }
      if (target_row == nullptr) continue;

      ClosedCycleCandidate candidate = FindBestClosedAssignmentCycle(
          source_row, component, target_row, source_region, &stats);
      if (candidate.displaced_component != nullptr) {
        plans.push_back(
            {source_row, component, target_row, source_region, candidate});
      }
    }

    std::sort(plans.begin(), plans.end(),
              [](const ClosedCyclePlan& lhs, const ClosedCyclePlan& rhs) {
                if (lhs.candidate.hpwl_improvement !=
                    rhs.candidate.hpwl_improvement) {
                  return lhs.candidate.hpwl_improvement >
                         rhs.candidate.hpwl_improvement;
                }
                return lhs.component->Id() < rhs.component->Id();
              });
    std::vector<Component*> invalidated_components;
    invalidated_components.reserve(plans.size());
    for (const ClosedCyclePlan& plan : plans) {
      if (!CommitClosedAssignmentCycle(plan.source_row, plan.component,
                                       plan.target_row, plan.source_region,
                                       plan.candidate, &stats)) {
        ++stats.cycle_invalidated;
        invalidated_components.push_back(plan.component);
      }
    }
    pending_components = std::move(invalidated_components);
  }
  return stats;
}

GriddedDetailedPlacer::MoveStats
GriddedDetailedPlacer::RunBatchedRelocationStage(bool insertion_aware) {
  MoveStats total_stats;
  component_rows_.clear();
  for (GriddedRow* row : rows_) {
    for (Component* component : row->Components()) {
      component_rows_[component] = row;
    }
  }

  const int max_passes = insertion_aware ? kMaxInsertionRefinementPasses
                                         : kMaxBatchedRelocationPasses;
  // Don't-look bits for the insertion pass: a component whose scan finds no
  // improving move is skipped until a committed move touches its neighborhood.
  // This removes the redundant full re-scan that dominated insertion-refinement
  // cost (see the batch cost attribution experiment).
  std::unordered_set<Component*> insertion_dont_look;
  // The don't-look dependency set is conservative but not complete (a component
  // that only targets a row whose contents changed is not re-looked). Once the
  // incremental passes converge, run one final full sweep with all bits cleared
  // to recover those missed moves before stopping.
  bool insertion_verification_done = false;
  for (int pass = 0; pass < max_passes; ++pass) {
    double hpwl_before_pass = insertion_aware ? WeightedHPWL() : 0;
    std::vector<Component*> components;
    components.reserve(component_rows_.size());
    for (GriddedRow* row : rows_) {
      components.insert(components.end(), row->Components().begin(),
                        row->Components().end());
    }
    std::sort(components.begin(), components.end(),
              [](const Component* lhs, const Component* rhs) {
                return lhs->Id() < rhs->Id();
              });

    std::vector<RelocationPlan> plans;
    plans.reserve(components.size());
    for (Component* component : components) {
      if (insertion_aware && insertion_dont_look.count(component) > 0) {
        continue;
      }
      RelocationPlan plan;
      GriddedRow* source_row = component_rows_.at(component);
      bool found = insertion_aware
                       ? FindBestInsertionRelocation(source_row, component,
                                                     &plan, &total_stats)
                       : FindBestDirectRelocation(source_row, component, &plan,
                                                  &total_stats);
      if (found) {
        plans.push_back(plan);
      } else if (insertion_aware) {
        insertion_dont_look.insert(component);
      }
    }
    if (plans.empty()) {
      if (insertion_aware && !insertion_verification_done &&
          !insertion_dont_look.empty()) {
        insertion_dont_look.clear();
        insertion_verification_done = true;
        continue;
      }
      break;
    }
    ++total_stats.batch_passes;
    total_stats.batch_plans += static_cast<int>(plans.size());
    std::sort(plans.begin(), plans.end(),
              [](const RelocationPlan& lhs, const RelocationPlan& rhs) {
                if (lhs.hpwl_improvement != rhs.hpwl_improvement) {
                  return lhs.hpwl_improvement > rhs.hpwl_improvement;
                }
                return lhs.component->Id() < rhs.component->Id();
              });

    total_stats.batch_selected += static_cast<int>(plans.size());

    int accepted_this_pass = 0;
    std::unordered_set<Component*> insertion_dirty;
    for (const RelocationPlan& plan : plans) {
      auto current_row = component_rows_.find(plan.component);
      if (current_row == component_rows_.end() ||
          current_row->second != plan.source_row) {
        continue;
      }
      MoveStats commit_stats;
      if (TryMove(plan.source_row, plan.component, plan.target_row,
                  plan.target_lx, &commit_stats, plan.insertion_position)) {
        ++accepted_this_pass;
        ++total_stats.accepted;
        ++total_stats.batch_accepted;
        if (insertion_aware) {
          CollectInsertionDirtyComponents(plan.component, plan.source_row,
                                          plan.target_row, &insertion_dirty);
        }
      }
    }
    if (accepted_this_pass == 0) break;
    // Re-look at every component whose neighborhood a committed move disturbed.
    for (Component* component : insertion_dirty) {
      insertion_dont_look.erase(component);
    }
    if (insertion_aware) {
      double hpwl_after_pass = WeightedHPWL();
      double relative_improvement =
          hpwl_before_pass > 0
              ? (hpwl_before_pass - hpwl_after_pass) / hpwl_before_pass
              : 0;
      LOG(info) << "  insertion refinement pass " << pass
                << ": accepted=" << accepted_this_pass
                << ", HPWL=" << hpwl_after_pass
                << "um, improvement=" << hpwl_before_pass - hpwl_after_pass
                << "um (" << relative_improvement * 100.0 << "%)\n";
      if (relative_improvement < kMinInsertionRefinementRelativeImprovement) {
        if (!insertion_verification_done && !insertion_dont_look.empty()) {
          insertion_dont_look.clear();
          insertion_verification_done = true;
          continue;
        }
        break;
      }
    }
  }
  component_rows_.clear();
  return total_stats;
}

void GriddedDetailedPlacer::RunSafePairMerge() {
  component_rows_.clear();
  for (GriddedRow* row : rows_) {
    for (Component* component : row->Components()) {
      component_rows_[component] = row;
    }
  }

  std::vector<Component*> movers;
  for (GriddedRow* row : rows_) {
    movers.insert(movers.end(), row->Components().begin(),
                  row->Components().end());
  }
  std::sort(movers.begin(), movers.end(),
            [](const Component* lhs, const Component* rhs) {
              return lhs->Id() < rhs->Id();
            });

  std::vector<Net>& nets = ckt_ptr_->Nets();
  int attempted = 0;
  int merged = 0;
  for (Component* mover : movers) {
    if (mover->NetList().size() > 3) continue;  // low-degree only
    auto mover_row = component_rows_.find(mover);
    if (mover_row == component_rows_.end()) continue;
    for (int net_id : mover->NetList()) {
      Net& net = nets[net_id];
      if (net.PinCnt() != 2) continue;
      Component* partner = nullptr;
      for (NetPin& pin : net.ComponentPins()) {
        if (pin.ComponentPtr() != nullptr && pin.ComponentPtr() != mover) {
          partner = pin.ComponentPtr();
          break;
        }
      }
      if (partner == nullptr || !partner->IsMovable()) continue;
      if (partner->NetList().size() > 3) continue;
      Macro* mover_macro = mover->MacroPtr();
      Macro* partner_macro = partner->MacroPtr();
      if (mover_macro->FirstPwellHeight() != partner_macro->FirstPwellHeight() ||
          mover_macro->FirstNwellHeight() != partner_macro->FirstNwellHeight()) {
        continue;
      }
      auto partner_row = component_rows_.find(partner);
      if (partner_row == component_rows_.end()) continue;
      GriddedRow* source_row = mover_row->second;
      GriddedRow* target_row = partner_row->second;
      if (source_row == target_row) continue;  // already co-located

      // Insert the mover immediately after its partner in the target row's X
      // order; TryMove repacks the row and keeps the move only if it is legal
      // and improves affected-net HPWL.
      int insertion_position = 0;
      for (Component* target_component : target_row->Components()) {
        if (target_component->LLX() < partner->LLX() ||
            (target_component->LLX() == partner->LLX() &&
             target_component->Id() <= partner->Id())) {
          ++insertion_position;
        }
      }
      const double target_lx = partner->LLX() + partner->Width();
      ++attempted;
      MoveStats commit_stats;
      if (TryMove(source_row, mover, target_row, target_lx, &commit_stats,
                  insertion_position)) {
        ++merged;
        break;  // mover is placed; move on to the next mover
      }
    }
  }
  LOG(info) << "  safe-pair merge: attempted=" << attempted
            << ", merged=" << merged << "\n";
  component_rows_.clear();
}

GriddedDetailedPlacer::MoveStats GriddedDetailedPlacer::RunRelocationStage(
    bool enable_ejection) {
  MoveStats total_stats;
  if (enable_relocation_ && enable_batched_assignment_moves_) {
    ElapsedTime batch_timer;
    batch_timer.RecordStartTime();
    total_stats.Add(RunBatchedRelocationStage());
    batch_timer.RecordEndTime();
    batch_relocation_wall_s_ += batch_timer.GetWallTime();
  }
  std::vector<Component*> components;
  component_rows_.clear();
  for (GriddedRow* row : rows_) {
    for (Component* component : row->Components()) {
      components.push_back(component);
      component_rows_[component] = row;
    }
  }
  std::sort(components.begin(), components.end(),
            [](const Component* lhs, const Component* rhs) {
              return lhs->Id() < rhs->Id();
            });
  std::vector<Component*> deferred_cycle_components;
  std::vector<Component*>* deferred_cycles =
      enable_ejection && enable_batched_assignment_moves_
          ? &deferred_cycle_components
          : nullptr;
  ElapsedTime sequential_timer;
  sequential_timer.RecordStartTime();
  for (Component* component : components) {
    total_stats.Add(TryOptimalRegionMove(component_rows_.at(component),
                                         component, enable_ejection,
                                         deferred_cycles));
  }
  sequential_timer.RecordEndTime();
  sequential_relocation_wall_s_ += sequential_timer.GetWallTime();
  if (!deferred_cycle_components.empty()) {
    ElapsedTime cycle_timer;
    cycle_timer.RecordStartTime();
    total_stats.Add(RunBatchedAssignmentCycles(deferred_cycle_components));
    cycle_timer.RecordEndTime();
    assignment_cycle_wall_s_ += cycle_timer.GetWallTime();
  }
  component_rows_.clear();
  return total_stats;
}

bool GriddedDetailedPlacer::ClusterRowX(GriddedRow* row, bool* changed) {
  DaliExpects(changed != nullptr,
              "Gridded row clustering requires a changed-row output");
  *changed = false;
  auto& components = row->Components();
  if (components.empty()) {
    return false;
  }
  std::sort(components.begin(), components.end(),
            [](const Component* lhs, const Component* rhs) {
              if (lhs->LLX() == rhs->LLX()) {
                return lhs->Id() < rhs->Id();
              }
              return lhs->LLX() < rhs->LLX();
            });

  int total_width = 0;
  std::vector<double> original_lx;
  std::vector<double> transformed_targets;
  std::vector<double> weights;
  original_lx.reserve(components.size());
  transformed_targets.reserve(components.size());
  weights.reserve(components.size());
  std::vector<Net>& nets = ckt_ptr_->Nets();
  for (Component* component : components) {
    original_lx.push_back(component->LLX());
    OptimalRegion region = ComputeOptimalRegion(component);
    double target_lx = component->LLX();
    if (region.valid) {
      target_lx = std::clamp(component->LLX(), region.lx, region.ux);
    }
    transformed_targets.push_back(target_lx - total_width);
    total_width += component->Width();

    // Weight each cell by its incident low-fanout net weight so a merged block's
    // legal position follows its highly connected cells. Default (unweighted)
    // mode keeps every weight at 1.
    double weight = 1.0;
    if (weighted_clustering_) {
      weight = 0.0;
      for (int net_id : component->NetList()) {
        Net& net = nets[net_id];
        if (net.PinCnt() <= 1 || net.PinCnt() >= net_ignore_threshold_) {
          continue;
        }
        weight += net.Weight();
      }
      if (weight <= 0) weight = 1.0;
    }
    weights.push_back(weight);
  }

  int min_transformed_lx = row->LLX() + row->LeftBoundaryMargin();
  int max_transformed_lx =
      row->URX() - row->RightBoundaryMargin() - total_width;
  DaliExpects(min_transformed_lx <= max_transformed_lx,
              "Cannot cluster an overflowing gridded row");

  struct IsotonicBlock {
    int begin = 0;
    int end = 0;
    double weighted_sum = 0;  // sum of weight * target
    double total_weight = 0;

    double Mean() const { return weighted_sum / total_weight; }
  };
  std::vector<IsotonicBlock> blocks;
  blocks.reserve(components.size());
  for (int i = 0; i < static_cast<int>(components.size()); ++i) {
    blocks.push_back(
        {i, i + 1, weights[i] * transformed_targets[i], weights[i]});
    while (blocks.size() >= 2 &&
           blocks[blocks.size() - 2].Mean() > blocks.back().Mean()) {
      IsotonicBlock right = blocks.back();
      blocks.pop_back();
      blocks.back().end = right.end;
      blocks.back().weighted_sum += right.weighted_sum;
      blocks.back().total_weight += right.total_weight;
    }
  }

  std::vector<int> transformed_lx(components.size(), 0);
  for (const IsotonicBlock& block : blocks) {
    int legal_lx = static_cast<int>(std::llround(block.Mean()));
    legal_lx = std::clamp(legal_lx, min_transformed_lx, max_transformed_lx);
    for (int i = block.begin; i < block.end; ++i) {
      transformed_lx[i] = legal_lx;
    }
  }

  std::vector<int> affected_net_ids = CollectRowNetIds({row});
  double cost_before = NetWireLengthCost(affected_net_ids);
  int prefix_width = 0;
  for (size_t i = 0; i < components.size(); ++i) {
    double legal_lx = transformed_lx[i] + prefix_width;
    *changed = *changed || legal_lx != original_lx[i];
    components[i]->SetLLX(legal_lx);
    prefix_width += components[i]->Width();
  }
  if (!*changed) {
    return false;
  }

  if (NetWireLengthCost(affected_net_ids) + kMinSignificantHpwlImprovement <
      cost_before) {
    return true;
  }
  for (size_t i = 0; i < components.size(); ++i) {
    components[i]->SetLLX(original_lx[i]);
  }
  return false;
}

GriddedDetailedPlacer::ClusterStats
GriddedDetailedPlacer::RunSingleSegmentClustering() {
  ClusterStats stats;
  for (GriddedRow* row : rows_) {
    if (row->Components().empty()) {
      continue;
    }
    ++stats.visited_rows;
    bool changed = false;
    bool accepted = ClusterRowX(row, &changed);
    stats.changed_rows += changed;
    stats.accepted_rows += accepted;
  }
  return stats;
}

void GriddedDetailedPlacer::LogClusteringPass(const std::string& stage_name,
                                              const ClusterStats& stats,
                                              double hpwl_before) {
  double hpwl_after = WeightedHPWL();
  LOG(info) << "  " << stage_name << ": visited=" << stats.visited_rows
            << ", changed=" << stats.changed_rows
            << ", accepted=" << stats.accepted_rows << ", HPWL=" << hpwl_after
            << "um, improvement=" << hpwl_before - hpwl_after << "um\n";
}

void GriddedDetailedPlacer::LogMoveStage(const MoveStats& stats,
                                         double hpwl_before) {
  double hpwl_after = WeightedHPWL();
  LOG(info) << "  row relocation: candidates=" << stats.candidates
            << ", evaluated=" << stats.evaluated
            << ", accepted=" << stats.accepted
            << ", blockers(source=" << stats.source_singleton
            << ", width=" << stats.width_blocked
            << ", p-well=" << stats.p_well_blocked
            << ", n-well=" << stats.n_well_blocked
            << "), no-HPWL-gain=" << stats.no_hpwl_improvement
            << ", ejection(attempted=" << stats.ejection_attempts
            << ", evaluated=" << stats.ejection_evaluated
            << ", accepted=" << stats.ejection_accepted
            << ", no-HPWL-gain=" << stats.ejection_no_hpwl_improvement << ")"
            << ", cycle(attempted=" << stats.cycle_attempts
            << ", evaluated=" << stats.cycle_evaluated
            << ", accepted=" << stats.cycle_accepted
            << ", invalidated=" << stats.cycle_invalidated
            << ", no-HPWL-gain=" << stats.cycle_no_hpwl_improvement << ")"
            << ", batch(passes=" << stats.batch_passes
            << ", plans=" << stats.batch_plans
            << ", selected=" << stats.batch_selected
            << ", accepted=" << stats.batch_accepted
            << ", insertion-trials=" << stats.insertion_positions_evaluated
            << ")"
            << ", HPWL=" << hpwl_after
            << "um, improvement=" << hpwl_before - hpwl_after << "um\n";
}

GriddedDetailedPlacer::SwapStats GriddedDetailedPlacer::RunVerticalSwapStage() {
  SwapStats total_stats;
  for (const RowStripe& stripe : row_stripes_) {
    for (size_t i = 1; i < stripe.rows.size(); ++i) {
      SwapStats row_pair_stats = TryClosestComponentSwaps(
          stripe.rows[i - 1], stripe.rows[i], kMaxSwapCandidatesPerRowPair);
      total_stats.candidates += row_pair_stats.candidates;
      total_stats.accepted += row_pair_stats.accepted;
    }
  }
  return total_stats;
}

GriddedDetailedPlacer::SwapStats GriddedDetailedPlacer::RunGlobalSwapStage() {
  SwapStats total_stats;
  for (GriddedRow* row : rows_) {
    for (int source_index = 0;
         source_index < static_cast<int>(row->Components().size());
         ++source_index) {
      SwapStats component_stats = TryOptimalRegionSwaps(row, source_index);
      total_stats.candidates += component_stats.candidates;
      total_stats.accepted += component_stats.accepted;
    }
  }
  return total_stats;
}

void GriddedDetailedPlacer::LogSwapStage(const std::string& stage_name,
                                         const SwapStats& stats,
                                         double hpwl_before) {
  double hpwl_after = WeightedHPWL();
  LOG(info) << "  " << stage_name << ": candidates=" << stats.candidates
            << ", accepted=" << stats.accepted << ", HPWL=" << hpwl_after
            << "um, improvement=" << hpwl_before - hpwl_after << "um\n";
}

void GriddedDetailedPlacer::EmitSnapshot(const std::string& id,
                                         const std::string& label,
                                         const std::string& subgroup,
                                         int iteration) {
  if (!snapshot_callback_) return;
  snapshot_callback_(id, label, subgroup, iteration);
}

void GriddedDetailedPlacer::BuildRowStripeIndex() {
  std::map<std::pair<int, int>, std::vector<GriddedRow*>> grouped_rows;
  for (GriddedRow* row : rows_) {
    grouped_rows[{row->LLX(), row->URX()}].push_back(row);
  }

  row_stripes_.clear();
  row_stripes_.reserve(grouped_rows.size());
  for (auto& entry : grouped_rows) {
    std::vector<GriddedRow*>& rows = entry.second;
    std::sort(rows.begin(), rows.end(),
              [](const GriddedRow* lhs, const GriddedRow* rhs) {
                return lhs->CenterY() < rhs->CenterY();
              });
    row_stripes_.push_back({std::move(rows)});
  }
}

bool GriddedDetailedPlacer::StartPlacement() {
  PrintStartStatement("gridded detailed placement");
  DaliExpects(ckt_ptr_ != nullptr,
              "No input circuit specified for gridded detailed placement");

  ElapsedTime total_timer;
  total_timer.RecordStartTime();
  batch_relocation_wall_s_ = 0;
  sequential_relocation_wall_s_ = 0;
  assignment_cycle_wall_s_ = 0;
  insertion_refinement_wall_s_ = 0;
  double relocation_wall_time = 0;
  double relocation_cpu_time = 0;
  double global_swap_wall_time = 0;
  double global_swap_cpu_time = 0;
  double vertical_swap_wall_time = 0;
  double vertical_swap_cpu_time = 0;
  double local_reorder_wall_time = 0;
  double local_reorder_cpu_time = 0;
  double clustering_wall_time = 0;
  double clustering_cpu_time = 0;
  MoveStats total_relocation_stats;
  SwapStats total_global_swap_stats;
  SwapStats total_vertical_swap_stats;
  ClusterStats total_clustering_stats;
  int clustering_pass_count = 0;

  LOG(info) << "Gridded detailed placement:\n"
            << "  gridded rows: " << rows_.size() << "\n"
            << "  maximum rounds: " << max_rounds_ << "\n"
            << "  relative convergence threshold: " << min_relative_improvement_
            << "\n"
            << "  row relocation: "
            << (enable_relocation_ ? "enabled" : "disabled") << "\n"
            << "  assignment move order: "
            << (enable_batched_assignment_moves_
                    ? "exact-gain ranked, sequential fallback, then "
                      "insertion refinement"
                    : "sequential")
            << "\n"
            << "  maximum candidate rows: " << max_candidate_rows_ << "\n"
            << "  vertical swap: "
            << (enable_vertical_swap_ ? "enabled" : "disabled") << "\n"
            << "  HPWL before : " << WeightedHPWL() << "um\n";

  ElapsedTime clustering_timer;
  clustering_timer.RecordStartTime();
  double hpwl_before_clustering = WeightedHPWL();
  ClusterStats initial_clustering_stats = RunSingleSegmentClustering();
  total_clustering_stats.Add(initial_clustering_stats);
  ++clustering_pass_count;
  clustering_timer.RecordEndTime();
  clustering_wall_time += clustering_timer.GetWallTime();
  clustering_cpu_time += clustering_timer.GetCpuTime();
  LogClusteringPass("initial X clustering", initial_clustering_stats,
                    hpwl_before_clustering);
  RecordPlacementHpwlMetrics("gridded_detailed.initial_clustering", *ckt_ptr_);
  EmitSnapshot("gridded.initial_clustering",
               "Gridded Detailed Initial X Clustering", "initial_clustering",
               0);

  double previous_hpwl = WeightedHPWL();
  int iteration_count = 0;
  for (int iteration = 0; iteration < max_rounds_; ++iteration) {
    LOG(info) << "  detailed iteration " << iteration << "\n";
    ElapsedTime round_timer;
    round_timer.RecordStartTime();

    double hpwl_before_stage = WeightedHPWL();
    ElapsedTime stage_timer;
    if (enable_relocation_) {
      stage_timer.RecordStartTime();
      // Ejection chains provide their useful occupancy change up front. Later
      // rounds retain ordinary relocation without repeating the costly search.
      MoveStats relocation_stats = RunRelocationStage(iteration == 0);
      total_relocation_stats.Add(relocation_stats);
      stage_timer.RecordEndTime();
      relocation_wall_time += stage_timer.GetWallTime();
      relocation_cpu_time += stage_timer.GetCpuTime();
      LogMoveStage(relocation_stats, hpwl_before_stage);
      RecordPlacementHpwlMetrics("gridded_detailed.relocation", *ckt_ptr_);
      EmitSnapshot("gridded.iter_" + std::to_string(iteration) + ".relocation",
                   "Gridded Detailed Iteration " + std::to_string(iteration) +
                       " Row Relocation",
                   "relocation", iteration);
      hpwl_before_stage = WeightedHPWL();
    }

    stage_timer.RecordStartTime();
    SwapStats global_swap_stats = RunGlobalSwapStage();
    total_global_swap_stats.candidates += global_swap_stats.candidates;
    total_global_swap_stats.accepted += global_swap_stats.accepted;
    stage_timer.RecordEndTime();
    global_swap_wall_time += stage_timer.GetWallTime();
    global_swap_cpu_time += stage_timer.GetCpuTime();
    LogSwapStage("global swap", global_swap_stats, hpwl_before_stage);
    RecordPlacementHpwlMetrics("gridded_detailed.global_swap", *ckt_ptr_);
    EmitSnapshot("gridded.iter_" + std::to_string(iteration) + ".global_swap",
                 "Gridded Detailed Iteration " + std::to_string(iteration) +
                     " Global Swap",
                 "global_swap", iteration);

    if (enable_vertical_swap_) {
      hpwl_before_stage = WeightedHPWL();
      stage_timer.RecordStartTime();
      SwapStats vertical_swap_stats = RunVerticalSwapStage();
      total_vertical_swap_stats.candidates += vertical_swap_stats.candidates;
      total_vertical_swap_stats.accepted += vertical_swap_stats.accepted;
      stage_timer.RecordEndTime();
      vertical_swap_wall_time += stage_timer.GetWallTime();
      vertical_swap_cpu_time += stage_timer.GetCpuTime();
      LogSwapStage("vertical swap", vertical_swap_stats, hpwl_before_stage);
      RecordPlacementHpwlMetrics("gridded_detailed.vertical_swap", *ckt_ptr_);
      EmitSnapshot(
          "gridded.iter_" + std::to_string(iteration) + ".vertical_swap",
          "Gridded Detailed Iteration " + std::to_string(iteration) +
              " Vertical Swap",
          "vertical_swap", iteration);
    }

    stage_timer.RecordStartTime();
    RunLocalReorderStage();
    stage_timer.RecordEndTime();
    local_reorder_wall_time += stage_timer.GetWallTime();
    local_reorder_cpu_time += stage_timer.GetCpuTime();
    EmitSnapshot("gridded.iter_" + std::to_string(iteration) + ".local_reorder",
                 "Gridded Detailed Iteration " + std::to_string(iteration) +
                     " Local Reorder",
                 "local_reorder", iteration);

    double current_hpwl = WeightedHPWL();
    double improvement = previous_hpwl - current_hpwl;
    double relative_improvement =
        previous_hpwl > 0 ? improvement / previous_hpwl : 0;
    round_timer.RecordEndTime();
    LOG(info) << "  iteration improvement: " << improvement << "um ("
              << relative_improvement * 100.0 << "%)"
              << ", wall time: " << round_timer.GetWallTime() << "s\n";
    std::string round_metric =
        "gridded_detailed.round_" + std::to_string(iteration);
    RecordPlacementHpwlMetrics(round_metric + ".hpwl", *ckt_ptr_);
    RecordPlacementMetric(round_metric + ".improvement", improvement);
    RecordPlacementMetric(round_metric + ".relative_improvement",
                          relative_improvement);
    RecordPlacementMetric("time." + round_metric + ".wall_s",
                          round_timer.GetWallTime());
    RecordPlacementMetric("time." + round_metric + ".cpu_s",
                          round_timer.GetCpuTime());
    if (improvement <= kMinSignificantHpwlImprovement) {
      break;
    }
    ++iteration_count;
    previous_hpwl = current_hpwl;
    if (relative_improvement < min_relative_improvement_) {
      LOG(info) << "  detailed placement converged: relative improvement "
                << relative_improvement << " is below "
                << min_relative_improvement_ << "\n";
      break;
    }
  }

  if (enable_relocation_ && enable_batched_assignment_moves_) {
    double hpwl_before_insertion_refinement = WeightedHPWL();
    ElapsedTime insertion_timer;
    insertion_timer.RecordStartTime();
    MoveStats insertion_stats = RunBatchedRelocationStage(true);
    total_relocation_stats.Add(insertion_stats);
    insertion_timer.RecordEndTime();
    insertion_refinement_wall_s_ += insertion_timer.GetWallTime();
    relocation_wall_time += insertion_timer.GetWallTime();
    relocation_cpu_time += insertion_timer.GetCpuTime();
    LogMoveStage(insertion_stats, hpwl_before_insertion_refinement);
    RecordPlacementHpwlMetrics("gridded_detailed.insertion_refinement",
                               *ckt_ptr_);
    RecordPlacementMetric("gridded_detailed.insertion_refinement.passes",
                          insertion_stats.batch_passes);
    RecordPlacementMetric("gridded_detailed.insertion_refinement.accepted",
                          insertion_stats.batch_accepted);
    RecordPlacementMetric(
        "gridded_detailed.insertion_refinement.positions_evaluated",
        insertion_stats.insertion_positions_evaluated);
    EmitSnapshot("gridded.insertion_refinement",
                 "Gridded Detailed Insertion Refinement",
                 "insertion_refinement", iteration_count);
  }

  if (enable_safe_pair_merge_) {
    double hpwl_before_merge = WeightedHPWL();
    ElapsedTime merge_timer;
    merge_timer.RecordStartTime();
    RunSafePairMerge();
    merge_timer.RecordEndTime();
    LOG(info) << "  safe-pair merge: HPWL " << hpwl_before_merge << " -> "
              << WeightedHPWL() << "um, wall=" << merge_timer.GetWallTime()
              << "s\n";
    RecordPlacementHpwlMetrics("gridded_detailed.safe_pair_merge", *ckt_ptr_);
  }

  for (int pass = 0; pass < kMaxFinalClusteringPasses; ++pass) {
    clustering_timer.RecordStartTime();
    hpwl_before_clustering = WeightedHPWL();
    ClusterStats clustering_stats = RunSingleSegmentClustering();
    double clustering_hpwl = WeightedHPWL();
    double clustering_improvement = hpwl_before_clustering - clustering_hpwl;
    double relative_improvement =
        hpwl_before_clustering > 0
            ? clustering_improvement / hpwl_before_clustering
            : 0;
    total_clustering_stats.Add(clustering_stats);
    ++clustering_pass_count;
    clustering_timer.RecordEndTime();
    clustering_wall_time += clustering_timer.GetWallTime();
    clustering_cpu_time += clustering_timer.GetCpuTime();
    LogClusteringPass("final X clustering pass " + std::to_string(pass),
                      clustering_stats, hpwl_before_clustering);
    RecordPlacementHpwlMetrics(
        "gridded_detailed.final_clustering_" + std::to_string(pass), *ckt_ptr_);
    EmitSnapshot("gridded.final_clustering." + std::to_string(pass),
                 "Gridded Detailed Final X Clustering " + std::to_string(pass),
                 "final_clustering", pass);
    if (clustering_stats.accepted_rows == 0 ||
        relative_improvement < kMinClusteringRelativeImprovement) {
      break;
    }
  }

  total_timer.RecordEndTime();
  LOG(info) << "  accepted detailed iterations: " << iteration_count << "\n"
            << "  HPWL after  : " << WeightedHPWL() << "um\n"
            << "  time summary:\n"
            << "    row relocation: wall=" << relocation_wall_time
            << "s, cpu=" << relocation_cpu_time << "s\n"
            << "    global swap   : wall=" << global_swap_wall_time
            << "s, cpu=" << global_swap_cpu_time << "s\n"
            << "    vertical swap : wall=" << vertical_swap_wall_time
            << "s, cpu=" << vertical_swap_cpu_time << "s\n"
            << "    local reorder : wall=" << local_reorder_wall_time
            << "s, cpu=" << local_reorder_cpu_time << "s\n"
            << "    X clustering  : wall=" << clustering_wall_time
            << "s, cpu=" << clustering_cpu_time << "s\n"
            << "    total         : wall=" << total_timer.GetWallTime()
            << "s, cpu=" << total_timer.GetCpuTime() << "s\n";

  RecordPlacementMetric("gridded_detailed.iterations", iteration_count);
  RecordPlacementMetric("gridded_detailed.relocation.candidates",
                        total_relocation_stats.candidates);
  RecordPlacementMetric("gridded_detailed.relocation.source_singleton",
                        total_relocation_stats.source_singleton);
  RecordPlacementMetric("gridded_detailed.relocation.width_blocked",
                        total_relocation_stats.width_blocked);
  RecordPlacementMetric("gridded_detailed.relocation.p_well_blocked",
                        total_relocation_stats.p_well_blocked);
  RecordPlacementMetric("gridded_detailed.relocation.n_well_blocked",
                        total_relocation_stats.n_well_blocked);
  RecordPlacementMetric("gridded_detailed.relocation.evaluated",
                        total_relocation_stats.evaluated);
  RecordPlacementMetric("gridded_detailed.relocation.no_hpwl_improvement",
                        total_relocation_stats.no_hpwl_improvement);
  RecordPlacementMetric("gridded_detailed.relocation.accepted",
                        total_relocation_stats.accepted);
  RecordPlacementMetric("gridded_detailed.ejection.attempts",
                        total_relocation_stats.ejection_attempts);
  RecordPlacementMetric("gridded_detailed.ejection.evaluated",
                        total_relocation_stats.ejection_evaluated);
  RecordPlacementMetric("gridded_detailed.ejection.no_hpwl_improvement",
                        total_relocation_stats.ejection_no_hpwl_improvement);
  RecordPlacementMetric("gridded_detailed.ejection.accepted",
                        total_relocation_stats.ejection_accepted);
  RecordPlacementMetric("gridded_detailed.assignment_cycle.attempts",
                        total_relocation_stats.cycle_attempts);
  RecordPlacementMetric("gridded_detailed.assignment_cycle.evaluated",
                        total_relocation_stats.cycle_evaluated);
  RecordPlacementMetric("gridded_detailed.assignment_cycle.no_hpwl_improvement",
                        total_relocation_stats.cycle_no_hpwl_improvement);
  RecordPlacementMetric("gridded_detailed.assignment_cycle.accepted",
                        total_relocation_stats.cycle_accepted);
  RecordPlacementMetric("gridded_detailed.assignment_cycle.invalidated",
                        total_relocation_stats.cycle_invalidated);
  RecordPlacementMetric("gridded_detailed.relocation.batch.passes",
                        total_relocation_stats.batch_passes);
  RecordPlacementMetric("gridded_detailed.relocation.batch.plans",
                        total_relocation_stats.batch_plans);
  RecordPlacementMetric("gridded_detailed.relocation.batch.selected",
                        total_relocation_stats.batch_selected);
  RecordPlacementMetric("gridded_detailed.relocation.batch.accepted",
                        total_relocation_stats.batch_accepted);
  RecordPlacementMetric("gridded_detailed.global_swap.candidates",
                        total_global_swap_stats.candidates);
  RecordPlacementMetric("gridded_detailed.global_swap.accepted",
                        total_global_swap_stats.accepted);
  RecordPlacementMetric("gridded_detailed.vertical_swap.candidates",
                        total_vertical_swap_stats.candidates);
  RecordPlacementMetric("gridded_detailed.vertical_swap.accepted",
                        total_vertical_swap_stats.accepted);
  RecordPlacementMetric("gridded_detailed.clustering.passes",
                        clustering_pass_count);
  RecordPlacementMetric("gridded_detailed.clustering.visited_rows",
                        total_clustering_stats.visited_rows);
  RecordPlacementMetric("gridded_detailed.clustering.changed_rows",
                        total_clustering_stats.changed_rows);
  RecordPlacementMetric("gridded_detailed.clustering.accepted_rows",
                        total_clustering_stats.accepted_rows);
  RecordPlacementMetric("time.gridded_detailed.relocation.wall_s",
                        relocation_wall_time);
  RecordPlacementMetric("time.gridded_detailed.relocation.cpu_s",
                        relocation_cpu_time);
  RecordPlacementMetric("time.gridded_detailed.relocation.batch.wall_s",
                        batch_relocation_wall_s_);
  RecordPlacementMetric("time.gridded_detailed.relocation.sequential.wall_s",
                        sequential_relocation_wall_s_);
  RecordPlacementMetric("time.gridded_detailed.relocation.cycles.wall_s",
                        assignment_cycle_wall_s_);
  RecordPlacementMetric(
      "time.gridded_detailed.relocation.insertion_refinement.wall_s",
      insertion_refinement_wall_s_);
  RecordPlacementMetric("time.gridded_detailed.global_swap.wall_s",
                        global_swap_wall_time);
  RecordPlacementMetric("time.gridded_detailed.global_swap.cpu_s",
                        global_swap_cpu_time);
  RecordPlacementMetric("time.gridded_detailed.vertical_swap.wall_s",
                        vertical_swap_wall_time);
  RecordPlacementMetric("time.gridded_detailed.vertical_swap.cpu_s",
                        vertical_swap_cpu_time);
  RecordPlacementMetric("time.gridded_detailed.local_reorder.wall_s",
                        local_reorder_wall_time);
  RecordPlacementMetric("time.gridded_detailed.local_reorder.cpu_s",
                        local_reorder_cpu_time);
  RecordPlacementMetric("time.gridded_detailed.clustering.wall_s",
                        clustering_wall_time);
  RecordPlacementMetric("time.gridded_detailed.clustering.cpu_s",
                        clustering_cpu_time);
  RecordPlacementMetric("time.gridded_detailed.total.wall_s",
                        total_timer.GetWallTime());
  RecordPlacementMetric("time.gridded_detailed.total.cpu_s",
                        total_timer.GetCpuTime());

  PrintEndStatement("gridded detailed placement", true);
  return true;
}

bool GriddedDetailedPlacer::StartLocalReorder() {
  PrintStartStatement("gridded local reorder");
  DaliExpects(ckt_ptr_ != nullptr,
              "No input circuit specified for gridded local reorder");

  ElapsedTime timer;
  timer.RecordStartTime();
  double hpwl_before = WeightedHPWL();
  RunLocalReorderStage();
  timer.RecordEndTime();

  LOG(info) << "Gridded local reorder:\n"
            << "  gridded rows: " << rows_.size() << "\n"
            << "  HPWL before : " << hpwl_before << "um\n"
            << "  HPWL after  : " << WeightedHPWL() << "um\n"
            << "  wall time   : " << timer.GetWallTime() << "s\n"
            << "  cpu time    : " << timer.GetCpuTime() << "s\n";
  RecordPlacementMetric("time.gridded_detailed.local_reorder.wall_s",
                        timer.GetWallTime());
  RecordPlacementMetric("time.gridded_detailed.local_reorder.cpu_s",
                        timer.GetCpuTime());
  PrintEndStatement("gridded local reorder", true);
  return true;
}

void GriddedDetailedPlacer::RunLocalClosure() {
  DaliExpects(ckt_ptr_ != nullptr,
              "No input circuit specified for gridded local closure");
  RunSingleSegmentClustering();
  RunLocalReorderStage(false);
  RunSingleSegmentClustering();
}

void GriddedDetailedPlacer::RunOneRoundClosure() {
  DaliExpects(ckt_ptr_ != nullptr,
              "No input circuit specified for gridded detailed closure");
  RunSingleSegmentClustering();
  if (enable_relocation_) {
    RunRelocationStage(true);
  }
  RunGlobalSwapStage();
  if (enable_vertical_swap_) {
    RunVerticalSwapStage();
  }
  RunLocalReorderStage(false);
  RunSingleSegmentClustering();
}

}  // namespace dali
