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
#include "dali/placer/well_legalizer/ortools_compact_gridded_legalizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

#include "dali/placer/well_legalizer/exact_gridded_legalization_util.h"

#ifdef DALI_HAS_OR_TOOLS
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/sorted_interval_list.h"
#endif

namespace dali {

bool OrToolsCompactGriddedLegalizer::IsAvailable() {
  return OrToolsExactGriddedLegalizer::IsAvailable();
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

/** CP-SAT variables for one physical row slot. */
struct CompactGriddedRowVariables {
  BoolVar active;
  BoolVar orient_n;
  IntVar y;
  IntVar p_well_height;
  IntVar n_well_height;
  std::vector<IntervalVar> component_intervals;
};

/** CP-SAT variables and coordinate bounds for one movable component. */
struct CompactGriddedCellVariables {
  IntVar x;
  IntVar y;
  IntVar start_choice;
  BoolVar is_flipped;
  std::vector<int> candidate_start_slots;
  int minimum_x = 0;
  int maximum_x = 0;
  int minimum_y = 0;
  int maximum_y = 0;
};

/** Min/max variables retained for physical HPWL reporting. */
struct CompactGriddedNetVariables {
  IntVar minimum_x;
  IntVar maximum_x;
  IntVar minimum_y;
  IntVar maximum_y;
  double weight = 0.0;
};

/** Convert an OR-Tools status into Dali's solver-independent status. */
ExactGriddedLegalizationStatus CompactGriddedStatus(
    operations_research::sat::CpSolverStatus status) {
  switch (status) {
    case operations_research::sat::CpSolverStatus::OPTIMAL:
      return ExactGriddedLegalizationStatus::kOptimal;
    case operations_research::sat::CpSolverStatus::FEASIBLE:
      return ExactGriddedLegalizationStatus::kFeasible;
    case operations_research::sat::CpSolverStatus::INFEASIBLE:
      return ExactGriddedLegalizationStatus::kInfeasible;
    case operations_research::sat::CpSolverStatus::MODEL_INVALID:
      return ExactGriddedLegalizationStatus::kInvalidModel;
    case operations_research::sat::CpSolverStatus::UNKNOWN:
      return ExactGriddedLegalizationStatus::kUnknown;
    default:
      return ExactGriddedLegalizationStatus::kUnknown;
  }
}

/** Return the physical weighted HPWL represented by solved net extrema. */
double ExtractCompactGriddedWeightedHpwl(
    const CpSolverResponse& response,
    const std::vector<CompactGriddedNetVariables>& nets,
    const ExactGriddedLegalizationModel& model, int64_t pin_scale) {
  double weighted_hpwl = 0.0;
  for (const CompactGriddedNetVariables& net : nets) {
    int64_t span_x =
        operations_research::sat::SolutionIntegerValue(response,
                                                       net.maximum_x) -
        operations_research::sat::SolutionIntegerValue(response, net.minimum_x);
    int64_t span_y =
        operations_research::sat::SolutionIntegerValue(response,
                                                       net.maximum_y) -
        operations_research::sat::SolutionIntegerValue(response, net.minimum_y);
    weighted_hpwl +=
        net.weight *
        (span_x * model.distance_scale_x + span_y * model.distance_scale_y) /
        static_cast<double>(pin_scale);
  }
  return weighted_hpwl;
}

/** Return `normal_value` or `flipped_value` according to the flip bit. */
LinearExpr SelectCompactOrientationValue(BoolVar is_flipped,
                                         int64_t normal_value,
                                         int64_t flipped_value) {
  return LinearExpr(normal_value) +
         LinearExpr::Term(is_flipped, flipped_value - normal_value);
}

#endif  // DALI_HAS_OR_TOOLS

ExactGriddedLegalizationResult OrToolsCompactGriddedLegalizer::Solve(
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
  const std::string validation_error = model.Validate();
  if (!validation_error.empty()) {
    result.status = ExactGriddedLegalizationStatus::kInvalidModel;
    result.message = validation_error;
    return result;
  }
  if (config.maximum_time_seconds <= 0.0 || config.number_of_workers <= 0 ||
      config.pin_coordinate_scale <= 0 ||
      config.maximum_row_displacement < -1 ||
      config.maximum_row_assignment_changes < -1) {
    result.status = ExactGriddedLegalizationStatus::kInvalidModel;
    result.message =
        "solver time, worker count, pin scale, row radius, or row-change "
        "budget is invalid";
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
  std::unordered_map<int, size_t> cell_indices;
  for (size_t index = 0; index < model.stripes.size(); ++index) {
    stripe_indices.emplace(model.stripes[index].stripe_id, index);
  }
  for (size_t index = 0; index < model.cells.size(); ++index) {
    cell_indices.emplace(model.cells[index].component_id, index);
  }

  CpModelBuilder cp_model;
  DoubleLinearExpr objective;
  std::vector<BoolVar> stripe_phase_variables;
  std::vector<std::vector<CompactGriddedRowVariables> > stripe_row_variables;
  std::vector<CompactGriddedRowVariables*> flat_rows;
  std::vector<IntVar> flat_row_active;
  std::vector<IntVar> flat_row_orient_n;
  std::vector<IntVar> flat_row_y;
  std::vector<IntVar> flat_row_p_height;
  std::vector<IntVar> flat_row_n_height;
  std::vector<int64_t> flat_row_lx;
  std::vector<int64_t> flat_row_ly;
  std::vector<int64_t> flat_row_ux;
  std::vector<int64_t> flat_row_uy;
  std::vector<size_t> flat_row_stripe_indices;
  std::vector<int> flat_row_local_indices;
  std::vector<int> stripe_slot_offsets(model.stripes.size(), 0);
  std::vector<int> stripe_phase_hints(model.stripes.size(), -1);

  int total_row_slots = 0;
  for (size_t stripe_index = 0; stripe_index < model.stripes.size();
       ++stripe_index) {
    stripe_slot_offsets[stripe_index] = total_row_slots;
    total_row_slots += model.stripes[stripe_index].maximum_rows;
  }
  stripe_phase_variables.reserve(model.stripes.size());
  stripe_row_variables.reserve(model.stripes.size());
  flat_rows.reserve(total_row_slots);
  flat_row_active.reserve(total_row_slots);
  flat_row_orient_n.reserve(total_row_slots);
  flat_row_y.reserve(total_row_slots);
  flat_row_p_height.reserve(total_row_slots);
  flat_row_n_height.reserve(total_row_slots);
  flat_row_lx.reserve(total_row_slots);
  flat_row_ly.reserve(total_row_slots);
  flat_row_ux.reserve(total_row_slots);
  flat_row_uy.reserve(total_row_slots);
  flat_row_stripe_indices.reserve(total_row_slots);
  flat_row_local_indices.reserve(total_row_slots);

  for (size_t stripe_index = 0; stripe_index < model.stripes.size();
       ++stripe_index) {
    const ExactGriddedStripe& stripe = model.stripes[stripe_index];
    BoolVar phase = cp_model.NewBoolVar().WithName(
        "compact_stripe_phase_" + std::to_string(stripe.stripe_id));
    stripe_phase_variables.push_back(phase);
    std::vector<CompactGriddedRowVariables> rows;
    rows.reserve(stripe.maximum_rows);
    const int stripe_height = stripe.uy - stripe.ly;
    for (int row_index = 0; row_index < stripe.maximum_rows; ++row_index) {
      const std::string suffix =
          std::to_string(stripe.stripe_id) + "_" + std::to_string(row_index);
      CompactGriddedRowVariables row;
      row.active =
          cp_model.NewBoolVar().WithName("compact_row_active_" + suffix);
      row.orient_n =
          cp_model.NewBoolVar().WithName("compact_row_orient_n_" + suffix);
      row.y = cp_model.NewIntVar(Domain(stripe.ly, stripe.uy))
                  .WithName("compact_row_y_" + suffix);
      row.p_well_height = cp_model.NewIntVar(Domain(0, stripe_height))
                              .WithName("compact_row_p_height_" + suffix);
      row.n_well_height = cp_model.NewIntVar(Domain(0, stripe_height))
                              .WithName("compact_row_n_height_" + suffix);
      cp_model
          .AddGreaterOrEqual(row.p_well_height, stripe.minimum_p_well_height)
          .OnlyEnforceIf(row.active);
      cp_model
          .AddGreaterOrEqual(row.n_well_height, stripe.minimum_n_well_height)
          .OnlyEnforceIf(row.active);
      cp_model.AddEquality(row.p_well_height, 0)
          .OnlyEnforceIf(row.active.Not());
      cp_model.AddEquality(row.n_well_height, 0)
          .OnlyEnforceIf(row.active.Not());
      if (row_index % 2 == 0) {
        cp_model.AddEquality(row.orient_n, phase);
      } else {
        cp_model.AddEquality(LinearExpr(row.orient_n) + phase, 1);
      }
      if (row_index == 0) {
        cp_model.AddGreaterOrEqual(row.y, stripe.ly);
      } else {
        CompactGriddedRowVariables& previous = rows.back();
        cp_model.AddGreaterOrEqual(previous.active, row.active);
        cp_model.AddGreaterOrEqual(row.y, previous.y + previous.p_well_height +
                                              previous.n_well_height);
      }
      if (!stripe.initial_rows.empty()) {
        const ExactGriddedRowHint& hint = stripe.initial_rows[row_index];
        cp_model.AddHint(row.active, hint.active);
        cp_model.AddHint(row.y, hint.y);
        cp_model.AddHint(row.p_well_height, hint.p_well_height);
        cp_model.AddHint(row.n_well_height, hint.n_well_height);
        if (config.fix_row_geometry) {
          cp_model.AddEquality(row.active, hint.active);
          cp_model.AddEquality(row.y, hint.y);
          cp_model.AddEquality(row.p_well_height, hint.p_well_height);
          cp_model.AddEquality(row.n_well_height, hint.n_well_height);
        }
      }
      rows.push_back(std::move(row));
    }
    CompactGriddedRowVariables& last_row = rows.back();
    cp_model.AddLessOrEqual(
        last_row.y + last_row.p_well_height + last_row.n_well_height,
        stripe.uy);
    stripe_row_variables.push_back(std::move(rows));
  }

  for (size_t stripe_index = 0; stripe_index < model.stripes.size();
       ++stripe_index) {
    const ExactGriddedStripe& stripe = model.stripes[stripe_index];
    for (int row_index = 0; row_index < stripe.maximum_rows; ++row_index) {
      CompactGriddedRowVariables& row =
          stripe_row_variables[stripe_index][row_index];
      flat_rows.push_back(&row);
      flat_row_active.emplace_back(row.active);
      flat_row_orient_n.emplace_back(row.orient_n);
      flat_row_y.push_back(row.y);
      flat_row_p_height.push_back(row.p_well_height);
      flat_row_n_height.push_back(row.n_well_height);
      flat_row_lx.push_back(stripe.lx + stripe.left_boundary_margin);
      flat_row_ly.push_back(stripe.ly);
      flat_row_ux.push_back(stripe.ux - stripe.right_boundary_margin);
      flat_row_uy.push_back(stripe.uy);
      flat_row_stripe_indices.push_back(stripe_index);
      flat_row_local_indices.push_back(row_index);
    }
  }

  std::vector<CompactGriddedCellVariables> cell_variables;
  std::vector<int> hinted_cell_x;
  std::vector<int> hinted_cell_y;
  std::vector<BoolVar> row_assignment_changes;
  cell_variables.reserve(model.cells.size());
  hinted_cell_x.reserve(model.cells.size());
  hinted_cell_y.reserve(model.cells.size());
  row_assignment_changes.reserve(model.cells.size());
  auto no_overlap = cp_model.AddNoOverlap2D();
  const bool use_fixed_row_no_overlap =
      config.maximum_row_displacement == 0 &&
      std::all_of(model.cells.begin(), model.cells.end(),
                  [](const ExactGriddedCell& cell) {
                    return cell.candidate_stripe_ids.size() == 1 &&
                           cell.initial_start_row >= 0;
                  });

  for (size_t cell_index = 0; cell_index < model.cells.size(); ++cell_index) {
    const ExactGriddedCell& cell = model.cells[cell_index];
    std::vector<int64_t> legal_start_slots;
    int minimum_x = std::numeric_limits<int>::max();
    int maximum_x = std::numeric_limits<int>::min();
    int minimum_y = std::numeric_limits<int>::max();
    int maximum_y = std::numeric_limits<int>::min();
    for (int stripe_id : cell.candidate_stripe_ids) {
      const size_t stripe_index = stripe_indices.at(stripe_id);
      const ExactGriddedStripe& stripe = model.stripes[stripe_index];
      int first_start = 0;
      int last_start =
          stripe.maximum_rows - static_cast<int>(cell.regions.size());
      if (config.maximum_row_displacement >= 0 && cell.initial_start_row >= 0) {
        first_start =
            std::max(first_start,
                     cell.initial_start_row - config.maximum_row_displacement);
        last_start = std::min(last_start, cell.initial_start_row +
                                              config.maximum_row_displacement);
      }
      for (int start_row = first_start; start_row <= last_start; ++start_row) {
        legal_start_slots.push_back(stripe_slot_offsets[stripe_index] +
                                    start_row);
      }
      minimum_x = std::min(minimum_x, stripe.lx + stripe.left_boundary_margin);
      maximum_x = std::max(
          maximum_x, stripe.ux - stripe.right_boundary_margin - cell.width);
      minimum_y = std::min(minimum_y, stripe.ly);
      maximum_y = std::max(maximum_y, stripe.uy - cell.height);
    }
    if (legal_start_slots.empty() || minimum_x > maximum_x ||
        minimum_y > maximum_y) {
      result.status = ExactGriddedLegalizationStatus::kInvalidModel;
      result.message = "component has no compact legal placement domain";
      return result;
    }
    result.row_assignment_choice_count += legal_start_slots.size();

    CompactGriddedCellVariables variables;
    variables.minimum_x = minimum_x;
    variables.maximum_x = maximum_x;
    variables.minimum_y = minimum_y;
    variables.maximum_y = maximum_y;
    variables.x =
        cp_model.NewIntVar(Domain(minimum_x, maximum_x))
            .WithName("compact_cell_x_" + std::to_string(cell.component_id));
    variables.y =
        cp_model.NewIntVar(Domain(minimum_y, maximum_y))
            .WithName("compact_cell_y_" + std::to_string(cell.component_id));
    variables.start_choice =
        cp_model.NewIntVar(Domain(0, legal_start_slots.size() - 1))
            .WithName("compact_cell_row_choice_" +
                      std::to_string(cell.component_id));
    variables.candidate_start_slots.reserve(legal_start_slots.size());
    for (int64_t slot : legal_start_slots) {
      variables.candidate_start_slots.push_back(static_cast<int>(slot));
    }
    variables.is_flipped = cp_model.NewBoolVar().WithName(
        "compact_cell_flipped_" + std::to_string(cell.component_id));

    std::vector<int64_t> candidate_lx;
    std::vector<int64_t> candidate_ly;
    std::vector<int64_t> candidate_ux;
    std::vector<int64_t> candidate_uy;
    candidate_lx.reserve(legal_start_slots.size());
    candidate_ly.reserve(legal_start_slots.size());
    candidate_ux.reserve(legal_start_slots.size());
    candidate_uy.reserve(legal_start_slots.size());
    for (int64_t slot : legal_start_slots) {
      candidate_lx.push_back(flat_row_lx[slot]);
      candidate_ly.push_back(flat_row_ly[slot]);
      candidate_ux.push_back(flat_row_ux[slot]);
      candidate_uy.push_back(flat_row_uy[slot]);
    }
    const bool uses_one_stripe =
        std::all_of(candidate_lx.begin(), candidate_lx.end(),
                    [&](int64_t value) { return value == candidate_lx[0]; }) &&
        std::all_of(candidate_ly.begin(), candidate_ly.end(),
                    [&](int64_t value) { return value == candidate_ly[0]; }) &&
        std::all_of(candidate_ux.begin(), candidate_ux.end(),
                    [&](int64_t value) { return value == candidate_ux[0]; }) &&
        std::all_of(candidate_uy.begin(), candidate_uy.end(),
                    [&](int64_t value) { return value == candidate_uy[0]; });
    if (uses_one_stripe) {
      cp_model.AddGreaterOrEqual(variables.x, candidate_lx[0]);
      cp_model.AddLessOrEqual(variables.x + cell.width, candidate_ux[0]);
      cp_model.AddGreaterOrEqual(variables.y, candidate_ly[0]);
      cp_model.AddLessOrEqual(variables.y + cell.height, candidate_uy[0]);
    } else {
      IntVar selected_lx = cp_model.NewIntVar(Domain(minimum_x, maximum_x));
      IntVar selected_ly = cp_model.NewIntVar(Domain(minimum_y, maximum_y));
      IntVar selected_ux = cp_model.NewIntVar(
          Domain(minimum_x + cell.width, maximum_x + cell.width));
      IntVar selected_uy = cp_model.NewIntVar(
          Domain(minimum_y + cell.height, maximum_y + cell.height));
      cp_model.AddElement(variables.start_choice, candidate_lx, selected_lx);
      cp_model.AddElement(variables.start_choice, candidate_ly, selected_ly);
      cp_model.AddElement(variables.start_choice, candidate_ux, selected_ux);
      cp_model.AddElement(variables.start_choice, candidate_uy, selected_uy);
      cp_model.AddGreaterOrEqual(variables.x, selected_lx);
      cp_model.AddLessOrEqual(variables.x + cell.width, selected_ux);
      cp_model.AddGreaterOrEqual(variables.y, selected_ly);
      cp_model.AddLessOrEqual(variables.y + cell.height, selected_uy);
    }

    IntVar first_row_y;
    IntVar first_row_p_height;
    IntVar first_row_n_height;
    for (int region_index = 0;
         region_index < static_cast<int>(cell.regions.size()); ++region_index) {
      const ExactGriddedCellRegion& normal_region = cell.regions[region_index];
      const ExactGriddedCellRegion& source_flipped_region =
          cell.regions[cell.regions.size() - 1 - region_index];
      const bool normal_orient_n = normal_region.n_well_above_p_well;
      const bool flipped_orient_n = !source_flipped_region.n_well_above_p_well;
      std::vector<IntVar> candidate_active;
      std::vector<IntVar> candidate_orient_n;
      std::vector<IntVar> candidate_y;
      std::vector<IntVar> candidate_p_height;
      std::vector<IntVar> candidate_n_height;
      candidate_active.reserve(legal_start_slots.size());
      candidate_orient_n.reserve(legal_start_slots.size());
      candidate_y.reserve(legal_start_slots.size());
      candidate_p_height.reserve(legal_start_slots.size());
      candidate_n_height.reserve(legal_start_slots.size());
      for (int64_t start_slot : legal_start_slots) {
        const size_t selected_slot = start_slot + region_index;
        candidate_active.push_back(flat_row_active[selected_slot]);
        candidate_orient_n.push_back(flat_row_orient_n[selected_slot]);
        candidate_y.push_back(flat_row_y[selected_slot]);
        candidate_p_height.push_back(flat_row_p_height[selected_slot]);
        candidate_n_height.push_back(flat_row_n_height[selected_slot]);
      }
      IntVar selected_active;
      IntVar selected_orient_n;
      IntVar selected_y;
      IntVar selected_p_height;
      IntVar selected_n_height;
      if (candidate_active.size() == 1) {
        selected_active = candidate_active[0];
        selected_orient_n = candidate_orient_n[0];
        selected_y = candidate_y[0];
        selected_p_height = candidate_p_height[0];
        selected_n_height = candidate_n_height[0];
      } else {
        selected_active = cp_model.NewIntVar(Domain(0, 1));
        selected_orient_n = cp_model.NewIntVar(Domain(0, 1));
        selected_y =
            cp_model.NewIntVar(Domain(minimum_y, maximum_y + cell.height));
        selected_p_height =
            cp_model.NewIntVar(Domain(0, maximum_y - minimum_y + cell.height));
        selected_n_height =
            cp_model.NewIntVar(Domain(0, maximum_y - minimum_y + cell.height));
        cp_model.AddVariableElement(variables.start_choice, candidate_active,
                                    selected_active);
        cp_model.AddVariableElement(variables.start_choice, candidate_orient_n,
                                    selected_orient_n);
        cp_model.AddVariableElement(variables.start_choice, candidate_y,
                                    selected_y);
        cp_model.AddVariableElement(variables.start_choice, candidate_p_height,
                                    selected_p_height);
        cp_model.AddVariableElement(variables.start_choice, candidate_n_height,
                                    selected_n_height);
      }
      cp_model.AddEquality(selected_active, 1);
      cp_model.AddEquality(selected_orient_n,
                           SelectCompactOrientationValue(
                               variables.is_flipped, normal_orient_n ? 1 : 0,
                               flipped_orient_n ? 1 : 0));
      cp_model.AddGreaterOrEqual(
          selected_p_height,
          SelectCompactOrientationValue(variables.is_flipped,
                                        normal_region.p_well_height,
                                        source_flipped_region.p_well_height));
      cp_model.AddGreaterOrEqual(
          selected_n_height,
          SelectCompactOrientationValue(variables.is_flipped,
                                        normal_region.n_well_height,
                                        source_flipped_region.n_well_height));
      if (region_index == 0) {
        first_row_y = selected_y;
        first_row_p_height = selected_p_height;
        first_row_n_height = selected_n_height;
      }
    }

    const ExactGriddedCellRegion& normal_first_region = cell.regions.front();
    const ExactGriddedCellRegion& flipped_first_source = cell.regions.back();
    LinearExpr normal_y = first_row_y;
    if (normal_first_region.n_well_above_p_well) {
      normal_y += first_row_p_height;
      normal_y -= normal_first_region.p_well_height;
    } else {
      normal_y += first_row_n_height;
      normal_y -= normal_first_region.n_well_height;
    }
    LinearExpr flipped_y = first_row_y;
    if (!flipped_first_source.n_well_above_p_well) {
      flipped_y += first_row_p_height;
      flipped_y -= flipped_first_source.p_well_height;
    } else {
      flipped_y += first_row_n_height;
      flipped_y -= flipped_first_source.n_well_height;
    }
    cp_model.AddEquality(variables.y, normal_y)
        .OnlyEnforceIf(variables.is_flipped.Not());
    cp_model.AddEquality(variables.y, flipped_y)
        .OnlyEnforceIf(variables.is_flipped);

    const int cell_hint_x = std::clamp(cell.initial_x, minimum_x, maximum_x);
    const int cell_hint_y = std::clamp(cell.initial_y, minimum_y, maximum_y);
    cp_model.AddHint(variables.x, cell_hint_x);
    cp_model.AddHint(variables.y, cell_hint_y);
    if (config.fix_cell_x) {
      cp_model.AddEquality(variables.x, cell_hint_x);
    }
    hinted_cell_x.push_back(cell_hint_x);
    hinted_cell_y.push_back(cell_hint_y);
    if (cell.initial_stripe_id >= 0 && cell.initial_start_row >= 0) {
      const size_t initial_stripe_index =
          stripe_indices.at(cell.initial_stripe_id);
      const int initial_slot =
          stripe_slot_offsets[initial_stripe_index] + cell.initial_start_row;
      const auto initial_choice =
          std::find(variables.candidate_start_slots.begin(),
                    variables.candidate_start_slots.end(), initial_slot);
      if (initial_choice == variables.candidate_start_slots.end()) {
        result.status = ExactGriddedLegalizationStatus::kInvalidModel;
        result.message = "component hint is outside its compact row domain";
        return result;
      }
      const int initial_choice_index = static_cast<int>(std::distance(
          variables.candidate_start_slots.begin(), initial_choice));
      cp_model.AddHint(variables.start_choice, initial_choice_index);
      cp_model.AddHint(variables.is_flipped, cell.initial_is_flipped);
      if (config.fix_cell_orientation) {
        cp_model.AddEquality(variables.is_flipped, cell.initial_is_flipped);
      }
      if (config.maximum_row_assignment_changes >= 0) {
        BoolVar assignment_changed = cp_model.NewBoolVar().WithName(
            "compact_cell_row_changed_" + std::to_string(cell.component_id));
        cp_model.AddNotEqual(variables.start_choice, initial_choice_index)
            .OnlyEnforceIf(assignment_changed);
        cp_model.AddEquality(variables.start_choice, initial_choice_index)
            .OnlyEnforceIf(assignment_changed.Not());
        cp_model.AddHint(assignment_changed, false);
        row_assignment_changes.push_back(assignment_changed);
      }
      bool required_phase = true;
      if (!ExactGriddedCandidateMatchesAlternatingRows(
              cell, cell.initial_start_row, cell.initial_is_flipped,
              &required_phase)) {
        result.status = ExactGriddedLegalizationStatus::kInvalidModel;
        result.message = "component hint violates alternating row orientations";
        return result;
      }
      int& phase_hint = stripe_phase_hints[initial_stripe_index];
      const int required_phase_value = required_phase ? 1 : 0;
      if (phase_hint >= 0 && phase_hint != required_phase_value) {
        result.status = ExactGriddedLegalizationStatus::kInvalidModel;
        result.message = "component hints require inconsistent stripe phases";
        return result;
      }
      phase_hint = required_phase_value;
    }

    if (use_fixed_row_no_overlap) {
      const int start_slot = variables.candidate_start_slots[0];
      for (int region_index = 0;
           region_index < static_cast<int>(cell.regions.size());
           ++region_index) {
        flat_rows[start_slot + region_index]->component_intervals.push_back(
            cp_model.NewFixedSizeIntervalVar(variables.x, cell.width));
      }
    } else {
      no_overlap.AddRectangle(
          cp_model.NewFixedSizeIntervalVar(variables.x, cell.width),
          cp_model.NewFixedSizeIntervalVar(variables.y, cell.height));
    }
    cell_variables.push_back(std::move(variables));
  }

  if (config.maximum_row_assignment_changes >= 0) {
    if (row_assignment_changes.size() != model.cells.size()) {
      result.status = ExactGriddedLegalizationStatus::kInvalidModel;
      result.message =
          "row-change budget requires a row assignment hint for every cell";
      return result;
    }
    cp_model.AddLessOrEqual(LinearExpr::Sum(row_assignment_changes),
                            config.maximum_row_assignment_changes);
  }

  if (use_fixed_row_no_overlap) {
    for (CompactGriddedRowVariables* row : flat_rows) {
      if (!row->component_intervals.empty()) {
        cp_model.AddNoOverlap(row->component_intervals);
      }
    }
  }

  for (size_t stripe_index = 0; stripe_index < stripe_phase_hints.size();
       ++stripe_index) {
    if (stripe_phase_hints[stripe_index] >= 0) {
      const bool first_row_orient_n = stripe_phase_hints[stripe_index] != 0;
      cp_model.AddHint(stripe_phase_variables[stripe_index],
                       first_row_orient_n);
      if (config.fix_row_geometry) {
        cp_model.AddEquality(stripe_phase_variables[stripe_index],
                             first_row_orient_n);
      }
      for (int row_index = 0;
           row_index < model.stripes[stripe_index].maximum_rows; ++row_index) {
        cp_model.AddHint(
            stripe_row_variables[stripe_index][row_index].orient_n,
            row_index % 2 == 0 ? first_row_orient_n : !first_row_orient_n);
      }
    }
  }

  if (config.displacement_weight > 0.0) {
    for (size_t cell_index = 0; cell_index < model.cells.size(); ++cell_index) {
      const ExactGriddedCell& cell = model.cells[cell_index];
      const CompactGriddedCellVariables& variables = cell_variables[cell_index];
      const int maximum_x_displacement =
          std::max(std::abs(variables.minimum_x - cell.initial_x),
                   std::abs(variables.maximum_x - cell.initial_x));
      const int maximum_y_displacement =
          std::max(std::abs(variables.minimum_y - cell.initial_y),
                   std::abs(variables.maximum_y - cell.initial_y));
      IntVar x_displacement =
          cp_model.NewIntVar(Domain(0, maximum_x_displacement));
      IntVar y_displacement =
          cp_model.NewIntVar(Domain(0, maximum_y_displacement));
      cp_model.AddAbsEquality(x_displacement, variables.x - cell.initial_x);
      cp_model.AddAbsEquality(y_displacement, variables.y - cell.initial_y);
      cp_model.AddHint(x_displacement,
                       std::abs(hinted_cell_x[cell_index] - cell.initial_x));
      cp_model.AddHint(y_displacement,
                       std::abs(hinted_cell_y[cell_index] - cell.initial_y));
      objective.AddTerm(x_displacement,
                        config.displacement_weight * model.distance_scale_x);
      objective.AddTerm(y_displacement,
                        config.displacement_weight * model.distance_scale_y);
    }
  }

  const int64_t pin_scale = config.pin_coordinate_scale;
  std::vector<CompactGriddedNetVariables> net_variables;
  net_variables.reserve(model.nets.size());
  for (size_t net_index = 0; net_index < model.nets.size(); ++net_index) {
    const ExactGriddedNet& net = model.nets[net_index];
    if (net.pins.size() < 2 || net.weight == 0.0) continue;

    std::vector<LinearExpr> pin_x_locations;
    std::vector<LinearExpr> pin_y_locations;
    std::vector<int64_t> hinted_pin_x;
    std::vector<int64_t> hinted_pin_y;
    int64_t minimum_x = std::numeric_limits<int64_t>::max();
    int64_t maximum_x = std::numeric_limits<int64_t>::min();
    int64_t minimum_y = std::numeric_limits<int64_t>::max();
    int64_t maximum_y = std::numeric_limits<int64_t>::min();
    for (size_t pin_index = 0; pin_index < net.pins.size(); ++pin_index) {
      const ExactGriddedNetPin& pin = net.pins[pin_index];
      if (pin.component_id < 0) {
        const int64_t fixed_x =
            std::llround(pin.fixed_x * static_cast<double>(pin_scale));
        const int64_t fixed_y =
            std::llround(pin.fixed_y * static_cast<double>(pin_scale));
        pin_x_locations.emplace_back(fixed_x);
        pin_y_locations.emplace_back(fixed_y);
        hinted_pin_x.push_back(fixed_x);
        hinted_pin_y.push_back(fixed_y);
        minimum_x = std::min(minimum_x, fixed_x);
        maximum_x = std::max(maximum_x, fixed_x);
        minimum_y = std::min(minimum_y, fixed_y);
        maximum_y = std::max(maximum_y, fixed_y);
        continue;
      }

      const size_t cell_index = cell_indices.at(pin.component_id);
      const CompactGriddedCellVariables& variables = cell_variables[cell_index];
      const int64_t offset_x_n = std::llround(pin.offset_x_n * pin_scale);
      const int64_t offset_x_fs = std::llround(pin.offset_x_fs * pin_scale);
      const int64_t offset_y_n = std::llround(pin.offset_y_n * pin_scale);
      const int64_t offset_y_fs = std::llround(pin.offset_y_fs * pin_scale);
      const int64_t pin_minimum_x =
          static_cast<int64_t>(variables.minimum_x) * pin_scale +
          std::min(offset_x_n, offset_x_fs);
      const int64_t pin_maximum_x =
          static_cast<int64_t>(variables.maximum_x) * pin_scale +
          std::max(offset_x_n, offset_x_fs);
      const int64_t pin_minimum_y =
          static_cast<int64_t>(variables.minimum_y) * pin_scale +
          std::min(offset_y_n, offset_y_fs);
      const int64_t pin_maximum_y =
          static_cast<int64_t>(variables.maximum_y) * pin_scale +
          std::max(offset_y_n, offset_y_fs);
      IntVar pin_x = cp_model.NewIntVar(Domain(pin_minimum_x, pin_maximum_x));
      IntVar pin_y = cp_model.NewIntVar(Domain(pin_minimum_y, pin_maximum_y));
      cp_model.AddEquality(
          pin_x, LinearExpr::Term(variables.x, pin_scale) +
                     SelectCompactOrientationValue(variables.is_flipped,
                                                   offset_x_n, offset_x_fs));
      cp_model.AddEquality(
          pin_y, LinearExpr::Term(variables.y, pin_scale) +
                     SelectCompactOrientationValue(variables.is_flipped,
                                                   offset_y_n, offset_y_fs));
      const ExactGriddedCell& cell = model.cells[cell_index];
      const int64_t pin_hint_x =
          static_cast<int64_t>(hinted_cell_x[cell_index]) * pin_scale +
          (cell.initial_is_flipped ? offset_x_fs : offset_x_n);
      const int64_t pin_hint_y =
          static_cast<int64_t>(hinted_cell_y[cell_index]) * pin_scale +
          (cell.initial_is_flipped ? offset_y_fs : offset_y_n);
      cp_model.AddHint(pin_x, pin_hint_x);
      cp_model.AddHint(pin_y, pin_hint_y);
      hinted_pin_x.push_back(pin_hint_x);
      hinted_pin_y.push_back(pin_hint_y);
      pin_x_locations.push_back(pin_x);
      pin_y_locations.push_back(pin_y);
      minimum_x = std::min(minimum_x, pin_minimum_x);
      maximum_x = std::max(maximum_x, pin_maximum_x);
      minimum_y = std::min(minimum_y, pin_minimum_y);
      maximum_y = std::max(maximum_y, pin_maximum_y);
    }

    CompactGriddedNetVariables variables;
    variables.minimum_x = cp_model.NewIntVar(Domain(minimum_x, maximum_x));
    variables.maximum_x = cp_model.NewIntVar(Domain(minimum_x, maximum_x));
    variables.minimum_y = cp_model.NewIntVar(Domain(minimum_y, maximum_y));
    variables.maximum_y = cp_model.NewIntVar(Domain(minimum_y, maximum_y));
    variables.weight = net.weight;
    cp_model.AddMinEquality(variables.minimum_x, pin_x_locations);
    cp_model.AddMaxEquality(variables.maximum_x, pin_x_locations);
    cp_model.AddMinEquality(variables.minimum_y, pin_y_locations);
    cp_model.AddMaxEquality(variables.maximum_y, pin_y_locations);
    cp_model.AddHint(
        variables.minimum_x,
        *std::min_element(hinted_pin_x.begin(), hinted_pin_x.end()));
    cp_model.AddHint(
        variables.maximum_x,
        *std::max_element(hinted_pin_x.begin(), hinted_pin_x.end()));
    cp_model.AddHint(
        variables.minimum_y,
        *std::min_element(hinted_pin_y.begin(), hinted_pin_y.end()));
    cp_model.AddHint(
        variables.maximum_y,
        *std::max_element(hinted_pin_y.begin(), hinted_pin_y.end()));
    objective.AddExpression(variables.maximum_x - variables.minimum_x,
                            config.weighted_hpwl_weight * net.weight *
                                model.distance_scale_x /
                                static_cast<double>(pin_scale));
    objective.AddExpression(variables.maximum_y - variables.minimum_y,
                            config.weighted_hpwl_weight * net.weight *
                                model.distance_scale_y /
                                static_cast<double>(pin_scale));
    net_variables.push_back(std::move(variables));
  }
  cp_model.Minimize(objective);

  operations_research::sat::SatParameters parameters;
  parameters.set_max_time_in_seconds(config.maximum_time_seconds);
  parameters.set_num_search_workers(config.number_of_workers);
  parameters.set_log_search_progress(config.log_search_progress);
  parameters.set_repair_hint(config.use_solution_hint);
  parameters.set_cp_model_presolve(config.use_presolve);
  const auto& hinted_model_proto = cp_model.Build();
  result.model_variable_count = hinted_model_proto.variables_size();
  result.model_constraint_count = hinted_model_proto.constraints_size();

  if (config.validate_solution_hint) {
    operations_research::sat::SatParameters hint_parameters = parameters;
    hint_parameters.set_fix_variables_to_their_hinted_value(true);
    const CpSolverResponse hint_response =
        operations_research::sat::SolveWithParameters(hinted_model_proto,
                                                      hint_parameters);
    result.hint_validation_status =
        CompactGriddedStatus(hint_response.status());
    result.hint_validation_wall_time_seconds = hint_response.wall_time();
    if (result.hint_validation_status ==
            ExactGriddedLegalizationStatus::kFeasible ||
        result.hint_validation_status ==
            ExactGriddedLegalizationStatus::kOptimal) {
      result.hinted_weighted_hpwl = ExtractCompactGriddedWeightedHpwl(
          hint_response, net_variables, model, pin_scale);
    } else if (result.hint_validation_status ==
               ExactGriddedLegalizationStatus::kInfeasible) {
      result.hint_validation_message = ValidateExactSolutionHint(model);
      if (result.hint_validation_message.empty()) {
        result.hint_validation_message =
            "CP-SAT rejected a hint that passed deterministic validation";
      }
    } else {
      result.hint_validation_message = hint_response.solution_info();
    }
  }

  auto solve_model_proto = hinted_model_proto;
  if (!config.use_solution_hint) {
    solve_model_proto.clear_solution_hint();
  }
  const CpSolverResponse response =
      operations_research::sat::SolveWithParameters(solve_model_proto,
                                                    parameters);
  result.status = CompactGriddedStatus(response.status());
  result.best_objective_bound = response.best_objective_bound();
  result.conflict_count = response.num_conflicts();
  result.branch_count = response.num_branches();
  result.wall_time_seconds = response.wall_time();
  if (!result.HasSolution()) {
    result.message =
        response.status() ==
                operations_research::sat::CpSolverStatus::INFEASIBLE
            ? "compact gridded legalization model is infeasible"
            : response.solution_info();
    return result;
  }

  result.objective_value = response.objective_value();
  result.relative_gap = std::max(0.0, response.objective_value() -
                                          response.best_objective_bound()) /
                        std::max(1.0, std::abs(response.objective_value()));
  result.weighted_hpwl = ExtractCompactGriddedWeightedHpwl(
      response, net_variables, model, pin_scale);

  result.cells.reserve(model.cells.size());
  for (size_t cell_index = 0; cell_index < model.cells.size(); ++cell_index) {
    const ExactGriddedCell& cell = model.cells[cell_index];
    const CompactGriddedCellVariables& variables = cell_variables[cell_index];
    const int choice =
        static_cast<int>(operations_research::sat::SolutionIntegerValue(
            response, variables.start_choice));
    const int slot = variables.candidate_start_slots[choice];
    const int x = static_cast<int>(
        operations_research::sat::SolutionIntegerValue(response, variables.x));
    const int y = static_cast<int>(
        operations_research::sat::SolutionIntegerValue(response, variables.y));
    result.cells.push_back(
        {cell.component_id,
         model.stripes[flat_row_stripe_indices[slot]].stripe_id,
         flat_row_local_indices[slot], x, y,
         operations_research::sat::SolutionBooleanValue(response,
                                                        variables.is_flipped)});
    result.total_displacement +=
        std::abs(static_cast<int64_t>(x) - cell.initial_x) +
        std::abs(static_cast<int64_t>(y) - cell.initial_y);
  }

  for (size_t slot = 0; slot < flat_rows.size(); ++slot) {
    const CompactGriddedRowVariables& row = *flat_rows[slot];
    if (!operations_research::sat::SolutionBooleanValue(response, row.active)) {
      continue;
    }
    result.rows.push_back(
        {model.stripes[flat_row_stripe_indices[slot]].stripe_id,
         flat_row_local_indices[slot],
         static_cast<int>(
             operations_research::sat::SolutionIntegerValue(response, row.y)),
         static_cast<int>(operations_research::sat::SolutionIntegerValue(
             response, row.p_well_height)),
         static_cast<int>(operations_research::sat::SolutionIntegerValue(
             response, row.n_well_height)),
         operations_research::sat::SolutionBooleanValue(response,
                                                        row.orient_n)});
  }
  result.message = result.status == ExactGriddedLegalizationStatus::kOptimal
                       ? "optimal compact legal placement"
                       : "feasible compact legal placement";
  return result;
#endif  // DALI_HAS_OR_TOOLS
}

}  // namespace dali
