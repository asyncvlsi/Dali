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
#ifndef DALI_PLACER_WELL_LEGALIZER_ORTOOLS_VERTICAL_HPWL_ROW_ASSIGNMENT_H_
#define DALI_PLACER_WELL_LEGALIZER_ORTOOLS_VERTICAL_HPWL_ROW_ASSIGNMENT_H_

#include <cstdint>
#include <string>
#include <vector>

namespace dali {

/** Available width for one candidate gridded row. */
struct VerticalHpwlRowCapacity {
  int row_id = -1;
  int capacity = 0;
};

/** One legal row choice and the component's lower-left Y on that row. */
struct VerticalHpwlRowCandidate {
  int row_id = -1;
  double component_lly = 0.0;
};

/** Component whose row membership is selected by the solver. */
struct VerticalHpwlRowComponent {
  int component_id = -1;
  int width = 0;
  int initial_row_id = -1;
  std::vector<VerticalHpwlRowCandidate> candidates;
};

/**
 * One net pin in the vertical-HPWL model.
 *
 * A negative component id denotes a fixed pin at fixed_y. For a movable pin,
 * candidate_offsets_y follows the corresponding component's candidate order.
 */
struct VerticalHpwlRowPin {
  int component_id = -1;
  double fixed_y = 0.0;
  std::vector<double> candidate_offsets_y;
};

/** Weighted net represented only in the vertical dimension. */
struct VerticalHpwlRowNet {
  std::vector<VerticalHpwlRowPin> pins;
  double weight = 1.0;
};

/** Complete capacitated row-membership problem. */
struct VerticalHpwlRowAssignmentModel {
  std::vector<VerticalHpwlRowCapacity> rows;
  std::vector<VerticalHpwlRowComponent> components;
  std::vector<VerticalHpwlRowNet> nets;
};

/** Runtime and numeric controls for one CP-SAT solve. */
struct VerticalHpwlRowAssignmentConfig {
  double maximum_time_seconds = 0.05;
  int number_of_workers = 1;
  int coordinate_scale = 1000;
};

/** Solver termination state independent of the optional OR-Tools API. */
enum class VerticalHpwlRowAssignmentStatus {
  kUnavailable,
  kInvalidModel,
  kUnknown,
  kInfeasible,
  kFeasible,
  kOptimal,
};

/** Selected row for one component. */
struct VerticalHpwlRowLocation {
  int component_id = -1;
  int row_id = -1;
};

/** Solution and diagnostics from one row-assignment model. */
struct VerticalHpwlRowAssignmentResult {
  VerticalHpwlRowAssignmentStatus status =
      VerticalHpwlRowAssignmentStatus::kUnavailable;
  std::vector<VerticalHpwlRowLocation> assignments;
  std::string message;
  double objective_value = 0.0;
  double best_objective_bound = 0.0;
  double wall_time_seconds = 0.0;

  /** Return true when assignments contain a feasible incumbent. */
  bool HasSolution() const;
};

/** Optional OR-Tools backend for capacitated vertical-HPWL row assignment. */
class OrToolsVerticalHpwlRowAssignment {
 public:
  /** Return true when Dali was built with a compatible OR-Tools package. */
  static bool IsAvailable();

  /** Solve one independent row-membership model. */
  VerticalHpwlRowAssignmentResult Solve(
      const VerticalHpwlRowAssignmentModel& model,
      const VerticalHpwlRowAssignmentConfig& config) const;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_ORTOOLS_VERTICAL_HPWL_ROW_ASSIGNMENT_H_
