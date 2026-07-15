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
#ifndef DALI_PLACER_WELL_LEGALIZER_ORTOOLS_FIXED_ROW_DISPLACEMENT_OPTIMIZER_H_
#define DALI_PLACER_WELL_LEGALIZER_ORTOOLS_FIXED_ROW_DISPLACEMENT_OPTIMIZER_H_

#include <cstdint>
#include <string>
#include <vector>

namespace dali {

/** One component variable in a fixed-row displacement model. */
struct FixedRowComponentVariable {
  int component_id = -1;
  int width = 0;
  int initial_x = 0;
  int minimum_x = 0;
  int maximum_x = 0;
};

/**
 * Left-to-right component order imposed by one physical row segment.
 *
 * A multi-height component may occur in several row sequences. All occurrences
 * refer to the same component variable and therefore share one X coordinate.
 */
struct FixedRowComponentSequence {
  std::vector<int> component_ids;
  int minimum_spacing = 0;
};

/** Complete fixed-row model passed to the optional OR-Tools backend. */
struct FixedRowDisplacementModel {
  std::vector<FixedRowComponentVariable> components;
  std::vector<FixedRowComponentSequence> rows;
};

/** Runtime limits for one CP-SAT solve. */
struct FixedRowDisplacementSolverConfig {
  double maximum_time_seconds = 10.0;
  int number_of_workers = 1;
};

/** Solver termination state independent of the OR-Tools API. */
enum class FixedRowDisplacementStatus {
  kUnavailable,
  kInvalidModel,
  kUnknown,
  kInfeasible,
  kFeasible,
  kOptimal,
};

/** One solved lower-left X coordinate in Dali grid units. */
struct FixedRowComponentLocation {
  int component_id = -1;
  int x = 0;
};

/** Solution and diagnostics returned by the fixed-row optimizer. */
struct FixedRowDisplacementResult {
  FixedRowDisplacementStatus status = FixedRowDisplacementStatus::kUnavailable;
  std::vector<FixedRowComponentLocation> locations;
  std::string message;
  int64_t total_displacement = 0;
  double best_objective_bound = 0.0;
  double relative_gap = 0.0;
  int64_t conflict_count = 0;
  int64_t branch_count = 0;
  double wall_time_seconds = 0.0;

  /** Return true when the result contains a feasible placement. */
  bool HasSolution() const;
};

/**
 * Optimize fixed-row component X coordinates with OR-Tools CP-SAT.
 *
 * The class exposes no OR-Tools types, keeping the optional dependency behind
 * one implementation boundary. When Dali is built without a compatible
 * OR-Tools installation, Solve() returns kUnavailable.
 */
class OrToolsFixedRowDisplacementOptimizer {
 public:
  /** Return true when Dali was built with the supported OR-Tools backend. */
  static bool IsAvailable();

  /**
   * Minimize total absolute X displacement while preserving every row order.
   *
   * Component bounds are bounds on the lower-left X coordinate. Locations and
   * widths are integer Dali grid units.
   */
  FixedRowDisplacementResult Solve(
      const FixedRowDisplacementModel& model,
      const FixedRowDisplacementSolverConfig& config = {}) const;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_ORTOOLS_FIXED_ROW_DISPLACEMENT_OPTIMIZER_H_
