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
#include <set>
#include <utility>

#include "dali/common/elapsed_time.h"
#include "dali/common/logging.h"
#include "dali/common/placement_metrics.h"

namespace dali {

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
}

void GriddedDetailedPlacer::SetSnapshotCallback(
    SnapshotCallback snapshot_callback) {
  snapshot_callback_ = std::move(snapshot_callback);
}

double GriddedDetailedPlacer::WireLengthCost(GriddedRow* row, int left_index,
                                             int right_index) const {
  auto& net_list = ckt_ptr_->Nets();
  std::set<Net*> involved_nets;
  for (int i = left_index; i <= right_index; ++i) {
    Component* component = row->Components()[i];
    for (int net_id : component->NetList()) {
      if (net_list[net_id].PinCnt() < 100) {
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

int GriddedDetailedPlacer::RunLocalReorderStage() {
  double previous_hpwl = WeightedHPWL();
  int total_changed_windows = 0;
  int iteration_count = 0;
  for (int iteration = 0; iteration < kMaxLocalReorderIterations; ++iteration) {
    auto row_state_before_iteration = SaveRowState(rows_);
    int reorder_windows = LocalReorderAllRows();
    double current_hpwl = WeightedHPWL();
    double improvement = previous_hpwl - current_hpwl;
    LOG(info) << "  local reorder iteration " << iteration
              << ": windows=" << reorder_windows << ", HPWL=" << current_hpwl
              << "um"
              << ", improvement=" << improvement << "um\n";
    if (improvement <= kMinSignificantHpwlImprovement) {
      RestoreRowState(row_state_before_iteration);
      LOG(info) << "    rejected: no significant HPWL improvement\n";
      break;
    }
    total_changed_windows += reorder_windows;
    RecordPlacementMetric("gridded_detailed.local_reorder", current_hpwl);
    ++iteration_count;
    previous_hpwl = current_hpwl;
  }
  RecordPlacementMetric("gridded_detailed.local_reorder", WeightedHPWL());

  LOG(info) << "  local reorder iterations: " << iteration_count << "\n"
            << "  accepted reorder windows: " << total_changed_windows << "\n";
  return total_changed_windows;
}

bool GriddedDetailedPlacer::IsSwapCandidate(Component* component) const {
  return component->IsMovable() && !component->NetList().empty() &&
         component->MacroPtr() != ckt_ptr_->tech().IoDummyMacroPtr();
}

int GriddedDetailedPlacer::UsedWidthAfterSwap(GriddedRow* row,
                                              Component* removed,
                                              Component* added) const {
  int used_width = 0;
  for (Component* component : row->Components()) {
    used_width += component == removed ? added->Width() : component->Width();
  }
  return used_width;
}

int GriddedDetailedPlacer::RequiredPHeightAfterSwap(GriddedRow* row,
                                                    Component* removed,
                                                    Component* added) const {
  int required_height = 0;
  for (Component* component : row->Components()) {
    Component* candidate = component == removed ? added : component;
    required_height =
        std::max(required_height, candidate->MacroPtr()->FirstPwellHeight());
  }
  return required_height;
}

int GriddedDetailedPlacer::RequiredNHeightAfterSwap(GriddedRow* row,
                                                    Component* removed,
                                                    Component* added) const {
  int required_height = 0;
  for (Component* component : row->Components()) {
    Component* candidate = component == removed ? added : component;
    required_height =
        std::max(required_height, candidate->MacroPtr()->FirstNwellHeight());
  }
  return required_height;
}

bool GriddedDetailedPlacer::IsNonHeightIncreasingSwap(
    GriddedRow* first_row, Component* first_component, GriddedRow* second_row,
    Component* second_component) const {
  if (UsedWidthAfterSwap(first_row, first_component, second_component) >
          first_row->UsableWidth() ||
      UsedWidthAfterSwap(second_row, second_component, first_component) >
          second_row->UsableWidth()) {
    return false;
  }

  return RequiredPHeightAfterSwap(first_row, first_component,
                                  second_component) <= first_row->PHeight() &&
         RequiredNHeightAfterSwap(first_row, first_component,
                                  second_component) <= first_row->NHeight() &&
         RequiredPHeightAfterSwap(second_row, second_component,
                                  first_component) <= second_row->PHeight() &&
         RequiredNHeightAfterSwap(second_row, second_component,
                                  first_component) <= second_row->NHeight();
}

double GriddedDetailedPlacer::RowPairWireLengthCost(
    GriddedRow* first_row, GriddedRow* second_row) const {
  std::set<int> net_ids;
  auto collect_row_net_ids = [&net_ids](GriddedRow* row) {
    for (Component* component : row->Components()) {
      for (int net_id : component->NetList()) {
        net_ids.insert(net_id);
      }
    }
  };
  collect_row_net_ids(first_row);
  collect_row_net_ids(second_row);

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

GriddedDetailedPlacer::OptimalRegion
GriddedDetailedPlacer::ComputeOptimalRegion(Component* component) const {
  std::vector<double> x_bounds;
  std::vector<double> y_bounds;
  auto& nets = ckt_ptr_->Nets();
  for (int net_id : component->NetList()) {
    Net& net = nets[net_id];
    if (net.PinCnt() <= 1 || net.PinCnt() >= 100) {
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
        offset_x = pin.OffsetX();
        offset_y = pin.OffsetY();
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
    x_bounds.push_back(min_x - offset_x);
    x_bounds.push_back(max_x - offset_x);
    y_bounds.push_back(min_y - offset_y);
    y_bounds.push_back(max_y - offset_y);
  }

  if (x_bounds.empty() || y_bounds.empty()) {
    return {};
  }

  std::sort(x_bounds.begin(), x_bounds.end());
  std::sort(y_bounds.begin(), y_bounds.end());
  int lx_index = static_cast<int>(x_bounds.size() - 1) / 2;
  int ux_index = lx_index;
  if (x_bounds.size() % 2 == 0) {
    ++ux_index;
  }
  int ly_index = static_cast<int>(y_bounds.size() - 1) / 2;
  int uy_index = ly_index;
  if (y_bounds.size() % 2 == 0) {
    ++uy_index;
  }

  return {true, x_bounds[lx_index], y_bounds[ly_index], x_bounds[ux_index],
          y_bounds[uy_index]};
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

void GriddedDetailedPlacer::LegalizeRowsAfterSwap(GriddedRow* first_row,
                                                  GriddedRow* second_row) {
  for (Component* component : first_row->Components()) {
    PlaceComponentInRow(first_row, component);
  }
  for (Component* component : second_row->Components()) {
    PlaceComponentInRow(second_row, component);
  }
  first_row->LegalizeLooseX();
  second_row->LegalizeLooseX();
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

  double row_pair_cost_before = RowPairWireLengthCost(first_row, second_row);
  auto row_state_before_swap = SaveRowState({first_row, second_row});
  std::swap(first_row->Components()[first_index],
            second_row->Components()[second_index]);
  LegalizeRowsAfterSwap(first_row, second_row);

  double row_pair_cost_after = RowPairWireLengthCost(first_row, second_row);
  if (row_pair_cost_after + kMinSignificantHpwlImprovement >=
      row_pair_cost_before) {
    RestoreRowState(row_state_before_swap);
    return false;
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
  struct CandidateRow {
    GriddedRow* row = nullptr;
    double distance = 0;
  };
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
  double current_y_distance =
      DistanceToOptimalRegionY(source_row, source_component, region);
  if (current_y_distance <= kMinSignificantHpwlImprovement) {
    return {};
  }

  std::vector<CandidateRow> candidate_rows;
  candidate_rows.reserve(rows_.size());
  for (GriddedRow* row : rows_) {
    if (row == source_row) {
      continue;
    }
    double row_distance =
        DistanceToOptimalRegionY(row, source_component, region);
    if (row_distance < current_y_distance) {
      candidate_rows.push_back({row, row_distance});
    }
  }
  std::sort(candidate_rows.begin(), candidate_rows.end(),
            [](const CandidateRow& lhs, const CandidateRow& rhs) {
              return lhs.distance < rhs.distance;
            });

  SwapStats stats;
  int row_limit = std::min(kMaxOptimalRegionRowsPerComponent,
                           static_cast<int>(candidate_rows.size()));
  for (int row_id = 0; row_id < row_limit; ++row_id) {
    GriddedRow* target_row = candidate_rows[row_id].row;
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
          {target_index, DistanceToOptimalRegionX(target_component, region)});
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

GriddedDetailedPlacer::SwapStats GriddedDetailedPlacer::RunVerticalSwapStage() {
  SwapStats total_stats;
  for (size_t i = 1; i < rows_.size(); ++i) {
    SwapStats row_pair_stats = TryClosestComponentSwaps(
        rows_[i - 1], rows_[i], kMaxSwapCandidatesPerRowPair);
    total_stats.candidates += row_pair_stats.candidates;
    total_stats.accepted += row_pair_stats.accepted;
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

bool GriddedDetailedPlacer::StartPlacement() {
  PrintStartStatement("gridded detailed placement");
  DaliExpects(ckt_ptr_ != nullptr,
              "No input circuit specified for gridded detailed placement");

  ElapsedTime total_timer;
  total_timer.RecordStartTime();
  double global_swap_wall_time = 0;
  double global_swap_cpu_time = 0;
  double vertical_swap_wall_time = 0;
  double vertical_swap_cpu_time = 0;
  double local_reorder_wall_time = 0;
  double local_reorder_cpu_time = 0;

  LOG(info) << "Gridded detailed placement:\n"
            << "  gridded rows: " << rows_.size() << "\n"
            << "  HPWL before : " << WeightedHPWL() << "um\n";

  double previous_hpwl = WeightedHPWL();
  int iteration_count = 0;
  for (int iteration = 0; iteration < kMaxDetailedIterations; ++iteration) {
    LOG(info) << "  detailed iteration " << iteration << "\n";

    double hpwl_before_stage = WeightedHPWL();
    ElapsedTime stage_timer;
    stage_timer.RecordStartTime();
    SwapStats global_swap_stats = RunGlobalSwapStage();
    stage_timer.RecordEndTime();
    global_swap_wall_time += stage_timer.GetWallTime();
    global_swap_cpu_time += stage_timer.GetCpuTime();
    LogSwapStage("global swap", global_swap_stats, hpwl_before_stage);
    RecordPlacementMetric("gridded_detailed.global_swap", WeightedHPWL());
    EmitSnapshot("gridded.iter_" + std::to_string(iteration) + ".global_swap",
                 "Gridded Detailed Iteration " + std::to_string(iteration) +
                     " Global Swap",
                 "global_swap", iteration);

    hpwl_before_stage = WeightedHPWL();
    stage_timer.RecordStartTime();
    SwapStats vertical_swap_stats = RunVerticalSwapStage();
    stage_timer.RecordEndTime();
    vertical_swap_wall_time += stage_timer.GetWallTime();
    vertical_swap_cpu_time += stage_timer.GetCpuTime();
    LogSwapStage("vertical swap", vertical_swap_stats, hpwl_before_stage);
    RecordPlacementMetric("gridded_detailed.vertical_swap", WeightedHPWL());
    EmitSnapshot("gridded.iter_" + std::to_string(iteration) + ".vertical_swap",
                 "Gridded Detailed Iteration " + std::to_string(iteration) +
                     " Vertical Swap",
                 "vertical_swap", iteration);

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
    LOG(info) << "  iteration improvement: " << improvement << "um\n";
    if (improvement <= kMinSignificantHpwlImprovement) {
      break;
    }
    ++iteration_count;
    previous_hpwl = current_hpwl;
  }

  total_timer.RecordEndTime();
  LOG(info) << "  accepted detailed iterations: " << iteration_count << "\n"
            << "  HPWL after  : " << WeightedHPWL() << "um\n"
            << "  time summary:\n"
            << "    global swap   : wall=" << global_swap_wall_time
            << "s, cpu=" << global_swap_cpu_time << "s\n"
            << "    vertical swap : wall=" << vertical_swap_wall_time
            << "s, cpu=" << vertical_swap_cpu_time << "s\n"
            << "    local reorder : wall=" << local_reorder_wall_time
            << "s, cpu=" << local_reorder_cpu_time << "s\n"
            << "    total         : wall=" << total_timer.GetWallTime()
            << "s, cpu=" << total_timer.GetCpuTime() << "s\n";

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

}  // namespace dali
