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
#include "dali/placer/well_legalizer/ortools_gridded_boundary_refiner.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "dali/common/helper.h"
#include "dali/placer/well_legalizer/ortools_compact_gridded_legalizer.h"

namespace dali {

OrToolsGriddedBoundaryRefiner::OrToolsGriddedBoundaryRefiner(
    Circuit* circuit, const OrToolsGriddedBoundaryRefinerConfig& config)
    : circuit_(circuit), config_(config) {
  DaliExpects(circuit_ != nullptr, "Cross-stripe refiner requires a circuit");
  DaliExpects(config_.maximum_time_seconds_per_model > 0.0,
              "Cross-stripe model time must be positive");
  DaliExpects(config_.maximum_total_time_seconds > 0.0,
              "Cross-stripe total time must be positive");
  DaliExpects(config_.minimum_relative_improvement >= 0.0,
              "Cross-stripe convergence threshold must be non-negative");
  DaliExpects(config_.maximum_sweeps > 0,
              "Cross-stripe refiner must allow at least one sweep");
  DaliExpects(config_.maximum_components_per_model > 0,
              "Cross-stripe component limit must be positive");
  DaliExpects(config_.maximum_assignment_changes >= -1,
              "Cross-stripe assignment budget must be at least negative one");
  DaliExpects(config_.number_of_workers > 0,
              "Cross-stripe worker count must be positive");
  DaliExpects(config_.net_ignore_threshold >= 2,
              "Cross-stripe net ignore threshold must be at least two");
}

std::vector<GriddedRow*> OrToolsGriddedBoundaryRefiner::SortedRows(
    Stripe* stripe) const {
  DaliExpects(stripe != nullptr, "Cannot collect rows from a null stripe");
  std::vector<GriddedRow*> rows;
  rows.reserve(stripe->gridded_rows_.size());
  for (GriddedRow& row : stripe->gridded_rows_) rows.push_back(&row);
  std::sort(rows.begin(), rows.end(),
            [](const GriddedRow* lhs, const GriddedRow* rhs) {
              return lhs->LLY() < rhs->LLY();
            });
  return rows;
}

std::pair<int, int> OrToolsGriddedBoundaryRefiner::ClosedRowRange(
    Stripe* stripe, int seed_row) const {
  const std::vector<GriddedRow*> rows = SortedRows(stripe);
  DaliExpects(seed_row >= 0 && seed_row < static_cast<int>(rows.size()),
              "Cross-stripe seed row is invalid");

  std::unordered_map<int, std::pair<int, int>> component_extents;
  for (int row_index = 0; row_index < static_cast<int>(rows.size());
       ++row_index) {
    for (const Component* component : rows[row_index]->Components()) {
      auto [extent, inserted] = component_extents.emplace(
          component->Id(), std::make_pair(row_index, row_index));
      if (!inserted) {
        extent->second.first = std::min(extent->second.first, row_index);
        extent->second.second = std::max(extent->second.second, row_index);
      }
    }
  }

  int first_row = seed_row;
  int last_row = seed_row;
  bool expanded = true;
  while (expanded) {
    expanded = false;
    for (int row_index = first_row; row_index <= last_row; ++row_index) {
      for (const Component* component : rows[row_index]->Components()) {
        const auto extent = component_extents.at(component->Id());
        if (extent.first < first_row) {
          first_row = extent.first;
          expanded = true;
        }
        if (extent.second > last_row) {
          last_row = extent.second;
          expanded = true;
        }
      }
    }
  }
  return {first_row, last_row};
}

int OrToolsGriddedBoundaryRefiner::NearestRowIndex(
    const std::vector<GriddedRow*>& rows, double y) const {
  DaliExpects(!rows.empty(), "Cannot select a row from an empty stripe");
  int nearest_row = 0;
  double nearest_distance = std::numeric_limits<double>::max();
  for (int row_index = 0; row_index < static_cast<int>(rows.size());
       ++row_index) {
    const double center =
        0.5 * (rows[row_index]->LLY() + rows[row_index]->URY());
    const double distance = std::abs(center - y);
    if (distance < nearest_distance) {
      nearest_distance = distance;
      nearest_row = row_index;
    }
  }
  return nearest_row;
}

std::vector<OrToolsGriddedBoundaryRefiner::BoundaryTarget>
OrToolsGriddedBoundaryRefiner::BuildTargets(
    std::vector<StripeColumn>* columns) const {
  std::vector<int> column_order(columns->size());
  for (int index = 0; index < static_cast<int>(columns->size()); ++index) {
    column_order[index] = index;
  }
  std::sort(column_order.begin(), column_order.end(),
            [columns](int lhs, int rhs) {
              return (*columns)[lhs].LLX() < (*columns)[rhs].LLX();
            });

  std::unordered_map<const Stripe*, int> stripe_ids;
  int next_stripe_id = 0;
  for (int column_index : column_order) {
    for (Stripe& stripe : (*columns)[column_index].stripe_list_) {
      stripe_ids.emplace(&stripe, next_stripe_id++);
    }
  }

  std::vector<BoundaryTarget> targets;
  for (int order_index = 0;
       order_index + 1 < static_cast<int>(column_order.size()); ++order_index) {
    const int first_column = column_order[order_index];
    const int second_column = column_order[order_index + 1];
    for (int first_stripe = 0;
         first_stripe <
         static_cast<int>((*columns)[first_column].stripe_list_.size());
         ++first_stripe) {
      Stripe* first = &(*columns)[first_column].stripe_list_[first_stripe];
      const std::vector<GriddedRow*> first_rows = SortedRows(first);
      if (first_rows.empty()) continue;
      for (int second_stripe = 0;
           second_stripe <
           static_cast<int>((*columns)[second_column].stripe_list_.size());
           ++second_stripe) {
        Stripe* second = &(*columns)[second_column].stripe_list_[second_stripe];
        if (first->URY() <= second->LLY() || second->URY() <= first->LLY()) {
          continue;
        }
        const std::vector<GriddedRow*> second_rows = SortedRows(second);
        if (second_rows.empty()) continue;
        for (int first_seed = 0;
             first_seed < static_cast<int>(first_rows.size()); ++first_seed) {
          const double center = 0.5 * (first_rows[first_seed]->LLY() +
                                       first_rows[first_seed]->URY());
          targets.push_back(
              {first_column, second_column, first_stripe, second_stripe,
               stripe_ids.at(first), stripe_ids.at(second), first_seed,
               NearestRowIndex(second_rows, center), first, second});
        }
      }
    }
  }
  return targets;
}

int OrToolsGriddedBoundaryRefiner::ComponentCount(
    const BoundaryTarget& target, const std::pair<int, int>& first_range,
    const std::pair<int, int>& second_range) const {
  std::unordered_set<int> component_ids;
  const std::vector<GriddedRow*> first_rows = SortedRows(target.first);
  const std::vector<GriddedRow*> second_rows = SortedRows(target.second);
  for (int row = first_range.first; row <= first_range.second; ++row) {
    for (const Component* component : first_rows[row]->Components()) {
      component_ids.insert(component->Id());
    }
  }
  for (int row = second_range.first; row <= second_range.second; ++row) {
    for (const Component* component : second_rows[row]->Components()) {
      component_ids.insert(component->Id());
    }
  }
  return static_cast<int>(component_ids.size());
}

OrToolsGriddedBoundaryRefinerResult OrToolsGriddedBoundaryRefiner::Optimize(
    std::vector<StripeColumn>* columns) const {
  DaliExpects(columns != nullptr,
              "Cross-stripe refiner requires stripe columns");
  OrToolsGriddedBoundaryRefinerResult aggregate;
  aggregate.available = OrToolsCompactGriddedLegalizer::IsAvailable();
  aggregate.hpwl_before = circuit_->WeightedHPWL();
  aggregate.hpwl_after = aggregate.hpwl_before;
  if (!aggregate.available) return aggregate;

  const std::vector<BoundaryTarget> targets = BuildTargets(columns);
  aggregate.candidate_windows = static_cast<int>(targets.size());
  const auto start_time = std::chrono::steady_clock::now();
  for (int sweep = 0; sweep < config_.maximum_sweeps; ++sweep) {
    const double sweep_hpwl_before = circuit_->WeightedHPWL();
    int accepted_in_sweep = 0;
    for (int offset = 0; offset < static_cast<int>(targets.size()); ++offset) {
      const int target_index =
          sweep % 2 == 0 ? offset
                         : static_cast<int>(targets.size()) - 1 - offset;
      const BoundaryTarget& target = targets[target_index];
      const std::pair<int, int> first_range =
          ClosedRowRange(target.first, target.first_seed_row);
      const std::pair<int, int> second_range =
          ClosedRowRange(target.second, target.second_seed_row);
      const int component_count =
          ComponentCount(target, first_range, second_range);
      if (component_count == 0) continue;
      if (component_count > config_.maximum_components_per_model) {
        ++aggregate.oversized_windows;
        continue;
      }

      OrToolsGriddedBoundaryOptimizerConfig optimizer_config;
      optimizer_config.maximum_time_seconds =
          config_.maximum_time_seconds_per_model;
      optimizer_config.number_of_workers = config_.number_of_workers;
      optimizer_config.net_ignore_threshold = config_.net_ignore_threshold;
      optimizer_config.minimum_p_well_height = config_.minimum_p_well_height;
      optimizer_config.minimum_n_well_height = config_.minimum_n_well_height;
      optimizer_config.maximum_row_displacement = -1;
      optimizer_config.maximum_assignment_changes =
          config_.maximum_assignment_changes;
      optimizer_config.run_local_detailed_closure =
          config_.run_local_detailed_closure;
      optimizer_config.use_solution_hint = config_.use_solution_hint;

      OrToolsGriddedBoundaryWindowResult window;
      window.sweep = sweep;
      window.first_column = target.first_column;
      window.second_column = target.second_column;
      window.first_stripe = target.first_stripe;
      window.second_stripe = target.second_stripe;
      window.first_row = first_range.first;
      window.first_last_row = first_range.second;
      window.second_row = second_range.first;
      window.second_last_row = second_range.second;
      window.optimization =
          OrToolsGriddedBoundaryOptimizer(circuit_, optimizer_config)
              .Optimize(target.first, target.first_stripe_id, first_range.first,
                        first_range.second, target.second,
                        target.second_stripe_id, second_range.first,
                        second_range.second);
      ++aggregate.attempted_models;
      if (window.optimization.status ==
              ExactGriddedLegalizationStatus::kFeasible ||
          window.optimization.status ==
              ExactGriddedLegalizationStatus::kOptimal) {
        ++aggregate.solved_models;
      }
      if (window.optimization.accepted) {
        ++aggregate.accepted_models;
        ++accepted_in_sweep;
        aggregate.accepted_cross_stripe_components +=
            window.optimization.cross_stripe_component_count;
        const double hpwl_improvement =
            window.optimization.affected_hpwl_before -
            window.optimization.affected_hpwl_after;
        if (window.optimization.cross_stripe_component_count > 0) {
          ++aggregate.accepted_cross_stripe_models;
          aggregate.cross_stripe_hpwl_improvement += hpwl_improvement;
        } else {
          aggregate.local_hpwl_improvement += hpwl_improvement;
        }
      }
      aggregate.solver_wall_time_seconds +=
          window.optimization.solver_wall_time_seconds;
      aggregate.windows.push_back(std::move(window));

      const double elapsed_seconds =
          std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                        start_time)
              .count();
      if (elapsed_seconds >= config_.maximum_total_time_seconds) {
        aggregate.time_budget_exhausted = true;
        break;
      }
    }
    ++aggregate.completed_sweeps;
    const double sweep_hpwl_after = circuit_->WeightedHPWL();
    const double relative_improvement = (sweep_hpwl_before - sweep_hpwl_after) /
                                        std::max(1.0, sweep_hpwl_before);
    if (aggregate.time_budget_exhausted || accepted_in_sweep == 0 ||
        relative_improvement <= config_.minimum_relative_improvement) {
      break;
    }
  }
  aggregate.hpwl_after = circuit_->WeightedHPWL();
  return aggregate;
}

}  // namespace dali
