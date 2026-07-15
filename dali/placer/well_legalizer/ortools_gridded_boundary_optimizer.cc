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
#include "dali/placer/well_legalizer/ortools_gridded_boundary_optimizer.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "dali/common/helper.h"
#include "dali/placer/well_legalizer/exact_gridded_boundary_model_builder.h"
#include "dali/placer/well_legalizer/gridded_row_assignment_transaction.h"
#include "dali/placer/well_legalizer/ortools_compact_gridded_legalizer.h"

namespace dali {

OrToolsGriddedBoundaryOptimizer::OrToolsGriddedBoundaryOptimizer(
    Circuit* circuit, const OrToolsGriddedBoundaryOptimizerConfig& config)
    : circuit_(circuit), config_(config) {
  DaliExpects(circuit_ != nullptr, "Cross-stripe optimizer requires a circuit");
  DaliExpects(config_.maximum_time_seconds > 0.0,
              "Cross-stripe solve time must be positive");
  DaliExpects(config_.number_of_workers > 0,
              "Cross-stripe worker count must be positive");
  DaliExpects(config_.net_ignore_threshold >= 2,
              "Cross-stripe net ignore threshold must be at least two");
  DaliExpects(
      config_.minimum_p_well_height >= 0 && config_.minimum_n_well_height >= 0,
      "Cross-stripe minimum well heights must be non-negative");
  DaliExpects(config_.maximum_row_displacement >= -1,
              "Cross-stripe row displacement must be at least negative one");
  DaliExpects(config_.maximum_assignment_changes >= -1,
              "Cross-stripe assignment budget must be at least negative one");
}

double OrToolsGriddedBoundaryOptimizer::AffectedNetHpwl(
    const std::vector<int>& net_ids, bool apply_fanout_cutoff) const {
  double hpwl = 0.0;
  for (int net_id : net_ids) {
    DaliExpects(
        net_id >= 0 && net_id < static_cast<int>(circuit_->Nets().size()),
        "Boundary component refers to an unknown net");
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

OrToolsGriddedBoundaryOptimizerResult OrToolsGriddedBoundaryOptimizer::Optimize(
    Stripe* first_stripe, int first_stripe_id, int first_row,
    int first_last_row, Stripe* second_stripe, int second_stripe_id,
    int second_row, int second_last_row) const {
  OrToolsGriddedBoundaryOptimizerResult result;
  result.available = OrToolsCompactGriddedLegalizer::IsAvailable();
  if (!result.available) return result;

  ExactGriddedBoundaryModelBuilderConfig builder_config;
  builder_config.net_ignore_threshold = config_.net_ignore_threshold;
  builder_config.minimum_p_well_height = config_.minimum_p_well_height;
  builder_config.minimum_n_well_height = config_.minimum_n_well_height;
  ExactGriddedBoundaryBuildResult build =
      ExactGriddedBoundaryModelBuilder(circuit_, builder_config)
          .Build(first_stripe, first_stripe_id, first_row, first_last_row,
                 second_stripe, second_stripe_id, second_row, second_last_row);
  result.component_count = static_cast<int>(build.model.cells.size());
  result.net_count = static_cast<int>(build.model.nets.size());
  result.modeled_hpwl_before = AffectedNetHpwl(build.affected_net_ids, true);
  result.affected_hpwl_before = AffectedNetHpwl(build.affected_net_ids, false);
  result.modeled_hpwl_after = result.modeled_hpwl_before;
  result.affected_hpwl_after = result.affected_hpwl_before;

  ExactGriddedLegalizationConfig solver_config;
  solver_config.maximum_time_seconds = config_.maximum_time_seconds;
  solver_config.number_of_workers = config_.number_of_workers;
  solver_config.maximum_row_displacement = config_.maximum_row_displacement;
  solver_config.maximum_row_assignment_changes =
      config_.maximum_assignment_changes;
  solver_config.fix_row_geometry = true;
  solver_config.use_presolve = true;
  solver_config.use_solution_hint = config_.use_solution_hint;
  solver_config.validate_solution_hint = true;
  const ExactGriddedLegalizationResult solution =
      OrToolsCompactGriddedLegalizer().Solve(build.model, solver_config);
  result.status = solution.status;
  result.solver_wall_time_seconds = solution.wall_time_seconds;
  result.relative_gap = solution.relative_gap;
  if (!solution.HasSolution()) return result;

  std::unordered_map<int, Component*> components_by_id;
  for (Component* component : build.components) {
    components_by_id.emplace(component->Id(), component);
  }
  std::unordered_map<int, const ExactGriddedCell*> model_cells_by_id;
  for (const ExactGriddedCell& cell : build.model.cells) {
    model_cells_by_id.emplace(cell.component_id, &cell);
  }
  std::unordered_map<int, const ExactGriddedBoundaryRowSet*> row_sets_by_id;
  std::vector<GriddedRow*> affected_rows;
  for (const ExactGriddedBoundaryRowSet& row_set : build.row_sets) {
    row_sets_by_id.emplace(row_set.stripe_id, &row_set);
    affected_rows.insert(affected_rows.end(), row_set.rows.begin(),
                         row_set.rows.end());
  }

  bool solution_is_complete = solution.cells.size() == build.model.cells.size();
  std::unordered_set<int> placed_component_ids;
  for (const ExactGriddedCellPlacement& placement : solution.cells) {
    const auto model_cell = model_cells_by_id.find(placement.component_id);
    const auto row_set = row_sets_by_id.find(placement.stripe_id);
    if (model_cell == model_cells_by_id.end() ||
        components_by_id.count(placement.component_id) == 0 ||
        row_set == row_sets_by_id.end() ||
        !placed_component_ids.insert(placement.component_id).second) {
      solution_is_complete = false;
      break;
    }
    const ExactGriddedCell& cell = *model_cell->second;
    if (placement.row_index < 0 ||
        placement.row_index + static_cast<int>(cell.regions.size()) >
            static_cast<int>(row_set->second->rows.size())) {
      solution_is_complete = false;
      break;
    }
    if (placement.stripe_id != cell.initial_stripe_id ||
        placement.row_index != cell.initial_start_row) {
      ++result.reassigned_component_count;
    }
    if (placement.stripe_id != cell.initial_stripe_id) {
      ++result.cross_stripe_component_count;
    }
  }

  GriddedRowAssignmentTransaction transaction(circuit_, affected_rows);
  if (solution_is_complete) {
    for (GriddedRow* row : affected_rows) row->Components().clear();
    for (const ExactGriddedCellPlacement& placement : solution.cells) {
      Component* component = components_by_id.at(placement.component_id);
      component->SetLowerLeft(placement.x, placement.y);
      component->SetOrient(placement.is_flipped ? FS : N);
      const ExactGriddedCell& cell =
          *model_cells_by_id.at(placement.component_id);
      const auto& rows = row_sets_by_id.at(placement.stripe_id)->rows;
      for (int region_index = 0;
           region_index < static_cast<int>(cell.regions.size());
           ++region_index) {
        rows[placement.row_index + region_index]->AddComponent(component);
      }
    }
    for (GriddedRow* row : affected_rows) {
      std::sort(row->Components().begin(), row->Components().end(),
                [](const Component* lhs, const Component* rhs) {
                  return lhs->LLX() < rhs->LLX() ||
                         (lhs->LLX() == rhs->LLX() && lhs->Id() < rhs->Id());
                });
    }
  }

  const bool rows_are_legal =
      solution_is_complete &&
      std::all_of(affected_rows.begin(), affected_rows.end(),
                  [](const GriddedRow* row) {
                    return row->HasLegalComponentPlacement();
                  });
  if (rows_are_legal) {
    result.modeled_hpwl_after = AffectedNetHpwl(build.affected_net_ids, true);
    result.affected_hpwl_after = AffectedNetHpwl(build.affected_net_ids, false);
  }
  const double modeled_tolerance =
      1e-9 * std::max(1.0, result.modeled_hpwl_before);
  const double affected_tolerance =
      1e-9 * std::max(1.0, result.affected_hpwl_before);
  const bool improves_modeled_hpwl =
      result.modeled_hpwl_after + modeled_tolerance <
      result.modeled_hpwl_before;
  const bool improves_affected_hpwl =
      result.affected_hpwl_after + affected_tolerance <
      result.affected_hpwl_before;
  if (rows_are_legal && improves_modeled_hpwl && improves_affected_hpwl) {
    result.accepted = true;
  } else {
    transaction.Restore();
    result.modeled_hpwl_after = result.modeled_hpwl_before;
    result.affected_hpwl_after = result.affected_hpwl_before;
  }
  return result;
}

}  // namespace dali
