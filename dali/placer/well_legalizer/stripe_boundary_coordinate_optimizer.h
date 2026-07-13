/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *******************************************************************************/
#ifndef DALI_PLACER_WELL_LEGALIZER_STRIPE_BOUNDARY_COORDINATE_OPTIMIZER_H_
#define DALI_PLACER_WELL_LEGALIZER_STRIPE_BOUNDARY_COORDINATE_OPTIMIZER_H_

#include <functional>
#include <vector>

namespace dali {

/** Feasibility and exact cost returned for one stripe-boundary candidate. */
struct StripeBoundaryEvaluation {
  bool feasible = false;
  double cost = 0.0;
};

/** Search limits for one local stripe-boundary sweep. */
struct StripeBoundaryCoordinateConfig {
  int step = 1;
  int minimum_pitch = 1;
  int maximum_pitch = 0;
  double minimum_improvement = 0.0;
};

/** Summary of an exact local stripe-boundary search. */
struct StripeBoundaryCoordinateResult {
  bool feasible = false;
  double initial_cost = 0.0;
  double final_cost = 0.0;
  int evaluated_candidates = 0;
  int accepted_moves = 0;
  std::vector<int> boundaries;
};

/**
 * Runs one deterministic coordinate-descent sweep over internal cutlines.
 *
 * For each cutline, the optimizer trials one step left and right while
 * preserving adjacent pitch constraints. The supplied evaluator owns all
 * domain behavior, including legalization, feasibility, and exact cost. A
 * move is accepted immediately only when it improves the current legal cost,
 * so later cutlines see earlier accepted geometry.
 */
class StripeBoundaryCoordinateOptimizer {
 public:
  using Evaluator = std::function<StripeBoundaryEvaluation(
      const std::vector<int>& boundaries)>;

  explicit StripeBoundaryCoordinateOptimizer(
      StripeBoundaryCoordinateConfig config);

  /** Run one left-to-right sweep from the supplied legal boundary vector. */
  StripeBoundaryCoordinateResult Optimize(
      const std::vector<int>& initial_boundaries,
      const Evaluator& evaluator) const;

 private:
  bool PitchesAreLegal(const std::vector<int>& boundaries,
                       int boundary_index) const;

  StripeBoundaryCoordinateConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_STRIPE_BOUNDARY_COORDINATE_OPTIMIZER_H_
