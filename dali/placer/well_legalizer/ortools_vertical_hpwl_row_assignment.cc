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
#include "dali/placer/well_legalizer/ortools_vertical_hpwl_row_assignment.h"

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

bool VerticalHpwlRowAssignmentResult::HasSolution() const {
  return status == VerticalHpwlRowAssignmentStatus::kFeasible ||
         status == VerticalHpwlRowAssignmentStatus::kOptimal;
}

bool OrToolsVerticalHpwlRowAssignment::IsAvailable() {
#ifdef DALI_HAS_OR_TOOLS
  return true;
#else
  return false;
#endif
}

VerticalHpwlRowAssignmentResult OrToolsVerticalHpwlRowAssignment::Solve(
    const VerticalHpwlRowAssignmentModel& model,
    const VerticalHpwlRowAssignmentConfig& config) const {
  VerticalHpwlRowAssignmentResult result;
#ifndef DALI_HAS_OR_TOOLS
  (void)model;
  (void)config;
  result.message =
      "Dali was built without a compatible OR-Tools 9.15 installation";
  return result;
#else
  if (config.maximum_time_seconds <= 0.0 || config.number_of_workers <= 0 ||
      config.coordinate_scale <= 0) {
    result.status = VerticalHpwlRowAssignmentStatus::kInvalidModel;
    result.message =
        "time, worker count, and coordinate scale must be positive";
    return result;
  }

  std::unordered_map<int, size_t> row_indices;
  for (size_t index = 0; index < model.rows.size(); ++index) {
    const VerticalHpwlRowCapacity& row = model.rows[index];
    if (row.row_id < 0 || row.capacity < 0 ||
        !row_indices.emplace(row.row_id, index).second) {
      result.status = VerticalHpwlRowAssignmentStatus::kInvalidModel;
      result.message = "row ids must be unique and capacities non-negative";
      return result;
    }
  }

  std::unordered_map<int, size_t> component_indices;
  for (size_t index = 0; index < model.components.size(); ++index) {
    const VerticalHpwlRowComponent& component = model.components[index];
    bool contains_initial_row = false;
    std::unordered_set<int> candidate_rows;
    for (const VerticalHpwlRowCandidate& candidate : component.candidates) {
      contains_initial_row |= candidate.row_id == component.initial_row_id;
      if (row_indices.count(candidate.row_id) == 0 ||
          !std::isfinite(candidate.component_lly) ||
          !candidate_rows.insert(candidate.row_id).second) {
        result.status = VerticalHpwlRowAssignmentStatus::kInvalidModel;
        result.message = "component candidates must refer to unique known rows";
        return result;
      }
    }
    if (component.component_id < 0 || component.width <= 0 ||
        component.candidates.empty() || !contains_initial_row ||
        !component_indices.emplace(component.component_id, index).second) {
      result.status = VerticalHpwlRowAssignmentStatus::kInvalidModel;
      result.message = "every component must include its initial row";
      return result;
    }
  }

  for (const VerticalHpwlRowNet& net : model.nets) {
    if (!std::isfinite(net.weight) || net.weight < 0.0) {
      result.status = VerticalHpwlRowAssignmentStatus::kInvalidModel;
      result.message = "net weights must be finite and non-negative";
      return result;
    }
    for (const VerticalHpwlRowPin& pin : net.pins) {
      if (pin.component_id < 0) {
        if (!std::isfinite(pin.fixed_y)) {
          result.status = VerticalHpwlRowAssignmentStatus::kInvalidModel;
          result.message = "fixed pin coordinates must be finite";
          return result;
        }
        continue;
      }
      const auto component = component_indices.find(pin.component_id);
      if (component == component_indices.end() ||
          pin.candidate_offsets_y.size() !=
              model.components[component->second].candidates.size() ||
          !std::all_of(pin.candidate_offsets_y.begin(),
                       pin.candidate_offsets_y.end(),
                       [](double value) { return std::isfinite(value); })) {
        result.status = VerticalHpwlRowAssignmentStatus::kInvalidModel;
        result.message =
            "movable net pins must match a component and all its candidates";
        return result;
      }
    }
  }

  if (model.components.empty()) {
    result.status = VerticalHpwlRowAssignmentStatus::kOptimal;
    result.message = "empty model";
    return result;
  }

  using operations_research::Domain;
  using operations_research::sat::BoolVar;
  using operations_research::sat::CpModelBuilder;
  using operations_research::sat::CpSolverResponse;
  using operations_research::sat::CpSolverStatus;
  using operations_research::sat::DoubleLinearExpr;
  using operations_research::sat::IntVar;
  using operations_research::sat::LinearExpr;
  using operations_research::sat::SatParameters;

  CpModelBuilder cp_model;
  std::vector<std::vector<BoolVar> > assignments;
  assignments.reserve(model.components.size());
  for (const VerticalHpwlRowComponent& component : model.components) {
    std::vector<BoolVar> choices;
    LinearExpr selected_count;
    for (const VerticalHpwlRowCandidate& candidate : component.candidates) {
      BoolVar selected = cp_model.NewBoolVar().WithName(
          "component_" + std::to_string(component.component_id) + "_row_" +
          std::to_string(candidate.row_id));
      choices.push_back(selected);
      selected_count += selected;
      cp_model.AddHint(selected,
                       candidate.row_id == component.initial_row_id ? 1 : 0);
    }
    cp_model.AddEquality(selected_count, 1);
    assignments.push_back(std::move(choices));
  }

  for (const VerticalHpwlRowCapacity& row : model.rows) {
    LinearExpr used_width;
    for (size_t component_index = 0; component_index < model.components.size();
         ++component_index) {
      const VerticalHpwlRowComponent& component =
          model.components[component_index];
      for (size_t candidate_index = 0;
           candidate_index < component.candidates.size(); ++candidate_index) {
        if (component.candidates[candidate_index].row_id == row.row_id) {
          used_width +=
              assignments[component_index][candidate_index] * component.width;
        }
      }
    }
    cp_model.AddLessOrEqual(used_width, row.capacity);
  }

  const int64_t scale = config.coordinate_scale;
  DoubleLinearExpr objective;
  for (size_t net_index = 0; net_index < model.nets.size(); ++net_index) {
    const VerticalHpwlRowNet& net = model.nets[net_index];
    if (net.pins.size() < 2 || net.weight <= 0.0) continue;

    std::vector<LinearExpr> pin_locations;
    int64_t minimum_y = std::numeric_limits<int64_t>::max();
    int64_t maximum_y = std::numeric_limits<int64_t>::min();
    for (const VerticalHpwlRowPin& pin : net.pins) {
      if (pin.component_id < 0) {
        const int64_t y = std::llround(pin.fixed_y * scale);
        pin_locations.emplace_back(y);
        minimum_y = std::min(minimum_y, y);
        maximum_y = std::max(maximum_y, y);
        continue;
      }

      const size_t component_index = component_indices.at(pin.component_id);
      const VerticalHpwlRowComponent& component =
          model.components[component_index];
      LinearExpr pin_y;
      for (size_t candidate_index = 0;
           candidate_index < component.candidates.size(); ++candidate_index) {
        const int64_t y =
            std::llround((component.candidates[candidate_index].component_lly +
                          pin.candidate_offsets_y[candidate_index]) *
                         scale);
        pin_y += assignments[component_index][candidate_index] * y;
        minimum_y = std::min(minimum_y, y);
        maximum_y = std::max(maximum_y, y);
      }
      pin_locations.push_back(std::move(pin_y));
    }

    IntVar net_min = cp_model.NewIntVar(Domain(minimum_y, maximum_y));
    IntVar net_max = cp_model.NewIntVar(Domain(minimum_y, maximum_y));
    cp_model.AddMinEquality(net_min, pin_locations);
    cp_model.AddMaxEquality(net_max, pin_locations);
    objective.AddTerm(net_max, net.weight);
    objective.AddTerm(net_min, -net.weight);
  }
  cp_model.Minimize(objective);

  SatParameters parameters;
  parameters.set_max_time_in_seconds(config.maximum_time_seconds);
  parameters.set_num_search_workers(config.number_of_workers);
  const CpSolverResponse response =
      operations_research::sat::SolveWithParameters(cp_model.Build(),
                                                    parameters);
  result.wall_time_seconds = response.wall_time();
  result.objective_value = response.objective_value() / scale;
  result.best_objective_bound = response.best_objective_bound() / scale;

  switch (response.status()) {
    case CpSolverStatus::OPTIMAL:
      result.status = VerticalHpwlRowAssignmentStatus::kOptimal;
      break;
    case CpSolverStatus::FEASIBLE:
      result.status = VerticalHpwlRowAssignmentStatus::kFeasible;
      break;
    case CpSolverStatus::INFEASIBLE:
      result.status = VerticalHpwlRowAssignmentStatus::kInfeasible;
      result.message = "vertical-HPWL row-assignment model is infeasible";
      return result;
    default:
      result.status = VerticalHpwlRowAssignmentStatus::kUnknown;
      result.message = "solver did not find a feasible assignment";
      return result;
  }

  for (size_t component_index = 0; component_index < model.components.size();
       ++component_index) {
    const VerticalHpwlRowComponent& component =
        model.components[component_index];
    bool found = false;
    for (size_t candidate_index = 0;
         candidate_index < component.candidates.size(); ++candidate_index) {
      if (operations_research::sat::SolutionBooleanValue(
              response, assignments[component_index][candidate_index])) {
        result.assignments.push_back(
            {component.component_id,
             component.candidates[candidate_index].row_id});
        found = true;
        break;
      }
    }
    if (!found) {
      result.status = VerticalHpwlRowAssignmentStatus::kInvalidModel;
      result.assignments.clear();
      result.message = "solver returned an incomplete assignment";
      return result;
    }
  }
  return result;
#endif
}

}  // namespace dali
