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
#include "dali/placer/well_legalizer/ortools_gridded_row_optimizer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_set>

#include "dali/common/helper.h"

namespace dali {

OrToolsGriddedRowOptimizer::OrToolsGriddedRowOptimizer(
    Circuit* circuit, const OrToolsGriddedRowOptimizerConfig& config)
    : circuit_(circuit), config_(config) {
  DaliExpects(circuit_ != nullptr,
              "OR-Tools gridded-row optimizer requires a circuit");
  DaliExpects(config_.maximum_time_seconds_per_model > 0.0,
              "OR-Tools model solve time must be positive");
  DaliExpects(config_.maximum_total_time_seconds > 0.0,
              "OR-Tools total time budget must be positive");
  DaliExpects(config_.target_components_per_model > 0,
              "OR-Tools model component target must be positive");
  DaliExpects(config_.number_of_workers > 0,
              "OR-Tools worker count must be positive");
  DaliExpects(config_.net_ignore_threshold >= 2,
              "OR-Tools net ignore threshold must be at least two");
}

FixedRowDisplacementModel OrToolsGriddedRowOptimizer::BuildRowBatchModel(
    const std::vector<const GriddedRow*>& rows,
    std::unordered_map<int, Component*>* components_by_id,
    std::vector<int>* affected_net_ids) const {
  DaliExpects(components_by_id != nullptr,
              "Component lookup output must not be null");
  DaliExpects(affected_net_ids != nullptr,
              "Affected-net output must not be null");
  components_by_id->clear();
  affected_net_ids->clear();

  FixedRowDisplacementModel model;
  std::unordered_map<int, size_t> variable_indices;
  std::unordered_set<int> modeled_component_ids;

  for (const GriddedRow* row : rows) {
    DaliExpects(row != nullptr, "Gridded-row batch contains a null row");
    std::vector<Component*> components = row->Components();
    std::sort(components.begin(), components.end(),
              [](const Component* lhs, const Component* rhs) {
                return (lhs->LLX() < rhs->LLX()) ||
                       (lhs->LLX() == rhs->LLX() && lhs->Id() < rhs->Id());
              });

    FixedRowComponentSequence sequence;
    sequence.component_ids.reserve(components.size());
    for (Component* component : components) {
      const int component_id = component->Id();
      const int minimum_x = row->LLX() + row->LeftBoundaryMargin();
      const int maximum_x =
          row->URX() - row->RightBoundaryMargin() - component->Width();
      auto [index_it, inserted] =
          variable_indices.emplace(component_id, model.components.size());
      if (inserted) {
        model.components.push_back(
            {component_id, component->Width(),
             static_cast<int>(std::llround(component->LLX())), minimum_x,
             maximum_x});
        components_by_id->emplace(component_id, component);
        affected_net_ids->insert(affected_net_ids->end(),
                                 component->NetList().begin(),
                                 component->NetList().end());
      } else {
        FixedRowComponentVariable& variable =
            model.components[index_it->second];
        variable.minimum_x = std::max(variable.minimum_x, minimum_x);
        variable.maximum_x = std::min(variable.maximum_x, maximum_x);
      }
      sequence.component_ids.push_back(component_id);
    }
    if (!sequence.component_ids.empty()) {
      model.rows.push_back(std::move(sequence));
    }
  }

  std::sort(affected_net_ids->begin(), affected_net_ids->end());
  affected_net_ids->erase(
      std::unique(affected_net_ids->begin(), affected_net_ids->end()),
      affected_net_ids->end());
  for (int net_id : *affected_net_ids) {
    DaliExpects(
        net_id >= 0 && net_id < static_cast<int>(circuit_->Nets().size()),
        "Component refers to a net outside the circuit net list");
    Net& net = circuit_->Nets()[net_id];
    if (net.PinCnt() < 2 ||
        net.PinCnt() >= static_cast<size_t>(config_.net_ignore_threshold) ||
        net.Weight() <= 0.0) {
      continue;
    }

    FixedRowNet model_net;
    model_net.weight = net.Weight();
    model_net.pins.reserve(net.ComponentPins().size());
    bool has_variable_pin = false;
    for (const NetPin& pin : net.ComponentPins()) {
      if (variable_indices.count(pin.ComponentId()) != 0) {
        model_net.pins.push_back({pin.ComponentId(), pin.OffsetX(), 0.0});
        modeled_component_ids.insert(pin.ComponentId());
        has_variable_pin = true;
      } else {
        model_net.pins.push_back({-1, 0.0, pin.AbsX()});
      }
    }
    if (has_variable_pin) model.nets.push_back(std::move(model_net));
  }

  // Components connected only through ignored or degenerate nets do not
  // participate in the HPWL objective. Fixing them at their incoming legal
  // coordinate prevents arbitrary solver movement without displacement vars.
  for (FixedRowComponentVariable& component : model.components) {
    if (modeled_component_ids.count(component.component_id) == 0) {
      component.minimum_x = component.initial_x;
      component.maximum_x = component.initial_x;
    }
  }
  return model;
}

double OrToolsGriddedRowOptimizer::AffectedNetHpwl(
    const std::vector<int>& net_ids) const {
  double hpwl = 0.0;
  for (int net_id : net_ids) hpwl += circuit_->NetWeightedHPWL(net_id);
  return hpwl;
}

void OrToolsGriddedRowOptimizer::OptimizeRowBatch(
    const std::vector<const GriddedRow*>& rows,
    const FixedRowDisplacementSolverConfig& solver_config,
    const OrToolsFixedRowDisplacementOptimizer& optimizer,
    OrToolsGriddedRowOptimizerResult* aggregate) const {
  DaliExpects(aggregate != nullptr, "Optimizer result must not be null");

  std::unordered_map<int, Component*> components_by_id;
  std::vector<int> affected_net_ids;
  FixedRowDisplacementModel model =
      BuildRowBatchModel(rows, &components_by_id, &affected_net_ids);
  if (model.components.empty()) return;
  ++aggregate->attempted_models;

  std::vector<double> original_x;
  original_x.reserve(model.components.size());
  for (const FixedRowComponentVariable& variable : model.components) {
    original_x.push_back(components_by_id.at(variable.component_id)->LLX());
  }
  const double affected_hpwl_before = AffectedNetHpwl(affected_net_ids);
  FixedRowDisplacementResult result = optimizer.Solve(model, solver_config);
  aggregate->solver_wall_time_seconds += result.wall_time_seconds;
  if (!result.HasSolution()) return;
  ++aggregate->solved_models;

  for (const FixedRowComponentLocation& location : result.locations) {
    components_by_id.at(location.component_id)->SetLLX(location.x);
  }
  const double affected_hpwl_after = AffectedNetHpwl(affected_net_ids);
  const double tolerance = 1e-9 * std::max(1.0, std::abs(affected_hpwl_before));
  const bool legal = std::all_of(
      rows.begin(), rows.end(),
      [](const GriddedRow* row) { return row->HasLegalComponentPlacement(); });
  if (!legal || affected_hpwl_after > affected_hpwl_before + tolerance) {
    for (size_t index = 0; index < model.components.size(); ++index) {
      components_by_id.at(model.components[index].component_id)
          ->SetLLX(original_x[index]);
    }
    return;
  }
  ++aggregate->accepted_models;
  if (affected_hpwl_after + tolerance < affected_hpwl_before) {
    ++aggregate->improved_models;
  }
}

OrToolsGriddedRowOptimizerResult OrToolsGriddedRowOptimizer::Optimize(
    std::vector<StripeColumn>* columns) const {
  DaliExpects(columns != nullptr, "Stripe-column list must not be null");

  OrToolsGriddedRowOptimizerResult aggregate;
  aggregate.available = OrToolsFixedRowDisplacementOptimizer::IsAvailable();
  aggregate.hpwl_before = circuit_->WeightedHPWL();
  aggregate.hpwl_after = aggregate.hpwl_before;
  if (!aggregate.available) return aggregate;

  FixedRowDisplacementSolverConfig solver_config;
  solver_config.maximum_time_seconds = config_.maximum_time_seconds_per_model;
  solver_config.number_of_workers = config_.number_of_workers;
  solver_config.displacement_weight = config_.displacement_weight;
  solver_config.weighted_hpwl_x_weight = 1.0;

  OrToolsFixedRowDisplacementOptimizer optimizer;
  const auto start_time = std::chrono::steady_clock::now();
  bool stop = false;
  for (StripeColumn& column : *columns) {
    for (Stripe& stripe : column.stripe_list_) {
      std::vector<const GriddedRow*> row_batch;
      int component_count = 0;
      for (const GriddedRow& row : stripe.gridded_rows_) {
        int row_component_count = static_cast<int>(row.Components().size());
        if (!row_batch.empty() && component_count + row_component_count >
                                      config_.target_components_per_model) {
          OptimizeRowBatch(row_batch, solver_config, optimizer, &aggregate);
          row_batch.clear();
          component_count = 0;
          const double elapsed_seconds =
              std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                            start_time)
                  .count();
          if (elapsed_seconds >= config_.maximum_total_time_seconds) {
            aggregate.time_budget_exhausted = true;
            stop = true;
            break;
          }
        }
        row_batch.push_back(&row);
        component_count += row_component_count;
      }
      if (!stop && !row_batch.empty()) {
        OptimizeRowBatch(row_batch, solver_config, optimizer, &aggregate);
        const double elapsed_seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                          start_time)
                .count();
        if (elapsed_seconds >= config_.maximum_total_time_seconds) {
          aggregate.time_budget_exhausted = true;
          stop = true;
        }
      }
      if (stop) break;
    }
    if (stop) break;
  }
  aggregate.hpwl_after = circuit_->WeightedHPWL();
  return aggregate;
}

}  // namespace dali
