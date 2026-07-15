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
#include "dali/placer/well_legalizer/ortools_gridded_stripe_optimizer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#include "dali/common/helper.h"
#include "dali/placer/well_legalizer/exact_gridded_stripe_model_builder.h"
#include "dali/placer/well_legalizer/ortools_compact_gridded_legalizer.h"

namespace dali {

OrToolsGriddedStripeOptimizer::OrToolsGriddedStripeOptimizer(
    Circuit* circuit, const OrToolsGriddedStripeOptimizerConfig& config)
    : circuit_(circuit), config_(config) {
  DaliExpects(circuit_ != nullptr,
              "OR-Tools stripe optimizer requires a circuit");
  DaliExpects(config_.maximum_time_seconds_per_stripe > 0.0,
              "Stripe solve time must be positive");
  DaliExpects(config_.maximum_total_time_seconds > 0.0,
              "Stripe total time budget must be positive");
  DaliExpects(config_.minimum_relative_improvement >= 0.0,
              "Stripe convergence threshold must be non-negative");
  DaliExpects(config_.maximum_sweeps > 0,
              "Stripe optimizer must allow at least one sweep");
  DaliExpects(config_.number_of_workers > 0,
              "Stripe optimizer worker count must be positive");
  DaliExpects(config_.net_ignore_threshold >= 2,
              "Stripe net ignore threshold must be at least two");
  DaliExpects(
      config_.minimum_p_well_height >= 0 && config_.minimum_n_well_height >= 0,
      "Stripe minimum well heights must be non-negative");
  DaliExpects(config_.target_components_per_model >= 0,
              "Stripe model component target must be non-negative");
  DaliExpects(config_.maximum_components_per_model > 0,
              "Stripe model component limit must be positive");
  DaliExpects(config_.target_components_per_model == 0 ||
                  config_.maximum_components_per_model >=
                      config_.target_components_per_model,
              "Stripe model component limit must cover its target");
}

std::vector<std::pair<int, int>> OrToolsGriddedStripeOptimizer::BuildRowBands(
    const Stripe& stripe) const {
  std::vector<const GriddedRow*> rows;
  rows.reserve(stripe.gridded_rows_.size());
  for (const GriddedRow& row : stripe.gridded_rows_) rows.push_back(&row);
  std::sort(rows.begin(), rows.end(),
            [](const GriddedRow* lhs, const GriddedRow* rhs) {
              return lhs->LLY() < rhs->LLY();
            });
  if (rows.empty()) return {};
  if (config_.target_components_per_model == 0) {
    return {{0, static_cast<int>(rows.size()) - 1}};
  }

  struct ComponentExtent {
    int first_row = -1;
    int last_row = -1;
  };
  std::unordered_map<int, ComponentExtent> extents;
  for (int row_index = 0; row_index < static_cast<int>(rows.size());
       ++row_index) {
    for (const Component* component : rows[row_index]->Components()) {
      auto [extent, inserted] = extents.emplace(
          component->Id(), ComponentExtent{row_index, row_index});
      if (!inserted) {
        extent->second.first_row =
            std::min(extent->second.first_row, row_index);
        extent->second.last_row = std::max(extent->second.last_row, row_index);
      }
    }
  }

  std::vector<bool> safe_boundary(rows.size() + 1, true);
  for (const auto& entry : extents) {
    const ComponentExtent& extent = entry.second;
    for (int boundary = extent.first_row + 1; boundary <= extent.last_row;
         ++boundary) {
      safe_boundary[boundary] = false;
    }
  }

  auto component_count = [&](int first_row, int end_row) {
    std::unordered_set<int> component_ids;
    for (int row_index = first_row; row_index < end_row; ++row_index) {
      for (const Component* component : rows[row_index]->Components()) {
        component_ids.insert(component->Id());
      }
    }
    return static_cast<int>(component_ids.size());
  };

  std::vector<std::pair<int, int>> bands;
  int first_boundary = 0;
  const int final_boundary = static_cast<int>(rows.size());
  while (first_boundary < final_boundary) {
    int end_boundary = final_boundary;
    for (int candidate = first_boundary + 1; candidate <= final_boundary;
         ++candidate) {
      if (!safe_boundary[candidate]) continue;
      end_boundary = candidate;
      if (component_count(first_boundary, candidate) >=
          config_.target_components_per_model) {
        break;
      }
    }

    const int count = component_count(first_boundary, end_boundary);
    if (count > 0 && count <= config_.maximum_components_per_model) {
      bands.emplace_back(first_boundary, end_boundary - 1);
    }
    if (end_boundary == final_boundary) break;

    const int desired_boundary =
        first_boundary + std::max(1, (end_boundary - first_boundary) / 2);
    int next_boundary = end_boundary;
    for (int candidate = desired_boundary; candidate < end_boundary;
         ++candidate) {
      if (safe_boundary[candidate]) {
        next_boundary = candidate;
        break;
      }
    }
    first_boundary = next_boundary;
  }
  return bands;
}

double OrToolsGriddedStripeOptimizer::AffectedNetHpwl(
    const std::vector<int>& net_ids, bool apply_fanout_cutoff) const {
  double hpwl = 0.0;
  for (int net_id : net_ids) {
    DaliExpects(
        net_id >= 0 && net_id < static_cast<int>(circuit_->Nets().size()),
        "Stripe component refers to an unknown net");
    const Net& net = circuit_->Nets()[net_id];
    if (net.PinCnt() < 2 || net.Weight() <= 0.0) continue;
    if (apply_fanout_cutoff &&
        net.PinCnt() >= static_cast<size_t>(config_.net_ignore_threshold)) {
      continue;
    }
    hpwl += circuit_->NetWeightedHPWL(net_id);
  }
  return hpwl;
}

double OrToolsGriddedStripeOptimizer::EstimateIndependentPinXHpwlHeadroom(
    const ExactGriddedLegalizationModel& model) const {
  std::unordered_map<int, const ExactGriddedCell*> cells_by_id;
  for (const ExactGriddedCell& cell : model.cells) {
    cells_by_id.emplace(cell.component_id, &cell);
  }
  std::unordered_map<int, const ExactGriddedStripe*> stripes_by_id;
  for (const ExactGriddedStripe& stripe : model.stripes) {
    stripes_by_id.emplace(stripe.stripe_id, &stripe);
  }

  double headroom = 0.0;
  for (const ExactGriddedNet& net : model.nets) {
    if (net.pins.size() < 2 || net.weight <= 0.0) continue;

    double current_minimum = std::numeric_limits<double>::infinity();
    double current_maximum = -std::numeric_limits<double>::infinity();
    double maximum_lower_bound = -std::numeric_limits<double>::infinity();
    double minimum_upper_bound = std::numeric_limits<double>::infinity();
    for (const ExactGriddedNetPin& pin : net.pins) {
      double current_x = pin.fixed_x;
      double lower_bound = pin.fixed_x;
      double upper_bound = pin.fixed_x;
      if (pin.component_id >= 0) {
        const auto cell = cells_by_id.find(pin.component_id);
        DaliExpects(cell != cells_by_id.end(),
                    "Exact row-band net refers to an unknown component");
        const auto stripe = stripes_by_id.find(cell->second->initial_stripe_id);
        DaliExpects(stripe != stripes_by_id.end(),
                    "Exact row-band component refers to an unknown stripe");
        const double offset =
            cell->second->initial_is_flipped ? pin.offset_x_fs : pin.offset_x_n;
        current_x = cell->second->initial_x + offset;
        lower_bound =
            stripe->second->lx + stripe->second->left_boundary_margin + offset;
        upper_bound = stripe->second->ux -
                      stripe->second->right_boundary_margin -
                      cell->second->width + offset;
        DaliExpects(lower_bound <= upper_bound,
                    "Exact row-band pin has an empty X interval");
      }
      current_minimum = std::min(current_minimum, current_x);
      current_maximum = std::max(current_maximum, current_x);
      maximum_lower_bound = std::max(maximum_lower_bound, lower_bound);
      minimum_upper_bound = std::min(minimum_upper_bound, upper_bound);
    }

    const double current_span = current_maximum - current_minimum;
    const double relaxed_span =
        std::max(0.0, maximum_lower_bound - minimum_upper_bound);
    headroom += net.weight * model.distance_scale_x *
                std::max(0.0, current_span - relaxed_span);
  }
  return headroom;
}

OrToolsGriddedStripeOptimizerResult OrToolsGriddedStripeOptimizer::Optimize(
    std::vector<StripeColumn>* columns) const {
  DaliExpects(columns != nullptr, "Stripe-column list must not be null");

  OrToolsGriddedStripeOptimizerResult aggregate;
  aggregate.available = OrToolsCompactGriddedLegalizer::IsAvailable();
  aggregate.hpwl_before = circuit_->WeightedHPWL();
  aggregate.hpwl_after = aggregate.hpwl_before;
  if (!aggregate.available) return aggregate;

  struct StripeTarget {
    int column_index = -1;
    int stripe_index = -1;
    int model_stripe_id = -1;
    int first_row_index = -1;
    int last_row_index = -1;
    double initial_priority_headroom = 0.0;
    Stripe* stripe = nullptr;
  };

  ExactGriddedStripeModelBuilderConfig builder_config;
  builder_config.net_ignore_threshold = config_.net_ignore_threshold;
  builder_config.minimum_p_well_height = config_.minimum_p_well_height;
  builder_config.minimum_n_well_height = config_.minimum_n_well_height;
  ExactGriddedStripeModelBuilder builder(circuit_, builder_config);

  std::vector<StripeTarget> targets;
  int next_stripe_id = 0;
  for (int column_index = 0; column_index < static_cast<int>(columns->size());
       ++column_index) {
    StripeColumn& column = (*columns)[column_index];
    for (int stripe_index = 0;
         stripe_index < static_cast<int>(column.stripe_list_.size());
         ++stripe_index) {
      Stripe& stripe = column.stripe_list_[stripe_index];
      const int model_stripe_id = next_stripe_id++;
      for (const auto& [first_row, last_row] : BuildRowBands(stripe)) {
        const ExactGriddedStripeBuildResult initial_build =
            builder.BuildRowBand(&stripe, model_stripe_id, first_row, last_row);
        const double initial_priority_headroom =
            EstimateIndependentPinXHpwlHeadroom(initial_build.model);
        targets.push_back({column_index, stripe_index, model_stripe_id,
                           first_row, last_row, initial_priority_headroom,
                           &stripe});
      }
    }
  }
  // Rank once, then rebuild each model immediately before solving so accepted
  // changes from earlier overlapping bands remain visible.
  std::stable_sort(targets.begin(), targets.end(),
                   [](const StripeTarget& lhs, const StripeTarget& rhs) {
                     return lhs.initial_priority_headroom >
                            rhs.initial_priority_headroom;
                   });

  OrToolsCompactGriddedLegalizer solver;
  const auto start_time = std::chrono::steady_clock::now();

  for (int sweep = 0; sweep < config_.maximum_sweeps; ++sweep) {
    const double sweep_hpwl_before = circuit_->WeightedHPWL();
    int accepted_in_sweep = 0;
    for (int target_offset = 0;
         target_offset < static_cast<int>(targets.size()); ++target_offset) {
      const int target_index =
          sweep % 2 == 0 ? target_offset
                         : static_cast<int>(targets.size()) - 1 - target_offset;
      const StripeTarget& target = targets[target_index];
      ExactGriddedStripeBuildResult build =
          builder.BuildRowBand(target.stripe, target.model_stripe_id,
                               target.first_row_index, target.last_row_index);
      if (build.model.cells.empty()) continue;

      OrToolsGriddedStripeSolveResult stripe_result;
      stripe_result.sweep = sweep;
      stripe_result.column_index = target.column_index;
      stripe_result.stripe_index = target.stripe_index;
      stripe_result.first_row_index = target.first_row_index;
      stripe_result.last_row_index = target.last_row_index;
      stripe_result.component_count =
          static_cast<int>(build.model.cells.size());
      stripe_result.net_count = static_cast<int>(build.model.nets.size());
      stripe_result.initial_priority_headroom =
          target.initial_priority_headroom;
      stripe_result.modeled_hpwl_before =
          AffectedNetHpwl(build.affected_net_ids, true);
      stripe_result.affected_hpwl_before =
          AffectedNetHpwl(build.affected_net_ids, false);
      stripe_result.modeled_hpwl_after = stripe_result.modeled_hpwl_before;
      stripe_result.affected_hpwl_after = stripe_result.affected_hpwl_before;
      ++aggregate.attempted_models;

      ExactGriddedLegalizationConfig solver_config;
      solver_config.maximum_time_seconds =
          config_.maximum_time_seconds_per_stripe;
      solver_config.number_of_workers = config_.number_of_workers;
      solver_config.maximum_row_displacement = 0;
      solver_config.fix_row_geometry = true;
      solver_config.use_presolve = true;
      solver_config.use_solution_hint = config_.use_solution_hint;
      solver_config.validate_solution_hint = true;
      ExactGriddedLegalizationResult solution =
          solver.Solve(build.model, solver_config);
      stripe_result.status = solution.status;
      stripe_result.solver_wall_time_seconds = solution.wall_time_seconds;
      stripe_result.best_objective_bound = solution.best_objective_bound;
      stripe_result.relative_gap = solution.relative_gap;
      stripe_result.model_variable_count = solution.model_variable_count;
      stripe_result.model_constraint_count = solution.model_constraint_count;
      aggregate.solver_wall_time_seconds += solution.wall_time_seconds;

      if (solution.HasSolution()) {
        ++aggregate.solved_models;
        std::unordered_map<int, Component*> components_by_id;
        for (Component* component : build.components) {
          components_by_id.emplace(component->Id(), component);
        }
        std::unordered_map<int, const ExactGriddedCell*> model_cells_by_id;
        for (const ExactGriddedCell& cell : build.model.cells) {
          model_cells_by_id.emplace(cell.component_id, &cell);
        }

        bool preserves_fixed_assignment =
            solution.cells.size() == build.model.cells.size();
        for (const ExactGriddedCellPlacement& placement : solution.cells) {
          const auto model_cell =
              model_cells_by_id.find(placement.component_id);
          if (model_cell == model_cells_by_id.end()) {
            preserves_fixed_assignment = false;
            break;
          }
          const ExactGriddedCell& cell = *model_cell->second;
          if (placement.stripe_id != cell.initial_stripe_id ||
              placement.row_index != cell.initial_start_row ||
              placement.y != cell.initial_y ||
              placement.is_flipped != cell.initial_is_flipped) {
            preserves_fixed_assignment = false;
            break;
          }
        }

        std::vector<double> original_x;
        original_x.reserve(build.components.size());
        for (Component* component : build.components) {
          original_x.push_back(component->LLX());
        }
        if (preserves_fixed_assignment) {
          for (const ExactGriddedCellPlacement& placement : solution.cells) {
            components_by_id.at(placement.component_id)->SetLLX(placement.x);
          }
        }

        const bool rows_are_legal =
            preserves_fixed_assignment &&
            std::all_of(build.rows.begin(), build.rows.end(),
                        [](const GriddedRow* row) {
                          return row->HasLegalComponentPlacement();
                        });
        if (rows_are_legal) {
          stripe_result.modeled_hpwl_after =
              AffectedNetHpwl(build.affected_net_ids, true);
          stripe_result.affected_hpwl_after =
              AffectedNetHpwl(build.affected_net_ids, false);
        }
        const double modeled_tolerance =
            1e-9 * std::max(1.0, stripe_result.modeled_hpwl_before);
        const double affected_tolerance =
            1e-9 * std::max(1.0, stripe_result.affected_hpwl_before);
        const bool improves_modeled_hpwl =
            stripe_result.modeled_hpwl_after + modeled_tolerance <
            stripe_result.modeled_hpwl_before;
        const bool preserves_full_hpwl =
            stripe_result.affected_hpwl_after <=
            stripe_result.affected_hpwl_before + affected_tolerance;
        if (rows_are_legal && improves_modeled_hpwl && preserves_full_hpwl) {
          stripe_result.accepted = true;
          ++aggregate.accepted_models;
          ++accepted_in_sweep;
          for (GriddedRow* row : build.rows) {
            std::sort(
                row->Components().begin(), row->Components().end(),
                [](const Component* lhs, const Component* rhs) {
                  return (lhs->LLX() < rhs->LLX()) ||
                         (lhs->LLX() == rhs->LLX() && lhs->Id() < rhs->Id());
                });
          }
        } else {
          for (size_t component_index = 0;
               component_index < build.components.size(); ++component_index) {
            build.components[component_index]->SetLLX(
                original_x[component_index]);
          }
          stripe_result.modeled_hpwl_after = stripe_result.modeled_hpwl_before;
          stripe_result.affected_hpwl_after =
              stripe_result.affected_hpwl_before;
        }
      }
      aggregate.stripes.push_back(stripe_result);

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
