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

/**
 * @file
 * Reassigns components between nearby rows to reduce vertical wirelength.
 *
 * Works in windows of adjacent rows so each decision stays bounded. A candidate
 * reassignment is scored on the net wirelength it would save against the row
 * capacity it would consume, and taken only when the rows involved still fit.
 */
#include "dali/placer/well_legalizer/gridded_vertical_hpwl_row_optimizer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include "dali/common/helper.h"
#include "dali/common/placement_metrics.h"
#include "dali/placer/well_legalizer/gridded_detailed_placer.h"
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
  DaliExpects(config_.first_row_offset >= 0 &&
                  config_.first_row_offset < config_.row_stride,
              "Row-window offset must fall within one stride");
  DaliExpects(config_.net_ignore_threshold >= 2,
              "Net ignore threshold must be at least two");
  DaliExpects(config_.maximum_time_seconds_per_window > 0.0 &&
                  config_.maximum_total_time_seconds > 0.0 &&
                  config_.number_of_workers > 0 && config_.coordinate_scale > 0,
              "Row-assignment solve limits must be positive");
  DaliExpects(config_.minimum_hpwl_improvement >= 0.0,
              "Minimum HPWL improvement cannot be negative");
  DaliExpects(config_.maximum_local_closure_windows > 0,
              "Local closure window cap must be positive");
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

  // Later sweeps shift the schedule by one row. A nonzero initial offset
  // selects a complementary partition without applying an earlier sweep.
  const int first_row = (config_.first_row_offset + sweep) % config_.row_stride;
  std::vector<Window> windows;
  for (int begin = first_row;
       begin + config_.rows_per_window <= static_cast<int>(rows.size());
       begin += config_.row_stride) {
    Window window;
    window.stripe = stripe;
    window.first_row_index = begin;
    window.rows.insert(window.rows.end(), rows.begin() + begin,
                       rows.begin() + begin + config_.rows_per_window);
    windows.push_back(std::move(window));
  }
  return windows;
}

double GriddedVerticalHpwlRowOptimizer::WindowPotential(
    const Window& window) const {
  std::unordered_set<int> net_ids;
  for (const GriddedRow* row : window.rows) {
    for (const Component* component : row->Components()) {
      net_ids.insert(component->NetList().begin(), component->NetList().end());
    }
  }

  double potential = 0.0;
  for (int net_id : net_ids) {
    const Net& net = circuit_->Nets().at(net_id);
    if (net.PinCnt() < 2 ||
        net.PinCnt() >= static_cast<size_t>(config_.net_ignore_threshold) ||
        net.Weight() <= 0.0) {
      continue;
    }
    potential += circuit_->NetWeightedHPWL(net_id);
  }
  return potential;
}

double GriddedVerticalHpwlRowOptimizer::ComponentLlyInRow(
    const Component& component, const GriddedRow& row) const {
  const Macro* macro = component.MacroPtr();
  if (row.IsOrientN()) {
    return row.LLY() + row.PHeight() - macro->FirstPwellHeight();
  }
  return row.LLY() + row.NHeight() - macro->FirstNwellHeight();
}

void GriddedVerticalHpwlRowOptimizer::RunLocalDetailedClosure(
    const Window& window) const {
  GriddedDetailedPlacer detailed_placer;
  detailed_placer.SetCircuit(circuit_);
  detailed_placer.SetRows(window.rows);
  detailed_placer.SetEnableRelocation(true);
  detailed_placer.SetNetIgnoreThreshold(config_.net_ignore_threshold);

  // Candidate previews are private trials. Their per-stage metrics must not
  // overwrite the metrics reported for the selected placement flow.
  ScopedPlacementMetricSuppression suppress_metrics;
  detailed_placer.RunOneRoundClosure();
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
  double baseline_closure_gain = 0.0;
  if (config_.compare_local_detailed_closure) {
    RunLocalDetailedClosure(window);
    baseline_closure_gain = transaction.HpwlImprovement();
    transaction.Restore();
  }
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

  bool improves = transaction.ImprovesHpwl(config_.minimum_hpwl_improvement);
  double closure_advantage = 0.0;
  if (legal && config_.compare_local_detailed_closure) {
    RunLocalDetailedClosure(window);
    const double candidate_closure_gain = transaction.HpwlImprovement();
    result->local_closure_baseline_gain += baseline_closure_gain;
    result->local_closure_candidate_gain += candidate_closure_gain;
    closure_advantage = candidate_closure_gain - baseline_closure_gain;
    improves = closure_advantage > config_.minimum_hpwl_improvement;
    if (!improves) ++result->closure_rejected_windows;
  }
  if (!legal || !improves) {
    transaction.Restore();
    return;
  }

  ++result->accepted_windows;
  result->reassigned_components += reassigned;
  if (config_.compare_local_detailed_closure) {
    LOG(info) << "    accepted row-assignment window: stripe x="
              << window.stripe->LLX() << ", rows " << window.first_row_index
              << "-" << window.first_row_index + config_.rows_per_window - 1
              << ", closure advantage=" << closure_advantage << "um\n";
  }
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
    std::vector<Window> windows;
    for (StripeColumn& column : *columns) {
      for (Stripe& stripe : column.stripe_list_) {
        std::vector<Window> stripe_windows = BuildWindows(&stripe, sweep);
        windows.insert(windows.end(),
                       std::make_move_iterator(stripe_windows.begin()),
                       std::make_move_iterator(stripe_windows.end()));
      }
    }
    if (config_.compare_local_detailed_closure &&
        windows.size() >
            static_cast<size_t>(config_.maximum_local_closure_windows)) {
      std::sort(windows.begin(), windows.end(),
                [this](const Window& lhs, const Window& rhs) {
                  const double lhs_potential = WindowPotential(lhs);
                  const double rhs_potential = WindowPotential(rhs);
                  if (lhs_potential != rhs_potential) {
                    return lhs_potential > rhs_potential;
                  }
                  if (lhs.stripe->LLX() != rhs.stripe->LLX()) {
                    return lhs.stripe->LLX() < rhs.stripe->LLX();
                  }
                  return lhs.first_row_index < rhs.first_row_index;
                });
      result.skipped_closure_windows += static_cast<int>(windows.size()) -
                                        config_.maximum_local_closure_windows;
      windows.resize(config_.maximum_local_closure_windows);
    }
    for (const Window& window : windows) {
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
    ++result.completed_sweeps;
  }
  result.hpwl_after = circuit_->WeightedHPWL();
  return result;
}

}  // namespace dali
