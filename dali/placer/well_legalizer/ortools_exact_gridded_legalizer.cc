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
#include "dali/placer/well_legalizer/ortools_exact_gridded_legalizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

#ifdef DALI_HAS_OR_TOOLS
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"
#endif

namespace dali {

const char* ExactGriddedLegalizationStatusName(
    ExactGriddedLegalizationStatus status) {
  switch (status) {
    case ExactGriddedLegalizationStatus::kUnavailable:
      return "unavailable";
    case ExactGriddedLegalizationStatus::kInvalidModel:
      return "invalid_model";
    case ExactGriddedLegalizationStatus::kUnknown:
      return "unknown";
    case ExactGriddedLegalizationStatus::kInfeasible:
      return "infeasible";
    case ExactGriddedLegalizationStatus::kFeasible:
      return "feasible";
    case ExactGriddedLegalizationStatus::kOptimal:
      return "optimal";
  }
  return "unknown";
}

bool ExactGriddedLegalizationResult::HasSolution() const {
  return status == ExactGriddedLegalizationStatus::kFeasible ||
         status == ExactGriddedLegalizationStatus::kOptimal;
}

bool OrToolsExactGriddedLegalizer::IsAvailable() {
#ifdef DALI_HAS_OR_TOOLS
  return true;
#else
  return false;
#endif
}

#ifdef DALI_HAS_OR_TOOLS

using operations_research::Domain;
using operations_research::sat::BoolVar;
using operations_research::sat::CpModelBuilder;
using operations_research::sat::CpSolverResponse;
using operations_research::sat::DoubleLinearExpr;
using operations_research::sat::IntervalVar;
using operations_research::sat::IntVar;
using operations_research::sat::LinearExpr;

/** CP-SAT variables belonging to one potential gridded row. */
struct ExactGriddedRowVariables {
  BoolVar active;
  IntVar y;
  IntVar p_well_height;
  IntVar n_well_height;
  std::vector<BoolVar> occupants;
  std::vector<LinearExpr> p_well_height_candidates;
  std::vector<LinearExpr> n_well_height_candidates;
  std::vector<IntervalVar> intervals;
};

/** CP-SAT variables shared by every placement candidate for one component. */
struct ExactGriddedCellVariables {
  IntVar x;
  IntVar y;
  int minimum_x = 0;
  int maximum_x = 0;
  int minimum_y = 0;
  int maximum_y = 0;
  std::vector<size_t> candidate_indices;
};

/** One enumerated stripe, row-start, and orientation choice. */
struct ExactGriddedCandidateVariables {
  size_t cell_index = 0;
  size_t stripe_index = 0;
  int start_row = 0;
  bool is_flipped = false;
  bool required_first_row_orient_n = true;
  BoolVar presence;
};

/** Min/max variables retained so solved physical HPWL can be reported. */
struct ExactGriddedNetVariables {
  IntVar minimum_x;
  IntVar maximum_x;
  IntVar minimum_y;
  IntVar maximum_y;
  double weight = 0.0;
};

/** Return one region in physical bottom-up order for N or FS orientation. */
ExactGriddedCellRegion GetExactGriddedPhysicalRegion(
    const ExactGriddedCell& cell, int physical_region_index, bool is_flipped) {
  int source_index = is_flipped ? static_cast<int>(cell.regions.size()) - 1 -
                                      physical_region_index
                                : physical_region_index;
  ExactGriddedCellRegion region = cell.regions[source_index];
  if (is_flipped) {
    region.n_well_above_p_well = !region.n_well_above_p_well;
  }
  return region;
}

/**
 * Return whether a placement follows alternating row well orientations.
 *
 * required_first_row_orient_n receives the stripe's row-zero phase needed by
 * this placement, not the orientation of the component's starting row.
 */
bool ExactGriddedCandidateMatchesAlternatingRows(
    const ExactGriddedCell& cell, int start_row, bool is_flipped,
    bool* required_first_row_orient_n) {
  ExactGriddedCellRegion first_region =
      GetExactGriddedPhysicalRegion(cell, 0, is_flipped);
  bool first_row_orient_n = (start_row % 2 == 0)
                                ? first_region.n_well_above_p_well
                                : !first_region.n_well_above_p_well;
  for (int index = 0; index < static_cast<int>(cell.regions.size()); ++index) {
    bool row_orient_n = ((start_row + index) % 2 == 0) ? first_row_orient_n
                                                       : !first_row_orient_n;
    if (GetExactGriddedPhysicalRegion(cell, index, is_flipped)
            .n_well_above_p_well != row_orient_n) {
      return false;
    }
  }
  *required_first_row_orient_n = first_row_orient_n;
  return true;
}

#endif

ExactGriddedLegalizationResult OrToolsExactGriddedLegalizer::Solve(
    const ExactGriddedLegalizationModel& model,
    const ExactGriddedLegalizationConfig& config) const {
  ExactGriddedLegalizationResult result;

#ifndef DALI_HAS_OR_TOOLS
  (void)model;
  (void)config;
  result.message =
      "Dali was built without a compatible OR-Tools 9.15 installation";
  return result;
#else
  std::string validation_error = model.Validate();
  if (!validation_error.empty()) {
    result.status = ExactGriddedLegalizationStatus::kInvalidModel;
    result.message = validation_error;
    return result;
  }
  if (config.maximum_time_seconds <= 0.0) {
    result.status = ExactGriddedLegalizationStatus::kInvalidModel;
    result.message = "maximum solve time must be positive";
    return result;
  }
  if (config.number_of_workers <= 0) {
    result.status = ExactGriddedLegalizationStatus::kInvalidModel;
    result.message = "number of workers must be positive";
    return result;
  }
  if (config.pin_coordinate_scale <= 0) {
    result.status = ExactGriddedLegalizationStatus::kInvalidModel;
    result.message = "pin coordinate scale must be positive";
    return result;
  }
  if (!std::isfinite(config.weighted_hpwl_weight) ||
      config.weighted_hpwl_weight < 0.0 ||
      !std::isfinite(config.displacement_weight) ||
      config.displacement_weight < 0.0 ||
      (config.weighted_hpwl_weight == 0.0 &&
       config.displacement_weight == 0.0)) {
    result.status = ExactGriddedLegalizationStatus::kInvalidModel;
    result.message =
        "at least one finite, non-negative objective weight must be positive";
    return result;
  }
  if (model.cells.empty()) {
    result.status = ExactGriddedLegalizationStatus::kOptimal;
    result.message = "empty model";
    return result;
  }

  std::unordered_map<int, size_t> stripe_indices;
  for (size_t index = 0; index < model.stripes.size(); ++index) {
    stripe_indices.emplace(model.stripes[index].stripe_id, index);
  }
  std::unordered_map<int, size_t> cell_indices;
  for (size_t index = 0; index < model.cells.size(); ++index) {
    cell_indices.emplace(model.cells[index].component_id, index);
  }

  CpModelBuilder cp_model;
  DoubleLinearExpr objective;
  std::vector<BoolVar> first_row_orient_n_variables;
  std::vector<std::vector<ExactGriddedRowVariables> > row_variables;
  first_row_orient_n_variables.reserve(model.stripes.size());
  row_variables.reserve(model.stripes.size());

  for (const ExactGriddedStripe& stripe : model.stripes) {
    first_row_orient_n_variables.push_back(cp_model.NewBoolVar().WithName(
        "stripe_phase_" + std::to_string(stripe.stripe_id)));
    std::vector<ExactGriddedRowVariables> stripe_rows;
    stripe_rows.reserve(stripe.maximum_rows);
    int stripe_height = stripe.uy - stripe.ly;
    for (int row_index = 0; row_index < stripe.maximum_rows; ++row_index) {
      std::string suffix =
          std::to_string(stripe.stripe_id) + "_" + std::to_string(row_index);
      ExactGriddedRowVariables row;
      row.active = cp_model.NewBoolVar().WithName("row_active_" + suffix);
      row.y = cp_model.NewIntVar(Domain(stripe.ly, stripe.uy))
                  .WithName("row_y_" + suffix);
      row.p_well_height = cp_model.NewIntVar(Domain(0, stripe_height))
                              .WithName("row_p_height_" + suffix);
      row.n_well_height = cp_model.NewIntVar(Domain(0, stripe_height))
                              .WithName("row_n_height_" + suffix);
      row.p_well_height_candidates.push_back(
          LinearExpr::Term(row.active, stripe.minimum_p_well_height));
      row.n_well_height_candidates.push_back(
          LinearExpr::Term(row.active, stripe.minimum_n_well_height));
      stripe_rows.push_back(std::move(row));
    }
    row_variables.push_back(std::move(stripe_rows));
  }

  std::vector<ExactGriddedCellVariables> cell_variables;
  std::vector<ExactGriddedCandidateVariables> candidate_variables;
  std::vector<int> stripe_phase_hints(model.stripes.size(), -1);
  cell_variables.reserve(model.cells.size());

  for (size_t cell_index = 0; cell_index < model.cells.size(); ++cell_index) {
    const ExactGriddedCell& cell = model.cells[cell_index];
    int minimum_x = std::numeric_limits<int>::max();
    int maximum_x = std::numeric_limits<int>::min();
    int minimum_y = std::numeric_limits<int>::max();
    int maximum_y = std::numeric_limits<int>::min();
    for (int stripe_id : cell.candidate_stripe_ids) {
      const ExactGriddedStripe& stripe =
          model.stripes[stripe_indices.at(stripe_id)];
      minimum_x = std::min(minimum_x, stripe.lx + stripe.left_boundary_margin);
      maximum_x = std::max(
          maximum_x, stripe.ux - stripe.right_boundary_margin - cell.width);
      minimum_y = std::min(minimum_y, stripe.ly);
      maximum_y = std::max(maximum_y, stripe.uy);
    }
    if (minimum_x > maximum_x) {
      result.status = ExactGriddedLegalizationStatus::kInvalidModel;
      result.message = "component is wider than every candidate stripe";
      return result;
    }

    ExactGriddedCellVariables variables;
    variables.minimum_x = minimum_x;
    variables.maximum_x = maximum_x;
    variables.minimum_y = minimum_y;
    variables.maximum_y = maximum_y;
    variables.x = cp_model.NewIntVar(Domain(minimum_x, maximum_x))
                      .WithName("cell_x_" + std::to_string(cell.component_id));
    variables.y = cp_model.NewIntVar(Domain(minimum_y, maximum_y))
                      .WithName("cell_y_" + std::to_string(cell.component_id));
    cp_model.AddHint(variables.x,
                     std::clamp(cell.initial_x, minimum_x, maximum_x));
    cp_model.AddHint(variables.y,
                     std::clamp(cell.initial_y, minimum_y, maximum_y));
    cell_variables.push_back(std::move(variables));

    bool use_discrete_hint = false;
    bool hinted_first_row_orient_n = true;
    if (cell.initial_stripe_id >= 0 && cell.initial_start_row >= 0) {
      use_discrete_hint = ExactGriddedCandidateMatchesAlternatingRows(
          cell, cell.initial_start_row, cell.initial_is_flipped,
          &hinted_first_row_orient_n);
      if (!use_discrete_hint) {
        result.status = ExactGriddedLegalizationStatus::kInvalidModel;
        result.message =
            "component placement hint violates alternating row orientations";
        return result;
      }
    }
    std::vector<BoolVar> placements;
    for (int stripe_id : cell.candidate_stripe_ids) {
      size_t stripe_index = stripe_indices.at(stripe_id);
      const ExactGriddedStripe& stripe = model.stripes[stripe_index];
      if (cell.width > stripe.ux - stripe.lx - stripe.left_boundary_margin -
                           stripe.right_boundary_margin) {
        continue;
      }
      int last_start_row =
          stripe.maximum_rows - static_cast<int>(cell.regions.size());
      for (int start_row = 0; start_row <= last_start_row; ++start_row) {
        for (bool is_flipped : {false, true}) {
          bool required_first_row_orient_n = true;
          if (!ExactGriddedCandidateMatchesAlternatingRows(
                  cell, start_row, is_flipped, &required_first_row_orient_n)) {
            continue;
          }
          std::string candidate_name = std::to_string(cell.component_id) + "_" +
                                       std::to_string(stripe_id) + "_" +
                                       std::to_string(start_row) +
                                       (is_flipped ? "_fs" : "_n");
          BoolVar presence = cp_model.NewBoolVar().WithName("cell_placement_" +
                                                            candidate_name);
          const bool is_hinted_placement =
              use_discrete_hint && stripe_id == cell.initial_stripe_id &&
              start_row == cell.initial_start_row &&
              is_flipped == cell.initial_is_flipped;
          if (use_discrete_hint) {
            cp_model.AddHint(presence, is_hinted_placement);
          }
          if (is_hinted_placement) {
            int& stripe_phase_hint = stripe_phase_hints[stripe_index];
            int required_phase = hinted_first_row_orient_n ? 1 : 0;
            if (stripe_phase_hint >= 0 && stripe_phase_hint != required_phase) {
              result.status = ExactGriddedLegalizationStatus::kInvalidModel;
              result.message =
                  "component placement hints require inconsistent stripe "
                  "orientations";
              return result;
            }
            stripe_phase_hint = required_phase;
          }
          placements.push_back(presence);
          size_t candidate_index = candidate_variables.size();
          candidate_variables.push_back(
              {cell_index, stripe_index, start_row, is_flipped,
               required_first_row_orient_n, presence});
          cell_variables[cell_index].candidate_indices.push_back(
              candidate_index);

          cp_model
              .AddEquality(first_row_orient_n_variables[stripe_index],
                           required_first_row_orient_n ? 1 : 0)
              .OnlyEnforceIf(presence);
          cp_model
              .AddGreaterOrEqual(cell_variables[cell_index].x,
                                 stripe.lx + stripe.left_boundary_margin)
              .OnlyEnforceIf(presence);
          cp_model
              .AddLessOrEqual(
                  cell_variables[cell_index].x,
                  stripe.ux - stripe.right_boundary_margin - cell.width)
              .OnlyEnforceIf(presence);

          ExactGriddedCellRegion first_region =
              GetExactGriddedPhysicalRegion(cell, 0, is_flipped);
          ExactGriddedRowVariables& first_row =
              row_variables[stripe_index][start_row];
          LinearExpr y_location = first_row.y;
          if (first_region.n_well_above_p_well) {
            y_location += first_row.p_well_height;
            y_location -= first_region.p_well_height;
          } else {
            y_location += first_row.n_well_height;
            y_location -= first_region.n_well_height;
          }
          cp_model.AddEquality(cell_variables[cell_index].y, y_location)
              .OnlyEnforceIf(presence);

          for (int region_index = 0;
               region_index < static_cast<int>(cell.regions.size());
               ++region_index) {
            ExactGriddedCellRegion region =
                GetExactGriddedPhysicalRegion(cell, region_index, is_flipped);
            ExactGriddedRowVariables& row =
                row_variables[stripe_index][start_row + region_index];
            row.occupants.push_back(presence);
            row.p_well_height_candidates.push_back(
                LinearExpr::Term(presence, region.p_well_height));
            row.n_well_height_candidates.push_back(
                LinearExpr::Term(presence, region.n_well_height));
            row.intervals.push_back(cp_model.NewOptionalFixedSizeIntervalVar(
                cell_variables[cell_index].x, cell.width, presence));
          }
        }
      }
    }
    if (placements.empty()) {
      result.status = ExactGriddedLegalizationStatus::kInvalidModel;
      result.message = "component has no orientation-compatible placement";
      return result;
    }
    cp_model.AddExactlyOne(placements);
  }

  for (size_t stripe_index = 0; stripe_index < stripe_phase_hints.size();
       ++stripe_index) {
    if (stripe_phase_hints[stripe_index] >= 0) {
      cp_model.AddHint(first_row_orient_n_variables[stripe_index],
                       stripe_phase_hints[stripe_index] != 0);
    }
  }

  for (size_t stripe_index = 0; stripe_index < model.stripes.size();
       ++stripe_index) {
    const ExactGriddedStripe& stripe = model.stripes[stripe_index];
    std::vector<ExactGriddedRowVariables>& rows = row_variables[stripe_index];
    for (int row_index = 0; row_index < stripe.maximum_rows; ++row_index) {
      ExactGriddedRowVariables& row = rows[row_index];
      for (BoolVar occupant : row.occupants) {
        cp_model.AddImplication(occupant, row.active);
      }
      if (row.occupants.empty()) {
        cp_model.AddEquality(row.active, 0);
      } else {
        cp_model.AddLessOrEqual(row.active, LinearExpr::Sum(row.occupants));
      }
      cp_model.AddMaxEquality(row.p_well_height, row.p_well_height_candidates);
      cp_model.AddMaxEquality(row.n_well_height, row.n_well_height_candidates);
      if (!row.intervals.empty()) cp_model.AddNoOverlap(row.intervals);
      if (row_index > 0) {
        cp_model.AddGreaterOrEqual(rows[row_index - 1].active, row.active);
        cp_model.AddGreaterOrEqual(
            row.y, rows[row_index - 1].y + rows[row_index - 1].p_well_height +
                       rows[row_index - 1].n_well_height);
      } else {
        cp_model.AddGreaterOrEqual(row.y, stripe.ly);
      }
    }
    const ExactGriddedRowVariables& last_row = rows.back();
    cp_model.AddLessOrEqual(
        last_row.y + last_row.p_well_height + last_row.n_well_height,
        stripe.uy);
  }

  if (config.displacement_weight > 0.0) {
    for (size_t index = 0; index < model.cells.size(); ++index) {
      const ExactGriddedCell& cell = model.cells[index];
      const ExactGriddedCellVariables& variables = cell_variables[index];
      int maximum_x_displacement =
          std::max(std::abs(variables.minimum_x - cell.initial_x),
                   std::abs(variables.maximum_x - cell.initial_x));
      int maximum_y_displacement =
          std::max(std::abs(variables.minimum_y - cell.initial_y),
                   std::abs(variables.maximum_y - cell.initial_y));
      IntVar x_displacement =
          cp_model.NewIntVar(Domain(0, maximum_x_displacement));
      IntVar y_displacement =
          cp_model.NewIntVar(Domain(0, maximum_y_displacement));
      cp_model.AddAbsEquality(x_displacement, variables.x - cell.initial_x);
      cp_model.AddAbsEquality(y_displacement, variables.y - cell.initial_y);
      objective.AddTerm(x_displacement,
                        config.displacement_weight * model.distance_scale_x);
      objective.AddTerm(y_displacement,
                        config.displacement_weight * model.distance_scale_y);
    }
  }

  int64_t pin_scale = config.pin_coordinate_scale;
  std::vector<ExactGriddedNetVariables> net_variables;
  net_variables.reserve(model.nets.size());
  for (size_t net_index = 0; net_index < model.nets.size(); ++net_index) {
    const ExactGriddedNet& net = model.nets[net_index];
    if (net.pins.size() < 2 || net.weight == 0.0) continue;

    std::vector<LinearExpr> pin_x_locations;
    std::vector<LinearExpr> pin_y_locations;
    int64_t minimum_x = std::numeric_limits<int64_t>::max();
    int64_t maximum_x = std::numeric_limits<int64_t>::min();
    int64_t minimum_y = std::numeric_limits<int64_t>::max();
    int64_t maximum_y = std::numeric_limits<int64_t>::min();
    for (size_t pin_index = 0; pin_index < net.pins.size(); ++pin_index) {
      const ExactGriddedNetPin& pin = net.pins[pin_index];
      if (pin.component_id < 0) {
        int64_t fixed_x = static_cast<int64_t>(
            std::llround(pin.fixed_x * static_cast<double>(pin_scale)));
        int64_t fixed_y = static_cast<int64_t>(
            std::llround(pin.fixed_y * static_cast<double>(pin_scale)));
        pin_x_locations.emplace_back(fixed_x);
        pin_y_locations.emplace_back(fixed_y);
        minimum_x = std::min(minimum_x, fixed_x);
        maximum_x = std::max(maximum_x, fixed_x);
        minimum_y = std::min(minimum_y, fixed_y);
        maximum_y = std::max(maximum_y, fixed_y);
        continue;
      }

      size_t cell_index = cell_indices.at(pin.component_id);
      const ExactGriddedCellVariables& variables = cell_variables[cell_index];
      int64_t minimum_offset_x = static_cast<int64_t>(
          std::llround(std::min(pin.offset_x_n, pin.offset_x_fs) * pin_scale));
      int64_t maximum_offset_x = static_cast<int64_t>(
          std::llround(std::max(pin.offset_x_n, pin.offset_x_fs) * pin_scale));
      int64_t minimum_offset_y = static_cast<int64_t>(
          std::llround(std::min(pin.offset_y_n, pin.offset_y_fs) * pin_scale));
      int64_t maximum_offset_y = static_cast<int64_t>(
          std::llround(std::max(pin.offset_y_n, pin.offset_y_fs) * pin_scale));
      int64_t pin_minimum_x =
          static_cast<int64_t>(variables.minimum_x) * pin_scale +
          minimum_offset_x;
      int64_t pin_maximum_x =
          static_cast<int64_t>(variables.maximum_x) * pin_scale +
          maximum_offset_x;
      int64_t pin_minimum_y =
          static_cast<int64_t>(variables.minimum_y) * pin_scale +
          minimum_offset_y;
      int64_t pin_maximum_y =
          static_cast<int64_t>(variables.maximum_y) * pin_scale +
          maximum_offset_y;
      IntVar pin_x = cp_model.NewIntVar(Domain(pin_minimum_x, pin_maximum_x))
                         .WithName("pin_x_" + std::to_string(net_index) + "_" +
                                   std::to_string(pin_index));
      IntVar pin_y = cp_model.NewIntVar(Domain(pin_minimum_y, pin_maximum_y))
                         .WithName("pin_y_" + std::to_string(net_index) + "_" +
                                   std::to_string(pin_index));
      for (size_t candidate_index : variables.candidate_indices) {
        const ExactGriddedCandidateVariables& candidate =
            candidate_variables[candidate_index];
        int64_t offset_x = static_cast<int64_t>(std::llround(
            (candidate.is_flipped ? pin.offset_x_fs : pin.offset_x_n) *
            pin_scale));
        int64_t offset_y = static_cast<int64_t>(std::llround(
            (candidate.is_flipped ? pin.offset_y_fs : pin.offset_y_n) *
            pin_scale));
        cp_model
            .AddEquality(pin_x,
                         LinearExpr::Term(variables.x, pin_scale) + offset_x)
            .OnlyEnforceIf(candidate.presence);
        cp_model
            .AddEquality(pin_y,
                         LinearExpr::Term(variables.y, pin_scale) + offset_y)
            .OnlyEnforceIf(candidate.presence);
      }
      pin_x_locations.push_back(pin_x);
      pin_y_locations.push_back(pin_y);
      minimum_x = std::min(minimum_x, pin_minimum_x);
      maximum_x = std::max(maximum_x, pin_maximum_x);
      minimum_y = std::min(minimum_y, pin_minimum_y);
      maximum_y = std::max(maximum_y, pin_maximum_y);
    }

    ExactGriddedNetVariables variables;
    variables.minimum_x = cp_model.NewIntVar(Domain(minimum_x, maximum_x));
    variables.maximum_x = cp_model.NewIntVar(Domain(minimum_x, maximum_x));
    variables.minimum_y = cp_model.NewIntVar(Domain(minimum_y, maximum_y));
    variables.maximum_y = cp_model.NewIntVar(Domain(minimum_y, maximum_y));
    variables.weight = net.weight;
    cp_model.AddMinEquality(variables.minimum_x, pin_x_locations);
    cp_model.AddMaxEquality(variables.maximum_x, pin_x_locations);
    cp_model.AddMinEquality(variables.minimum_y, pin_y_locations);
    cp_model.AddMaxEquality(variables.maximum_y, pin_y_locations);
    objective.AddExpression(variables.maximum_x - variables.minimum_x,
                            config.weighted_hpwl_weight * net.weight *
                                model.distance_scale_x /
                                static_cast<double>(pin_scale));
    objective.AddExpression(variables.maximum_y - variables.minimum_y,
                            config.weighted_hpwl_weight * net.weight *
                                model.distance_scale_y /
                                static_cast<double>(pin_scale));
    net_variables.push_back(variables);
  }
  cp_model.Minimize(objective);

  operations_research::sat::SatParameters parameters;
  parameters.set_max_time_in_seconds(config.maximum_time_seconds);
  parameters.set_num_search_workers(config.number_of_workers);
  parameters.set_log_search_progress(config.log_search_progress);
  parameters.set_repair_hint(true);
  CpSolverResponse response = operations_research::sat::SolveWithParameters(
      cp_model.Build(), parameters);

  result.best_objective_bound = response.best_objective_bound();
  result.conflict_count = response.num_conflicts();
  result.branch_count = response.num_branches();
  result.wall_time_seconds = response.wall_time();
  switch (response.status()) {
    case operations_research::sat::CpSolverStatus::OPTIMAL:
      result.status = ExactGriddedLegalizationStatus::kOptimal;
      break;
    case operations_research::sat::CpSolverStatus::FEASIBLE:
      result.status = ExactGriddedLegalizationStatus::kFeasible;
      break;
    case operations_research::sat::CpSolverStatus::INFEASIBLE:
      result.status = ExactGriddedLegalizationStatus::kInfeasible;
      result.message = "exact gridded legalization model is infeasible";
      return result;
    case operations_research::sat::CpSolverStatus::MODEL_INVALID:
      result.status = ExactGriddedLegalizationStatus::kInvalidModel;
      result.message = response.solution_info();
      return result;
    case operations_research::sat::CpSolverStatus::UNKNOWN:
      result.status = ExactGriddedLegalizationStatus::kUnknown;
      result.message = "solver stopped before finding a legal placement";
      return result;
    default:
      result.status = ExactGriddedLegalizationStatus::kUnknown;
      result.message = "solver returned an unrecognized status";
      return result;
  }

  result.objective_value = response.objective_value();
  result.relative_gap = std::max(0.0, response.objective_value() -
                                          response.best_objective_bound()) /
                        std::max(1.0, std::abs(response.objective_value()));
  for (const ExactGriddedNetVariables& net : net_variables) {
    int64_t span_x =
        operations_research::sat::SolutionIntegerValue(response,
                                                       net.maximum_x) -
        operations_research::sat::SolutionIntegerValue(response, net.minimum_x);
    int64_t span_y =
        operations_research::sat::SolutionIntegerValue(response,
                                                       net.maximum_y) -
        operations_research::sat::SolutionIntegerValue(response, net.minimum_y);
    result.weighted_hpwl +=
        net.weight *
        (span_x * model.distance_scale_x + span_y * model.distance_scale_y) /
        static_cast<double>(pin_scale);
  }

  result.cells.reserve(model.cells.size());
  for (size_t cell_index = 0; cell_index < model.cells.size(); ++cell_index) {
    const ExactGriddedCell& cell = model.cells[cell_index];
    const ExactGriddedCellVariables& variables = cell_variables[cell_index];
    int x = static_cast<int>(
        operations_research::sat::SolutionIntegerValue(response, variables.x));
    int y = static_cast<int>(
        operations_research::sat::SolutionIntegerValue(response, variables.y));
    for (size_t candidate_index : variables.candidate_indices) {
      const ExactGriddedCandidateVariables& candidate =
          candidate_variables[candidate_index];
      if (!operations_research::sat::SolutionBooleanValue(response,
                                                          candidate.presence)) {
        continue;
      }
      result.cells.push_back({cell.component_id,
                              model.stripes[candidate.stripe_index].stripe_id,
                              candidate.start_row, x, y, candidate.is_flipped});
      break;
    }
    result.total_displacement +=
        std::abs(static_cast<int64_t>(x) - cell.initial_x) +
        std::abs(static_cast<int64_t>(y) - cell.initial_y);
  }

  for (size_t stripe_index = 0; stripe_index < model.stripes.size();
       ++stripe_index) {
    const ExactGriddedStripe& stripe = model.stripes[stripe_index];
    bool first_row_orient_n = operations_research::sat::SolutionBooleanValue(
        response, first_row_orient_n_variables[stripe_index]);
    for (int row_index = 0; row_index < stripe.maximum_rows; ++row_index) {
      const ExactGriddedRowVariables& row =
          row_variables[stripe_index][row_index];
      if (!operations_research::sat::SolutionBooleanValue(response,
                                                          row.active)) {
        continue;
      }
      result.rows.push_back(
          {stripe.stripe_id, row_index,
           static_cast<int>(
               operations_research::sat::SolutionIntegerValue(response, row.y)),
           static_cast<int>(operations_research::sat::SolutionIntegerValue(
               response, row.p_well_height)),
           static_cast<int>(operations_research::sat::SolutionIntegerValue(
               response, row.n_well_height)),
           (row_index % 2 == 0) ? first_row_orient_n : !first_row_orient_n});
    }
  }
  result.message =
      response.status() == operations_research::sat::CpSolverStatus::OPTIMAL
          ? "optimal legal placement"
          : "feasible legal placement";
  return result;
#endif
}

}  // namespace dali
