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

#include "dali/placer/well_legalizer/exact_gridded_legalization_util.h"

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
  BoolVar preserve_initial_well_heights;
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

/** One component interval and well demand assigned to a hinted row. */
struct ExactHintedRowOccupant {
  int component_id = -1;
  int x = 0;
  int width = 0;
  int p_well_height = 0;
  int n_well_height = 0;
};

/**
 * Return the first exact constraint violated by a complete placement hint.
 *
 * This deterministic check mirrors the physical CP-SAT constraints so an
 * infeasible production seed can be diagnosed without an opaque unsat result.
 */
std::string ValidateExactSolutionHint(
    const ExactGriddedLegalizationModel& model) {
  std::unordered_map<int, size_t> stripe_indices;
  for (size_t index = 0; index < model.stripes.size(); ++index) {
    stripe_indices.emplace(model.stripes[index].stripe_id, index);
    if (model.stripes[index].initial_rows.empty()) {
      return "solution hint does not include row geometry";
    }
  }

  std::vector<std::vector<std::vector<ExactHintedRowOccupant>>> occupants;
  occupants.reserve(model.stripes.size());
  for (const ExactGriddedStripe& stripe : model.stripes) {
    occupants.emplace_back(stripe.maximum_rows);
  }
  std::vector<int> stripe_phase(model.stripes.size(), -1);

  for (const ExactGriddedCell& cell : model.cells) {
    if (cell.initial_stripe_id < 0 || cell.initial_start_row < 0) {
      return "solution hint does not include every component assignment";
    }
    size_t stripe_index = stripe_indices.at(cell.initial_stripe_id);
    const ExactGriddedStripe& stripe = model.stripes[stripe_index];
    if (cell.initial_x < stripe.lx + stripe.left_boundary_margin ||
        cell.initial_x + cell.width >
            stripe.ux - stripe.right_boundary_margin) {
      return "component " + std::to_string(cell.component_id) +
             " lies outside its hinted stripe boundary";
    }

    bool required_first_row_orient_n = true;
    if (!ExactGriddedCandidateMatchesAlternatingRows(
            cell, cell.initial_start_row, cell.initial_is_flipped,
            &required_first_row_orient_n)) {
      return "component " + std::to_string(cell.component_id) +
             " violates alternating row orientations";
    }
    const int required_phase = required_first_row_orient_n ? 1 : 0;
    if (stripe_phase[stripe_index] >= 0 &&
        stripe_phase[stripe_index] != required_phase) {
      return "hinted components require inconsistent stripe orientations";
    }
    stripe_phase[stripe_index] = required_phase;

    const ExactGriddedRowHint& first_row =
        stripe.initial_rows[cell.initial_start_row];
    ExactGriddedCellRegion first_region =
        GetExactGriddedPhysicalRegion(cell, 0, cell.initial_is_flipped);
    int expected_y = first_row.y;
    if (first_region.n_well_above_p_well) {
      expected_y += first_row.p_well_height - first_region.p_well_height;
    } else {
      expected_y += first_row.n_well_height - first_region.n_well_height;
    }
    if (cell.initial_y != expected_y) {
      return "component " + std::to_string(cell.component_id) +
             " Y does not match its hinted row geometry: actual " +
             std::to_string(cell.initial_y) + ", expected " +
             std::to_string(expected_y) + ", row Y " +
             std::to_string(first_row.y) + ", row P/N heights " +
             std::to_string(first_row.p_well_height) + "/" +
             std::to_string(first_row.n_well_height) +
             ", component P/N heights " +
             std::to_string(first_region.p_well_height) + "/" +
             std::to_string(first_region.n_well_height) + ", flipped " +
             std::to_string(cell.initial_is_flipped);
    }

    for (int region_index = 0;
         region_index < static_cast<int>(cell.regions.size()); ++region_index) {
      ExactGriddedCellRegion region = GetExactGriddedPhysicalRegion(
          cell, region_index, cell.initial_is_flipped);
      occupants[stripe_index][cell.initial_start_row + region_index].push_back(
          {cell.component_id, cell.initial_x, cell.width, region.p_well_height,
           region.n_well_height});
    }
  }

  for (size_t stripe_index = 0; stripe_index < model.stripes.size();
       ++stripe_index) {
    const ExactGriddedStripe& stripe = model.stripes[stripe_index];
    for (int row_index = 0; row_index < stripe.maximum_rows; ++row_index) {
      const ExactGriddedRowHint& row = stripe.initial_rows[row_index];
      std::vector<ExactHintedRowOccupant>& row_occupants =
          occupants[stripe_index][row_index];
      if (row.active != !row_occupants.empty()) {
        return "stripe " + std::to_string(stripe.stripe_id) + " row " +
               std::to_string(row_index) +
               " activity does not match its occupants";
      }

      int expected_p_height = row.active ? stripe.minimum_p_well_height : 0;
      int expected_n_height = row.active ? stripe.minimum_n_well_height : 0;
      for (const ExactHintedRowOccupant& occupant : row_occupants) {
        expected_p_height = std::max(expected_p_height, occupant.p_well_height);
        expected_n_height = std::max(expected_n_height, occupant.n_well_height);
      }
      if (row.p_well_height < expected_p_height ||
          row.n_well_height < expected_n_height) {
        return "stripe " + std::to_string(stripe.stripe_id) + " row " +
               std::to_string(row_index) +
               " well heights are below occupied maxima";
      }

      std::sort(row_occupants.begin(), row_occupants.end(),
                [](const ExactHintedRowOccupant& first,
                   const ExactHintedRowOccupant& second) {
                  if (first.x != second.x) return first.x < second.x;
                  return first.component_id < second.component_id;
                });
      for (size_t occupant_index = 1; occupant_index < row_occupants.size();
           ++occupant_index) {
        const ExactHintedRowOccupant& previous =
            row_occupants[occupant_index - 1];
        const ExactHintedRowOccupant& current = row_occupants[occupant_index];
        if (previous.x + previous.width > current.x) {
          return "stripe " + std::to_string(stripe.stripe_id) + " row " +
                 std::to_string(row_index) + " components " +
                 std::to_string(previous.component_id) + " and " +
                 std::to_string(current.component_id) + " overlap";
        }
      }
    }
  }
  return "";
}

