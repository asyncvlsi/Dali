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
#include "dali/placer/well_legalizer/exact_gridded_legalization_window_analyzer.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "dali/common/helper.h"
#include "dali/placer/well_legalizer/exact_gridded_legalization_model_builder.h"

namespace dali {

ExactGriddedLegalizationWindowAnalyzer::ExactGriddedLegalizationWindowAnalyzer(
    Circuit* circuit, const ExactGriddedWindowAnalyzerConfig& config)
    : circuit_(circuit), config_(config) {
  DaliExpects(circuit_ != nullptr,
              "Exact gridded window analyzer requires a circuit");
  DaliExpects(config_.target_components_per_window > 0,
              "Exact gridded target window size must be positive");
  DaliExpects(config_.maximum_components_per_window >=
                  config_.target_components_per_window,
              "Exact gridded maximum window size must cover the target size");
  DaliExpects(config_.maximum_windows > 0,
              "Exact gridded maximum window count must be positive");
  DaliExpects(config_.net_ignore_threshold >= 2,
              "Exact gridded net ignore threshold must be at least two");
  DaliExpects(
      config_.minimum_p_well_height >= 0 && config_.minimum_n_well_height >= 0,
      "Exact gridded minimum well heights must be non-negative");
  DaliExpects(config_.maximum_time_seconds_per_window > 0.0,
              "Exact gridded window solve time must be positive");
  DaliExpects(config_.number_of_workers > 0,
              "Exact gridded worker count must be positive");
}

double ExactGriddedLegalizationWindowAnalyzer::CurrentWindowHpwl(
    const std::vector<Component*>& components) const {
  std::vector<int> net_ids;
  for (const Component* component : components) {
    net_ids.insert(net_ids.end(), component->NetList().begin(),
                   component->NetList().end());
  }
  std::sort(net_ids.begin(), net_ids.end());
  net_ids.erase(std::unique(net_ids.begin(), net_ids.end()), net_ids.end());

  double hpwl = 0.0;
  for (int net_id : net_ids) {
    Net& net = circuit_->Nets()[net_id];
    if (net.PinCnt() < 2 ||
        net.PinCnt() >= static_cast<size_t>(config_.net_ignore_threshold) ||
        net.Weight() <= 0.0) {
      continue;
    }
    hpwl += circuit_->NetWeightedHPWL(net_id);
  }
  return hpwl;
}

std::vector<ExactGriddedLegalizationWindowAnalyzer::WindowCandidate>
ExactGriddedLegalizationWindowAnalyzer::BuildStripeWindows(
    Stripe* stripe, int column_index, int stripe_index,
    int* oversized_windows) const {
  DaliExpects(stripe != nullptr, "Cannot analyze a null stripe");
  DaliExpects(oversized_windows != nullptr,
              "Oversized-window counter must not be null");

  std::vector<GriddedRow*> rows;
  rows.reserve(stripe->gridded_rows_.size());
  for (GriddedRow& row : stripe->gridded_rows_) rows.push_back(&row);
  std::sort(rows.begin(), rows.end(),
            [](const GriddedRow* first, const GriddedRow* second) {
              return first->LLY() < second->LLY();
            });

  struct ComponentExtent {
    Component* component = nullptr;
    int first_row = -1;
    int last_row = -1;
  };
  std::unordered_map<int, ComponentExtent> extents;
  std::vector<std::vector<int> > row_component_ids(rows.size());
  // Components() is the canonical row ownership used by final legalization
  // and gridded detailed placement. ComponentRegions() belongs to a separate
  // region-level path and is not populated for these finalized rows.
  for (int row_index = 0; row_index < static_cast<int>(rows.size());
       ++row_index) {
    for (Component* component : rows[row_index]->Components()) {
      auto [extent_it, inserted] = extents.emplace(
          component->Id(), ComponentExtent{component, row_index, row_index});
      if (!inserted) {
        extent_it->second.first_row =
            std::min(extent_it->second.first_row, row_index);
        extent_it->second.last_row =
            std::max(extent_it->second.last_row, row_index);
      }
      row_component_ids[row_index].push_back(component->Id());
    }
    std::sort(row_component_ids[row_index].begin(),
              row_component_ids[row_index].end());
    row_component_ids[row_index].erase(
        std::unique(row_component_ids[row_index].begin(),
                    row_component_ids[row_index].end()),
        row_component_ids[row_index].end());
  }

  std::vector<WindowCandidate> windows;
  int first_row = 0;
  while (first_row < static_cast<int>(rows.size())) {
    int last_row = first_row;
    int scanned_row = first_row;
    std::unordered_set<int> component_ids;
    bool crosses_lower_boundary = false;
    while (true) {
      while (scanned_row <= last_row) {
        for (int component_id : row_component_ids[scanned_row]) {
          const ComponentExtent& extent = extents.at(component_id);
          if (extent.first_row < first_row) crosses_lower_boundary = true;
          component_ids.insert(component_id);
          last_row = std::max(last_row, extent.last_row);
        }
        ++scanned_row;
      }
      if (crosses_lower_boundary ||
          component_ids.size() >=
              static_cast<size_t>(config_.target_components_per_window) ||
          last_row + 1 >= static_cast<int>(rows.size())) {
        break;
      }
      ++last_row;
    }

    DaliExpects(!crosses_lower_boundary,
                "Exact gridded window split a multi-region component");
    if (component_ids.empty()) {
      first_row = last_row + 1;
      continue;
    }
    if (component_ids.size() >
        static_cast<size_t>(config_.maximum_components_per_window)) {
      ++*oversized_windows;
      first_row = last_row + 1;
      continue;
    }

    WindowCandidate window;
    window.column_index = column_index;
    window.stripe_index = stripe_index;
    window.first_row_index = first_row;
    window.last_row_index = last_row;
    window.lx = stripe->LLX();
    window.ly = rows[first_row]->LLY();
    window.ux = stripe->URX();
    window.uy = rows[last_row]->URY();
    for (int row_index = first_row; row_index <= last_row; ++row_index) {
      window.left_boundary_margin = std::max(
          window.left_boundary_margin, rows[row_index]->LeftBoundaryMargin());
      window.right_boundary_margin = std::max(
          window.right_boundary_margin, rows[row_index]->RightBoundaryMargin());
    }
    window.components.reserve(component_ids.size());
    for (int component_id : component_ids) {
      window.components.push_back(extents.at(component_id).component);
    }
    std::sort(window.components.begin(), window.components.end(),
              [](const Component* first, const Component* second) {
                return first->Id() < second->Id();
              });
    window.current_weighted_hpwl = CurrentWindowHpwl(window.components);
    windows.push_back(std::move(window));
    first_row = last_row + 1;
  }
  return windows;
}

ExactGriddedWindowAnalysis ExactGriddedLegalizationWindowAnalyzer::Analyze(
    std::vector<StripeColumn>* columns) const {
  DaliExpects(columns != nullptr,
              "Exact gridded window analysis requires stripe columns");

  ExactGriddedWindowAnalysis analysis;
  analysis.available = OrToolsExactGriddedLegalizer::IsAvailable();
  if (!analysis.available) return analysis;

  std::vector<WindowCandidate> candidates;
  for (int column_index = 0; column_index < static_cast<int>(columns->size());
       ++column_index) {
    StripeColumn& column = (*columns)[column_index];
    for (int stripe_index = 0;
         stripe_index < static_cast<int>(column.stripe_list_.size());
         ++stripe_index) {
      std::vector<WindowCandidate> stripe_windows =
          BuildStripeWindows(&column.stripe_list_[stripe_index], column_index,
                             stripe_index, &analysis.oversized_windows);
      candidates.insert(candidates.end(),
                        std::make_move_iterator(stripe_windows.begin()),
                        std::make_move_iterator(stripe_windows.end()));
    }
  }
  analysis.candidate_windows = static_cast<int>(candidates.size());
  std::sort(candidates.begin(), candidates.end(),
            [](const WindowCandidate& first, const WindowCandidate& second) {
              if (first.current_weighted_hpwl != second.current_weighted_hpwl) {
                return first.current_weighted_hpwl >
                       second.current_weighted_hpwl;
              }
              if (first.column_index != second.column_index) {
                return first.column_index < second.column_index;
              }
              if (first.stripe_index != second.stripe_index) {
                return first.stripe_index < second.stripe_index;
              }
              return first.first_row_index < second.first_row_index;
            });

  ExactGriddedModelBuilderConfig builder_config;
  builder_config.net_ignore_threshold = config_.net_ignore_threshold;
  ExactGriddedLegalizationModelBuilder builder(circuit_, builder_config);
  ExactGriddedLegalizationConfig solver_config;
  solver_config.maximum_time_seconds = config_.maximum_time_seconds_per_window;
  solver_config.number_of_workers = config_.number_of_workers;
  OrToolsExactGriddedLegalizer solver;

  int windows_to_solve =
      std::min(config_.maximum_windows, static_cast<int>(candidates.size()));
  analysis.windows.reserve(windows_to_solve);
  for (int candidate_index = 0; candidate_index < windows_to_solve;
       ++candidate_index) {
    const WindowCandidate& candidate = candidates[candidate_index];
    ExactGriddedStripe model_stripe;
    model_stripe.stripe_id = 0;
    model_stripe.lx = candidate.lx;
    model_stripe.ly = candidate.ly;
    model_stripe.ux = candidate.ux;
    model_stripe.uy = candidate.uy;
    model_stripe.maximum_rows =
        candidate.last_row_index - candidate.first_row_index + 1;
    model_stripe.left_boundary_margin = candidate.left_boundary_margin;
    model_stripe.right_boundary_margin = candidate.right_boundary_margin;
    model_stripe.minimum_p_well_height = config_.minimum_p_well_height;
    model_stripe.minimum_n_well_height = config_.minimum_n_well_height;

    std::vector<ExactGriddedComponentDomain> domains;
    domains.reserve(candidate.components.size());
    for (Component* component : candidate.components) {
      domains.push_back({component, {model_stripe.stripe_id}});
    }
    ExactGriddedLegalizationModel model =
        builder.Build(domains, {model_stripe});
    ExactGriddedLegalizationResult solution =
        solver.Solve(model, solver_config);

    ExactGriddedWindowResult result;
    result.column_index = candidate.column_index;
    result.stripe_index = candidate.stripe_index;
    result.first_row_index = candidate.first_row_index;
    result.last_row_index = candidate.last_row_index;
    result.component_count = static_cast<int>(candidate.components.size());
    result.net_count = static_cast<int>(model.nets.size());
    result.current_weighted_hpwl = candidate.current_weighted_hpwl;
    result.solved_weighted_hpwl = solution.weighted_hpwl;
    result.best_objective_bound = solution.best_objective_bound;
    result.relative_gap = solution.relative_gap;
    result.wall_time_seconds = solution.wall_time_seconds;
    result.status = solution.status;
    analysis.windows.push_back(result);
    ++analysis.attempted_windows;
    analysis.solver_wall_time_seconds += solution.wall_time_seconds;
    if (!solution.HasSolution()) continue;

    ++analysis.solved_windows;
    if (solution.status == ExactGriddedLegalizationStatus::kOptimal) {
      ++analysis.optimal_windows;
    }
    analysis.diagnostic_current_hpwl_sum += result.current_weighted_hpwl;
    analysis.diagnostic_solved_hpwl_sum += result.solved_weighted_hpwl;
    analysis.diagnostic_lower_bound_sum += result.best_objective_bound;
  }
  return analysis;
}

}  // namespace dali
