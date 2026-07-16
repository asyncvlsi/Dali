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
#include "dali/placer/well_legalizer/gridded_vertical_hpwl_row_optimizer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "dali/common/helper.h"
#include "dali/placer/well_legalizer/gridded_row_assignment_transaction.h"

namespace dali {

GriddedVerticalHpwlRowOptimizer::GriddedVerticalHpwlRowOptimizer(
    Circuit* circuit, const GriddedVerticalHpwlRowOptimizerConfig& config)
    : circuit_(circuit), config_(config) {
  DaliExpects(circuit_ != nullptr,
              "Vertical-HPWL row optimization requires a circuit");
  DaliExpects(config_.rows_per_window > 0 && config_.maximum_sweeps > 0,
              "Row-window size and iteration controls must be positive");
  DaliExpects(
      config_.row_stride > 0 && config_.row_stride <= config_.rows_per_window,
      "Row windows require a positive stride no larger than a window");
  DaliExpects(config_.net_ignore_threshold >= 2,
              "Net ignore threshold must be at least two");
  DaliExpects(config_.maximum_time_seconds_per_window > 0.0 &&
                  config_.maximum_total_time_seconds > 0.0 &&
                  config_.number_of_workers > 0 && config_.coordinate_scale > 0,
              "Row-assignment solve limits must be positive");
  DaliExpects(config_.minimum_hpwl_improvement >= 0.0,
              "Minimum HPWL improvement cannot be negative");
}

std::vector<GriddedVerticalHpwlRowOptimizer::Window>
GriddedVerticalHpwlRowOptimizer::BuildWindows(Stripe* stripe, int sweep) const {
  DaliExpects(stripe != nullptr, "Cannot build row windows from a null stripe");
  std::vector<GriddedRow*> rows;
  rows.reserve(stripe->gridded_rows_.size());
  for (GriddedRow& row : stripe->gridded_rows_) rows.push_back(&row);
  std::sort(rows.begin(), rows.end(),
            [](const GriddedRow* lhs, const GriddedRow* rhs) {
              return lhs->LLY() < rhs->LLY();
            });

  // Later sweeps shift an overlapping schedule by one stride. The retained
  // default is one sweep, so production experiments reproduce even row pairs.
  const int first_row = sweep == 0 ? 0 : sweep % config_.row_stride;
  std::vector<Window> windows;
  for (int begin = first_row;
       begin + config_.rows_per_window <= static_cast<int>(rows.size());
       begin += config_.row_stride) {
    Window window;
    window.stripe = stripe;
    window.rows.insert(window.rows.end(), rows.begin() + begin,
                       rows.begin() + begin + config_.rows_per_window);
    windows.push_back(std::move(window));
  }
  return windows;
}

double GriddedVerticalHpwlRowOptimizer::ComponentLlyInRow(
    const Component& component, const GriddedRow& row) const {
  const Macro* macro = component.MacroPtr();
  if (row.IsOrientN()) {
    return row.LLY() + row.PHeight() - macro->FirstPwellHeight();
  }
  return row.LLY() + row.NHeight() - macro->FirstNwellHeight();
}

void GriddedVerticalHpwlRowOptimizer::OptimizeWindow(
    const Window& window, GriddedVerticalHpwlRowOptimizerResult* result) const {
  DaliExpects(result != nullptr, "Row-assignment result must not be null");

  std::unordered_map<int, int> stripe_occurrences;
  for (const GriddedRow& row : window.stripe->gridded_rows_) {
    for (const Component* component : row.Components()) {
      ++stripe_occurrences[component->Id()];
    }
  }

  std::unordered_map<int, Component*> components_by_id;
  std::unordered_map<int, GriddedRow*> initial_rows;
  std::unordered_map<int, double2d> initial_locations;
  std::unordered_map<int, int> row_fixed_width;
  for (GriddedRow* row : window.rows) {
    row_fixed_width[row->LLY()] = 0;
    for (Component* component : row->Components()) {
      if (stripe_occurrences[component->Id()] != 1) {
        row_fixed_width[row->LLY()] += component->Width();
        continue;
      }
      components_by_id.emplace(component->Id(), component);
      initial_rows.emplace(component->Id(), row);
      const auto location = row->InitLocations().find(component);
      initial_locations.emplace(
          component->Id(), location == row->InitLocations().end()
                               ? double2d(component->LLX(), component->LLY())
                               : location->second);
    }
  }
  if (components_by_id.empty()) return;

  VerticalHpwlRowAssignmentModel model;
  std::unordered_map<int, GriddedRow*> rows_by_id;
  for (int row_id = 0; row_id < static_cast<int>(window.rows.size());
       ++row_id) {
    GriddedRow* row = window.rows[row_id];
    rows_by_id.emplace(row_id, row);
    model.rows.push_back(
        {row_id, row->UsableWidth() - row_fixed_width.at(row->LLY())});
  }

  std::unordered_map<int, size_t> model_component_indices;
  for (const auto& [component_id, component] : components_by_id) {
    VerticalHpwlRowComponent variable;
    variable.component_id = component_id;
    variable.width = component->Width();
    for (int row_id = 0; row_id < static_cast<int>(window.rows.size());
         ++row_id) {
      GriddedRow* row = window.rows[row_id];
      if (row == initial_rows.at(component_id))
        variable.initial_row_id = row_id;
      const Macro* macro = component->MacroPtr();
      if (macro->FirstPwellHeight() <= row->PHeight() &&
          macro->FirstNwellHeight() <= row->NHeight()) {
        variable.candidates.push_back(
            {row_id, ComponentLlyInRow(*component, *row)});
      }
    }
    DaliExpects(variable.initial_row_id >= 0,
                "Current legal row is absent from vertical-HPWL candidates");
    const bool initial_is_candidate =
        std::any_of(variable.candidates.begin(), variable.candidates.end(),
                    [&](const VerticalHpwlRowCandidate& candidate) {
                      return candidate.row_id == variable.initial_row_id;
                    });
    if (!initial_is_candidate) continue;
    model_component_indices.emplace(component_id, model.components.size());
    model.components.push_back(std::move(variable));
  }
  if (model.components.empty()) return;

  std::unordered_set<int> affected_net_ids;
  for (const VerticalHpwlRowComponent& component : model.components) {
    const Component* circuit_component =
        components_by_id.at(component.component_id);
    affected_net_ids.insert(circuit_component->NetList().begin(),
                            circuit_component->NetList().end());
  }
  for (int net_id : affected_net_ids) {
    Net& net = circuit_->Nets().at(net_id);
    if (net.PinCnt() < 2 ||
        net.PinCnt() >= static_cast<size_t>(config_.net_ignore_threshold) ||
        net.Weight() <= 0.0) {
      continue;
    }
    VerticalHpwlRowNet model_net;
    model_net.weight = net.Weight();
    bool has_variable_pin = false;
    for (const NetPin& pin : net.ComponentPins()) {
      const auto component_index =
          model_component_indices.find(pin.ComponentId());
      if (component_index == model_component_indices.end()) {
        model_net.pins.push_back({-1, pin.AbsY(), {}});
        continue;
      }
      has_variable_pin = true;
      VerticalHpwlRowPin model_pin;
      model_pin.component_id = pin.ComponentId();
      const VerticalHpwlRowComponent& component =
          model.components[component_index->second];
      for (const VerticalHpwlRowCandidate& candidate : component.candidates) {
        const ComponentOrient orientation =
            rows_by_id.at(candidate.row_id)->IsOrientN() ? N : FS;
        model_pin.candidate_offsets_y.push_back(
            pin.PinPtr()->OffsetY(orientation));
      }
      model_net.pins.push_back(std::move(model_pin));
    }
    if (has_variable_pin) model.nets.push_back(std::move(model_net));
  }

  ++result->attempted_windows;
  VerticalHpwlRowAssignmentConfig solver_config;
  solver_config.maximum_time_seconds = config_.maximum_time_seconds_per_window;
  solver_config.number_of_workers = config_.number_of_workers;
  solver_config.coordinate_scale = config_.coordinate_scale;
  const VerticalHpwlRowAssignmentResult solution =
      OrToolsVerticalHpwlRowAssignment().Solve(model, solver_config);
  result->solver_wall_time_seconds += solution.wall_time_seconds;
  if (!solution.HasSolution()) return;
  ++result->solved_windows;

  GriddedRowAssignmentTransaction transaction(circuit_, window.rows);
  std::unordered_map<int, int> assigned_rows;
  for (const VerticalHpwlRowLocation& assignment : solution.assignments) {
    assigned_rows.emplace(assignment.component_id, assignment.row_id);
  }
  if (assigned_rows.size() != model.components.size()) return;

  std::unordered_map<GriddedRow*, int> base_used_size;
  for (GriddedRow* row : window.rows) {
    int variable_width = 0;
    auto& components = row->Components();
    components.erase(std::remove_if(components.begin(), components.end(),
                                    [&](Component* component) {
                                      if (model_component_indices.count(
                                              component->Id()) == 0)
                                        return false;
                                      variable_width += component->Width();
                                      row->InitLocations().erase(component);
                                      return true;
                                    }),
                     components.end());
    base_used_size[row] = row->UsedSize() - variable_width;
  }

  int reassigned = 0;
  for (const VerticalHpwlRowComponent& variable : model.components) {
    Component* component = components_by_id.at(variable.component_id);
    GriddedRow* target_row =
        rows_by_id.at(assigned_rows.at(variable.component_id));
    reassigned += target_row != initial_rows.at(variable.component_id);
    component->SetOrient(target_row->IsOrientN() ? N : FS);
    component->SetLLY(ComponentLlyInRow(*component, *target_row));
    target_row->Components().push_back(component);
    target_row->InitLocations()[component] =
        initial_locations.at(variable.component_id);
  }

  bool legal = true;
  for (GriddedRow* row : window.rows) {
    int variable_width = 0;
    for (const Component* component : row->Components()) {
      if (model_component_indices.count(component->Id()) != 0) {
        variable_width += component->Width();
      }
    }
    row->SetUsedSize(base_used_size.at(row) + variable_width);
    row->LegalizeLooseX();
    legal &= row->HasLegalComponentPlacement();
  }

  if (!legal || !transaction.ImprovesHpwl(config_.minimum_hpwl_improvement)) {
    transaction.Restore();
    return;
  }
  ++result->accepted_windows;
  result->reassigned_components += reassigned;
}

GriddedVerticalHpwlRowOptimizerResult GriddedVerticalHpwlRowOptimizer::Optimize(
    std::vector<StripeColumn>* columns) const {
  DaliExpects(columns != nullptr, "Stripe-column list must not be null");
  GriddedVerticalHpwlRowOptimizerResult result;
  result.available = OrToolsVerticalHpwlRowAssignment::IsAvailable();
  result.hpwl_before = circuit_->WeightedHPWL();
  result.hpwl_after = result.hpwl_before;
  if (!result.available) return result;

  const auto start = std::chrono::steady_clock::now();
  for (int sweep = 0; sweep < config_.maximum_sweeps; ++sweep) {
    for (StripeColumn& column : *columns) {
      for (Stripe& stripe : column.stripe_list_) {
        for (const Window& window : BuildWindows(&stripe, sweep)) {
          OptimizeWindow(window, &result);
          const double elapsed = std::chrono::duration<double>(
                                     std::chrono::steady_clock::now() - start)
                                     .count();
          if (elapsed >= config_.maximum_total_time_seconds) {
            result.time_budget_exhausted = true;
            result.hpwl_after = circuit_->WeightedHPWL();
            return result;
          }
        }
      }
    }
    ++result.completed_sweeps;
  }
  result.hpwl_after = circuit_->WeightedHPWL();
  return result;
}

}  // namespace dali
