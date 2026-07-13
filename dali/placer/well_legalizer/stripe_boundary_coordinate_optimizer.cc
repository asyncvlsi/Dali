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
#include "dali/placer/well_legalizer/stripe_boundary_coordinate_optimizer.h"

#include <algorithm>

#include "dali/common/helper.h"

namespace dali {

StripeBoundaryCoordinateOptimizer::StripeBoundaryCoordinateOptimizer(
    StripeBoundaryCoordinateConfig config)
    : config_(config) {
  DaliExpects(config_.step > 0, "Stripe boundary step must be positive");
  DaliExpects(config_.minimum_pitch > 0,
              "Stripe boundary minimum pitch must be positive");
  DaliExpects(config_.maximum_pitch == 0 ||
                  config_.maximum_pitch >= config_.minimum_pitch,
              "Stripe boundary maximum pitch is smaller than its minimum");
  DaliExpects(config_.minimum_improvement >= 0.0,
              "Stripe boundary minimum improvement cannot be negative");
}

StripeBoundaryCoordinateResult StripeBoundaryCoordinateOptimizer::Optimize(
    const std::vector<int>& initial_boundaries,
    const Evaluator& evaluator) const {
  DaliExpects(initial_boundaries.size() >= 2,
              "Stripe boundary optimization requires at least one column");
  DaliExpects(static_cast<bool>(evaluator),
              "Stripe boundary evaluator cannot be empty");
  DaliExpects(std::is_sorted(initial_boundaries.begin(),
                             initial_boundaries.end()),
              "Stripe boundaries must be sorted");

  StripeBoundaryCoordinateResult result;
  result.boundaries = initial_boundaries;
  StripeBoundaryEvaluation current = evaluator(result.boundaries);
  ++result.evaluated_candidates;
  if (!current.feasible) return result;

  result.feasible = true;
  result.initial_cost = current.cost;
  result.final_cost = current.cost;
  for (size_t boundary = 1; boundary + 1 < result.boundaries.size();
       ++boundary) {
    std::vector<int> best_boundaries = result.boundaries;
    double best_cost = result.final_cost;
    for (int direction : {-1, 1}) {
      std::vector<int> candidate = result.boundaries;
      candidate[boundary] += direction * config_.step;
      if (!PitchesAreLegal(candidate, static_cast<int>(boundary))) continue;

      StripeBoundaryEvaluation evaluation = evaluator(candidate);
      ++result.evaluated_candidates;
      if (evaluation.feasible &&
          evaluation.cost + config_.minimum_improvement < best_cost) {
        best_cost = evaluation.cost;
        best_boundaries = std::move(candidate);
      }
    }
    if (best_boundaries != result.boundaries) {
      result.boundaries = std::move(best_boundaries);
      result.final_cost = best_cost;
      ++result.accepted_moves;
    }
  }
  return result;
}

bool StripeBoundaryCoordinateOptimizer::PitchesAreLegal(
    const std::vector<int>& boundaries, int boundary_index) const {
  int left_pitch =
      boundaries[boundary_index] - boundaries[boundary_index - 1];
  int right_pitch =
      boundaries[boundary_index + 1] - boundaries[boundary_index];
  if (left_pitch < config_.minimum_pitch ||
      right_pitch < config_.minimum_pitch) {
    return false;
  }
  if (config_.maximum_pitch > 0 &&
      (left_pitch > config_.maximum_pitch ||
       right_pitch > config_.maximum_pitch)) {
    return false;
  }
  return true;
}

}  // namespace dali
