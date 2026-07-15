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
#ifndef DALI_PLACER_WELL_LEGALIZER_ORTOOLS_EXACT_GRIDDED_LEGALIZER_H_
#define DALI_PLACER_WELL_LEGALIZER_ORTOOLS_EXACT_GRIDDED_LEGALIZER_H_

#include <cstdint>
#include <string>
#include <vector>

#include "dali/placer/well_legalizer/exact_gridded_legalization_model.h"

namespace dali {

/** Solver termination state independent of the OR-Tools API. */
enum class ExactGriddedLegalizationStatus {
  kUnavailable,
  kInvalidModel,
  kUnknown,
  kInfeasible,
  kFeasible,
  kOptimal,
};

/** Runtime and objective controls for one exact legalization solve. */
struct ExactGriddedLegalizationConfig {
  double maximum_time_seconds = 300.0;
  int number_of_workers = 1;
  int pin_coordinate_scale = 1000;
  double weighted_hpwl_weight = 1.0;
  double displacement_weight = 0.0;
  bool log_search_progress = false;
};

/** One solved component location and discrete legalization assignment. */
struct ExactGriddedCellPlacement {
  int component_id = -1;
  int stripe_id = -1;
  int row_index = -1;
  int x = 0;
  int y = 0;
  bool is_flipped = false;
};

/** One active solved row in a stripe. */
struct ExactGriddedRowPlacement {
  int stripe_id = -1;
  int row_index = -1;
  int y = 0;
  int p_well_height = 0;
  int n_well_height = 0;
  bool is_orient_n = true;
};

/** Exact legalization solution and CP-SAT optimality diagnostics. */
struct ExactGriddedLegalizationResult {
  ExactGriddedLegalizationStatus status =
      ExactGriddedLegalizationStatus::kUnavailable;
  std::vector<ExactGriddedCellPlacement> cells;
  std::vector<ExactGriddedRowPlacement> rows;
  std::string message;
  double weighted_hpwl = 0.0;
  int64_t total_displacement = 0;
  double objective_value = 0.0;
  double best_objective_bound = 0.0;
  double relative_gap = 0.0;
  int64_t conflict_count = 0;
  int64_t branch_count = 0;
  double wall_time_seconds = 0.0;

  /** Return true when the result contains a complete legal placement. */
  bool HasSolution() const;
};

/**
 * Solve complete gridded legalization within fixed stripe rectangles.
 *
 * Unlike the fixed-row refiner, this backend chooses row formation,
 * component-to-row assignment, orientation, X ordering, and row Y locations.
 * OR-Tools types remain private to the implementation so solver support stays
 * optional for downstream Dali users.
 */
class OrToolsExactGriddedLegalizer {
 public:
  /** Return true when Dali was built with the supported OR-Tools backend. */
  static bool IsAvailable();

  /** Solve the supplied model and return the incumbent plus optimality bound.
   */
  ExactGriddedLegalizationResult Solve(
      const ExactGriddedLegalizationModel& model,
      const ExactGriddedLegalizationConfig& config = {}) const;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_ORTOOLS_EXACT_GRIDDED_LEGALIZER_H_
