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
#include <set>
#include <utility>

#include "dali/common/logging.h"
#include "dali/common/placement_metrics.h"

namespace dali {
namespace {

struct GriddedRowSnapshot {
  GriddedRow* row = nullptr;
  std::vector<Component*> component_order;
  std::vector<double> component_lx;
};

std::vector<GriddedRowSnapshot> SaveRowState(
    const std::vector<GriddedRow*>& rows) {
  std::vector<GriddedRowSnapshot> snapshots;
  snapshots.reserve(rows.size());
  for (GriddedRow* row : rows) {
    GriddedRowSnapshot snapshot;
    snapshot.row = row;
    snapshot.component_order = row->Components();
    snapshot.component_lx.reserve(row->Components().size());
    for (Component* component : row->Components()) {
      snapshot.component_lx.push_back(component->LLX());
    }
    snapshots.push_back(snapshot);
  }
  return snapshots;
}

void RestoreRowState(const std::vector<GriddedRowSnapshot>& snapshots) {
  for (const GriddedRowSnapshot& snapshot : snapshots) {
    snapshot.row->Components() = snapshot.component_order;
    for (size_t i = 0; i < snapshot.component_order.size(); ++i) {
      snapshot.component_order[i]->SetLLX(snapshot.component_lx[i]);
    }
  }
}

}  // namespace

void GriddedDetailedPlacer::SetRows(std::vector<GriddedRow*> rows) {
  rows_ = std::move(rows);
}

double GriddedDetailedPlacer::WireLengthCost(GriddedRow* row, int left_index,
                                             int right_index) {
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
                                               int gap, int window_size) {
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

int GriddedDetailedPlacer::LocalReorderInRow(GriddedRow* row, int window_size) {
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

bool GriddedDetailedPlacer::StartPlacement() {
  PrintStartStatement("gridded detailed placement");
  DaliExpects(ckt_ptr_ != nullptr,
              "No input circuit specified for gridded detailed placement");

  double previous_hpwl = WeightedHPWL();
  LOG(info) << "Gridded detailed placement local re-ordering:\n"
            << "  gridded rows: " << rows_.size() << "\n"
            << "  HPWL before : " << previous_hpwl << "um\n";

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

  LOG(info) << "  iterations: " << iteration_count << "\n"
            << "  accepted reorder windows: " << total_changed_windows << "\n"
            << "  HPWL after  : " << WeightedHPWL() << "um\n";

  PrintEndStatement("gridded detailed placement", true);
  return true;
}

}  // namespace dali
