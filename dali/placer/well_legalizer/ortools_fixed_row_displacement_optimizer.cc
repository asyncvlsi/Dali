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
#include "dali/placer/well_legalizer/ortools_fixed_row_displacement_optimizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

#ifdef DALI_HAS_OR_TOOLS
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"
#endif

namespace dali {

bool FixedRowDisplacementResult::HasSolution() const {
  return status == FixedRowDisplacementStatus::kFeasible ||
         status == FixedRowDisplacementStatus::kOptimal;
}

bool OrToolsFixedRowDisplacementOptimizer::IsAvailable() {
#ifdef DALI_HAS_OR_TOOLS
  return true;
#else
  return false;
#endif
}

FixedRowDisplacementResult OrToolsFixedRowDisplacementOptimizer::Solve(
    const FixedRowDisplacementModel& model,
    const FixedRowDisplacementSolverConfig& config) const {
  FixedRowDisplacementResult result;

#ifndef DALI_HAS_OR_TOOLS
  (void)model;
  (void)config;
  result.message =
      "Dali was built without a compatible OR-Tools 9.15 installation";
  return result;
#else
  if (config.maximum_time_seconds <= 0.0) {
    result.status = FixedRowDisplacementStatus::kInvalidModel;
    result.message = "maximum solve time must be positive";
    return result;
  }
  if (config.number_of_workers <= 0) {
    result.status = FixedRowDisplacementStatus::kInvalidModel;
    result.message = "number of workers must be positive";
    return result;
  }

  std::unordered_map<int, size_t> component_indices;
  component_indices.reserve(model.components.size());
  for (size_t index = 0; index < model.components.size(); ++index) {
    const FixedRowComponentVariable& component = model.components[index];
    if (component.component_id < 0) {
      result.status = FixedRowDisplacementStatus::kInvalidModel;
      result.message = "component ids must be non-negative";
      return result;
    }
    if (component.width <= 0) {
      result.status = FixedRowDisplacementStatus::kInvalidModel;
      result.message = "component widths must be positive";
      return result;
    }
    if (component.minimum_x > component.maximum_x) {
      result.status = FixedRowDisplacementStatus::kInvalidModel;
      result.message = "component X bounds are empty";
      return result;
    }
    if (!component_indices.emplace(component.component_id, index).second) {
      result.status = FixedRowDisplacementStatus::kInvalidModel;
      result.message = "component ids must be unique";
      return result;
    }
  }

  for (const FixedRowComponentSequence& row : model.rows) {
    if (row.minimum_spacing < 0) {
      result.status = FixedRowDisplacementStatus::kInvalidModel;
      result.message = "row spacing must be non-negative";
      return result;
    }
    std::unordered_set<int> row_component_ids;
    row_component_ids.reserve(row.component_ids.size());
    for (int component_id : row.component_ids) {
      if (component_indices.count(component_id) == 0) {
        result.status = FixedRowDisplacementStatus::kInvalidModel;
        result.message = "row refers to an unknown component";
        return result;
      }
      if (!row_component_ids.insert(component_id).second) {
        result.status = FixedRowDisplacementStatus::kInvalidModel;
        result.message = "a component occurs more than once in one row";
        return result;
      }
    }
  }

  if (model.components.empty()) {
    result.status = FixedRowDisplacementStatus::kOptimal;
    result.message = "empty model";
    return result;
  }

  using operations_research::Domain;
  using operations_research::sat::CpModelBuilder;
  using operations_research::sat::CpSolverResponse;
  using operations_research::sat::CpSolverStatus;
  using operations_research::sat::IntVar;
  using operations_research::sat::LinearExpr;
  using operations_research::sat::SatParameters;

  CpModelBuilder cp_model;
  std::vector<IntVar> x_variables;
  x_variables.reserve(model.components.size());
  LinearExpr displacement_objective;

  for (const FixedRowComponentVariable& component : model.components) {
    IntVar x =
        cp_model.NewIntVar(Domain(component.minimum_x, component.maximum_x))
            .WithName("x_" + std::to_string(component.component_id));
    x_variables.push_back(x);
    cp_model.AddHint(x, std::clamp(component.initial_x, component.minimum_x,
                                   component.maximum_x));

    int64_t lower_displacement = std::abs(
        static_cast<int64_t>(component.minimum_x) - component.initial_x);
    int64_t upper_displacement = std::abs(
        static_cast<int64_t>(component.maximum_x) - component.initial_x);
    int64_t maximum_displacement =
        std::max(lower_displacement, upper_displacement);
    IntVar absolute_displacement =
        cp_model.NewIntVar(Domain(0, maximum_displacement))
            .WithName("x_displacement_" +
                      std::to_string(component.component_id));
    cp_model.AddAbsEquality(absolute_displacement, x - component.initial_x);
    displacement_objective += absolute_displacement;
  }

  for (const FixedRowComponentSequence& row : model.rows) {
    for (size_t index = 1; index < row.component_ids.size(); ++index) {
      size_t left_index = component_indices.at(row.component_ids[index - 1]);
      size_t right_index = component_indices.at(row.component_ids[index]);
      const FixedRowComponentVariable& left = model.components[left_index];
      cp_model.AddGreaterOrEqual(
          x_variables[right_index],
          x_variables[left_index] + left.width + row.minimum_spacing);
    }
  }
  cp_model.Minimize(displacement_objective);

  SatParameters parameters;
  parameters.set_max_time_in_seconds(config.maximum_time_seconds);
  parameters.set_num_search_workers(config.number_of_workers);
  parameters.set_log_search_progress(false);
  CpSolverResponse response = operations_research::sat::SolveWithParameters(
      cp_model.Build(), parameters);

  result.best_objective_bound = response.best_objective_bound();
  result.conflict_count = response.num_conflicts();
  result.branch_count = response.num_branches();
  result.wall_time_seconds = response.wall_time();

  switch (response.status()) {
    case CpSolverStatus::OPTIMAL:
      result.status = FixedRowDisplacementStatus::kOptimal;
      break;
    case CpSolverStatus::FEASIBLE:
      result.status = FixedRowDisplacementStatus::kFeasible;
      break;
    case CpSolverStatus::INFEASIBLE:
      result.status = FixedRowDisplacementStatus::kInfeasible;
      result.message = "fixed-row model is infeasible";
      return result;
    case CpSolverStatus::MODEL_INVALID:
      result.status = FixedRowDisplacementStatus::kInvalidModel;
      result.message = response.solution_info();
      return result;
    case CpSolverStatus::UNKNOWN:
      result.status = FixedRowDisplacementStatus::kUnknown;
      result.message = "solver stopped before finding a feasible solution";
      return result;
    default:
      result.status = FixedRowDisplacementStatus::kUnknown;
      result.message = "solver returned an unrecognized status";
      return result;
  }

  result.total_displacement =
      static_cast<int64_t>(std::llround(response.objective_value()));
  result.relative_gap = std::max(0.0, response.objective_value() -
                                          response.best_objective_bound()) /
                        std::max(1.0, std::abs(response.objective_value()));
  result.locations.reserve(model.components.size());
  for (size_t index = 0; index < model.components.size(); ++index) {
    result.locations.push_back(
        {model.components[index].component_id,
         static_cast<int>(operations_research::sat::SolutionIntegerValue(
             response, x_variables[index]))});
  }
  result.message = response.status() == CpSolverStatus::OPTIMAL
                       ? "optimal solution"
                       : "feasible solution";
  return result;
#endif
}

}  // namespace dali
