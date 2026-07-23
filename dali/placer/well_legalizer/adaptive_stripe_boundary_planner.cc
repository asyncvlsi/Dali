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

/**
 * @file
 * Chooses stripe boundaries from local component demand instead of a uniform
 * width.
 *
 * Widening columns where the design is sparse and narrowing them where it is
 * dense balances occupancy better than even division, at the cost of columns
 * that no longer share a width. Selected by `-enable_adaptive_stripe_boundaries`.
 */
#include "dali/placer/well_legalizer/adaptive_stripe_boundary_planner.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "dali/common/helper.h"

namespace dali {

AdaptiveStripeBoundaryPlanner::AdaptiveStripeBoundaryPlanner(
    AdaptiveStripeBoundaryConfig config)
    : config_(config) {
  DaliExpects(config_.region_right > config_.region_left,
              "Adaptive stripe region must have positive width");
  DaliExpects(config_.column_count > 0,
              "Adaptive stripe column count must be positive");
  DaliExpects(config_.minimum_column_pitch > 0,
              "Adaptive stripe minimum pitch must be positive");
  DaliExpects(config_.maximum_column_pitch == 0 ||
                  config_.maximum_column_pitch >= config_.minimum_column_pitch,
              "Adaptive stripe maximum pitch is smaller than its minimum");
  DaliExpects(config_.boundary_step > 0,
              "Adaptive stripe boundary step must be positive");
  DaliExpects(config_.spacing_per_column >= 0,
              "Adaptive stripe spacing cannot be negative");
}

AdaptiveStripeBoundaryResult AdaptiveStripeBoundaryPlanner::Plan(
    const std::vector<StripeDemandSample>& samples) const {
  AdaptiveStripeBoundaryResult result;
  const int region_width = config_.region_right - config_.region_left;
  const int maximum_pitch = config_.maximum_column_pitch > 0
                                ? config_.maximum_column_pitch
                                : region_width;
  if (region_width < config_.column_count * config_.minimum_column_pitch ||
      region_width > config_.column_count * maximum_pitch) {
    return result;
  }

  std::vector<int> candidates;
  candidates.reserve(region_width / config_.boundary_step +
                     config_.column_count + 2);
  candidates.push_back(config_.region_left);
  for (int boundary = config_.region_left + config_.boundary_step;
       boundary < config_.region_right; boundary += config_.boundary_step) {
    candidates.push_back(boundary);
  }
  // Uniform cutlines preserve a feasible reference even when they do not land
  // on the sampling step.
  for (int column = 1; column < config_.column_count; ++column) {
    candidates.push_back(config_.region_left +
                         static_cast<int>(std::llround(
                             region_width * column /
                             static_cast<double>(config_.column_count))));
  }
  candidates.push_back(config_.region_right);
  std::sort(candidates.begin(), candidates.end());
  candidates.erase(std::unique(candidates.begin(), candidates.end()),
                   candidates.end());

  // prefix_demand[j] contains samples strictly left of candidates[j]. A
  // component exactly on a cutline therefore belongs to the right interval.
  std::vector<StripeDemandSample> sorted_samples = samples;
  std::sort(sorted_samples.begin(), sorted_samples.end(),
            [](const StripeDemandSample& lhs, const StripeDemandSample& rhs) {
              return lhs.x < rhs.x;
            });
  std::vector<double> prefix_demand(candidates.size(), 0.0);
  size_t sample_index = 0;
  double accumulated_demand = 0.0;
  for (size_t boundary_index = 0; boundary_index < candidates.size();
       ++boundary_index) {
    while (sample_index < sorted_samples.size() &&
           sorted_samples[sample_index].x < candidates[boundary_index]) {
      if (sorted_samples[sample_index].demand > 0.0) {
        accumulated_demand += sorted_samples[sample_index].demand;
      }
      ++sample_index;
    }
    prefix_demand[boundary_index] = accumulated_demand;
  }
  // Include samples on or beyond the right boundary in the final interval.
  while (sample_index < sorted_samples.size()) {
    if (sorted_samples[sample_index].demand > 0.0) {
      accumulated_demand += sorted_samples[sample_index].demand;
    }
    ++sample_index;
  }
  prefix_demand.back() = accumulated_demand;

  const int total_usable_width =
      region_width - config_.column_count * config_.spacing_per_column;
  if (total_usable_width <= 0) return result;
  const double demand_per_usable_width =
      accumulated_demand / total_usable_width;
  const double normalization =
      std::max(1.0, accumulated_demand / config_.column_count);
  const double average_pitch =
      region_width / static_cast<double>(config_.column_count);

  const double infinity = std::numeric_limits<double>::infinity();
  std::vector<std::vector<double>> cost(
      config_.column_count + 1,
      std::vector<double>(candidates.size(), infinity));
  std::vector<std::vector<int>> predecessor(
      config_.column_count + 1, std::vector<int>(candidates.size(), -1));
  cost[0][0] = 0.0;

  for (int column = 1; column <= config_.column_count; ++column) {
    for (size_t right_index = 1; right_index < candidates.size();
         ++right_index) {
      for (size_t left_index = 0; left_index < right_index; ++left_index) {
        if (!std::isfinite(cost[column - 1][left_index])) continue;
        const int pitch = candidates[right_index] - candidates[left_index];
        if (pitch < config_.minimum_column_pitch || pitch > maximum_pitch) {
          continue;
        }
        const int remaining_width =
            config_.region_right - candidates[right_index];
        const int remaining_columns = config_.column_count - column;
        if (remaining_width <
                remaining_columns * config_.minimum_column_pitch ||
            remaining_width > remaining_columns * maximum_pitch) {
          continue;
        }

        const int usable_width = pitch - config_.spacing_per_column;
        if (usable_width <= 0) continue;
        const double interval_demand =
            prefix_demand[right_index] - prefix_demand[left_index];
        const double target_demand = demand_per_usable_width * usable_width;
        const double normalized_error =
            (interval_demand - target_demand) / normalization;
        const double normalized_pitch_error =
            (pitch - average_pitch) / average_pitch;
        const double candidate_cost =
            cost[column - 1][left_index] + normalized_error * normalized_error +
            1e-9 * normalized_pitch_error * normalized_pitch_error;
        if (candidate_cost < cost[column][right_index]) {
          cost[column][right_index] = candidate_cost;
          predecessor[column][right_index] = static_cast<int>(left_index);
        }
      }
    }
  }

  const int final_index = static_cast<int>(candidates.size()) - 1;
  if (!std::isfinite(cost[config_.column_count][final_index])) return result;

  result.feasible = true;
  result.objective = cost[config_.column_count][final_index];
  result.boundaries.resize(config_.column_count + 1);
  int candidate_index = final_index;
  for (int column = config_.column_count; column >= 0; --column) {
    result.boundaries[column] = candidates[candidate_index];
    if (column > 0) {
      candidate_index = predecessor[column][candidate_index];
      DaliExpects(candidate_index >= 0,
                  "Adaptive stripe boundary traceback is incomplete");
    }
  }
  return result;
}

}  // namespace dali