/** Return physical weighted HPWL represented by solved net-extrema variables.
 */
double ExtractExactWeightedHpwl(
    const CpSolverResponse& response,
    const std::vector<ExactGriddedNetVariables>& net_variables,
    const ExactGriddedLegalizationModel& model, int64_t pin_scale) {
  double weighted_hpwl = 0.0;
  for (const ExactGriddedNetVariables& net : net_variables) {
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
  std::vector<std::vector<ExactGriddedRowVariables>> row_variables;
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
      if (!stripe.initial_rows.empty()) {
        const ExactGriddedRowHint& hint = stripe.initial_rows[row_index];
        row.preserve_initial_well_heights = cp_model.NewBoolVar().WithName(
            "row_preserve_initial_heights_" + suffix);
        cp_model.AddImplication(row.preserve_initial_well_heights, row.active);
        row.p_well_height_candidates.push_back(LinearExpr::Term(
            row.preserve_initial_well_heights, hint.p_well_height));
        row.n_well_height_candidates.push_back(LinearExpr::Term(
            row.preserve_initial_well_heights, hint.n_well_height));
        cp_model.AddHint(row.active, hint.active);
        cp_model.AddHint(row.preserve_initial_well_heights, hint.active);
        cp_model.AddHint(row.y, hint.y);
        cp_model.AddHint(row.p_well_height, hint.p_well_height);
        cp_model.AddHint(row.n_well_height, hint.n_well_height);
      }
      stripe_rows.push_back(std::move(row));
    }
    row_variables.push_back(std::move(stripe_rows));
  }

  std::vector<ExactGriddedCellVariables> cell_variables;
  std::vector<ExactGriddedCandidateVariables> candidate_variables;
  std::vector<int> stripe_phase_hints(model.stripes.size(), -1);
  std::vector<int> hinted_cell_x_locations;
  std::vector<int> hinted_cell_y_locations;
  cell_variables.reserve(model.cells.size());
  hinted_cell_x_locations.reserve(model.cells.size());
  hinted_cell_y_locations.reserve(model.cells.size());

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
    int hinted_x = std::clamp(cell.initial_x, minimum_x, maximum_x);
    int hinted_y = std::clamp(cell.initial_y, minimum_y, maximum_y);
    cp_model.AddHint(variables.x, hinted_x);
    cp_model.AddHint(variables.y, hinted_y);
    hinted_cell_x_locations.push_back(hinted_x);
    hinted_cell_y_locations.push_back(hinted_y);
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
      cp_model.AddHint(x_displacement, std::abs(hinted_cell_x_locations[index] -
                                                cell.initial_x));
      cp_model.AddHint(y_displacement, std::abs(hinted_cell_y_locations[index] -
                                                cell.initial_y));
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
    std::vector<int64_t> hinted_pin_x_locations;
    std::vector<int64_t> hinted_pin_y_locations;
    bool has_complete_net_hint = true;
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
        hinted_pin_x_locations.push_back(fixed_x);
        hinted_pin_y_locations.push_back(fixed_y);
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
      const ExactGriddedCell& cell = model.cells[cell_index];
      if (cell.initial_stripe_id >= 0 && cell.initial_start_row >= 0) {
        int64_t hinted_offset_x = static_cast<int64_t>(std::llround(
            (cell.initial_is_flipped ? pin.offset_x_fs : pin.offset_x_n) *
            pin_scale));
        int64_t hinted_offset_y = static_cast<int64_t>(std::llround(
            (cell.initial_is_flipped ? pin.offset_y_fs : pin.offset_y_n) *
            pin_scale));
        int64_t hinted_pin_x =
            static_cast<int64_t>(hinted_cell_x_locations[cell_index]) *
                pin_scale +
            hinted_offset_x;
        int64_t hinted_pin_y =
            static_cast<int64_t>(hinted_cell_y_locations[cell_index]) *
                pin_scale +
            hinted_offset_y;
        cp_model.AddHint(pin_x, hinted_pin_x);
        cp_model.AddHint(pin_y, hinted_pin_y);
        hinted_pin_x_locations.push_back(hinted_pin_x);
        hinted_pin_y_locations.push_back(hinted_pin_y);
      } else {
        has_complete_net_hint = false;
      }
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
    if (has_complete_net_hint) {
      cp_model.AddHint(variables.minimum_x,
                       *std::min_element(hinted_pin_x_locations.begin(),
                                         hinted_pin_x_locations.end()));
      cp_model.AddHint(variables.maximum_x,
                       *std::max_element(hinted_pin_x_locations.begin(),
                                         hinted_pin_x_locations.end()));
      cp_model.AddHint(variables.minimum_y,
                       *std::min_element(hinted_pin_y_locations.begin(),
                                         hinted_pin_y_locations.end()));
      cp_model.AddHint(variables.maximum_y,
                       *std::max_element(hinted_pin_y_locations.begin(),
                                         hinted_pin_y_locations.end()));
    }
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
  const auto& hinted_model_proto = cp_model.Build();
  result.model_variable_count = hinted_model_proto.variables_size();
  result.model_constraint_count = hinted_model_proto.constraints_size();
  if (config.validate_solution_hint) {
    operations_research::sat::SatParameters hint_parameters = parameters;
    hint_parameters.set_fix_variables_to_their_hinted_value(true);
    CpSolverResponse hint_response =
        operations_research::sat::SolveWithParameters(hinted_model_proto,
                                                      hint_parameters);
    result.hint_validation_wall_time_seconds = hint_response.wall_time();
    switch (hint_response.status()) {
      case operations_research::sat::CpSolverStatus::OPTIMAL:
        result.hint_validation_status =
            ExactGriddedLegalizationStatus::kOptimal;
        break;
      case operations_research::sat::CpSolverStatus::FEASIBLE:
        result.hint_validation_status =
            ExactGriddedLegalizationStatus::kFeasible;
        break;
      case operations_research::sat::CpSolverStatus::INFEASIBLE:
        result.hint_validation_status =
            ExactGriddedLegalizationStatus::kInfeasible;
        break;
      case operations_research::sat::CpSolverStatus::MODEL_INVALID:
        result.hint_validation_status =
            ExactGriddedLegalizationStatus::kInvalidModel;
        break;
      case operations_research::sat::CpSolverStatus::UNKNOWN:
      default:
        result.hint_validation_status =
            ExactGriddedLegalizationStatus::kUnknown;
        break;
    }
    if (result.hint_validation_status ==
            ExactGriddedLegalizationStatus::kFeasible ||
        result.hint_validation_status ==
            ExactGriddedLegalizationStatus::kOptimal) {
      result.hinted_weighted_hpwl = ExtractExactWeightedHpwl(
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
  CpSolverResponse response = operations_research::sat::SolveWithParameters(
      solve_model_proto, parameters);

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
  result.weighted_hpwl =
      ExtractExactWeightedHpwl(response, net_variables, model, pin_scale);

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
