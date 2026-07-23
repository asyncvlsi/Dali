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
 * Chooses stripe boundaries by packing columns at a uniform width.
 *
 * The simple planner: it divides the region evenly, subject to the maximum row
 * width the technology allows. See the adaptive planner for the alternative
 * that varies boundaries with local demand.
 */
#include "dali/placer/well_legalizer/packed_stripe_boundary_planner.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "dali/common/helper.h"

namespace dali {

PackedStripeBoundaryPlanner::PackedStripeBoundaryPlanner(
    AdaptiveStripeBoundaryConfig config)
    : config_(config) {
  DaliExpects(config_.region_right > config_.region_left,
              "Packed stripe region must have positive width");
  DaliExpects(config_.column_count > 0,
              "Packed stripe column count must be positive");
  DaliExpects(config_.minimum_column_pitch > 0,
              "Packed stripe minimum pitch must be positive");
  DaliExpects(config_.maximum_column_pitch == 0 ||
                  config_.maximum_column_pitch >= config_.minimum_column_pitch,
              "Packed stripe maximum pitch is smaller than its minimum");
  DaliExpects(config_.boundary_step > 0,
              "Packed stripe boundary step must be positive");
  DaliExpects(config_.spacing_per_column >= 0,
              "Packed stripe spacing cannot be negative");
}

/**
 * Choose stripe boundaries by packing columns at a uniform width.
 *
 * The simple counterpart to the adaptive planner; divides the region evenly
 * within the maximum row width.
 * @param samples per-position demand, used only to size the region.
 * @return the boundary positions.
 */
AdaptiveStripeBoundaryResult PackedStripeBoundaryPlanner::Plan(
    const std::vector<StripePackingSample>& samples) const {
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

  int signature_count = 0;
  for (const StripePackingSample& sample : samples) {
    DaliExpects(sample.width > 0,
                "Packed stripe sample width must be positive");
    DaliExpects(sample.signature_height > 0,
                "Packed stripe signature height must be positive");
    DaliExpects(sample.signature_id >= 0,
                "Packed stripe signature id cannot be negative");
    signature_count = std::max(signature_count, sample.signature_id + 1);
  }

  std::vector<int> signature_heights(signature_count, 0);
  std::vector<std::vector<unsigned long long>> prefix_widths(
      signature_count, std::vector<unsigned long long>(candidates.size(), 0));
  std::vector<StripePackingSample> sorted_samples = samples;
  std::sort(sorted_samples.begin(), sorted_samples.end(),
            [](const StripePackingSample& lhs, const StripePackingSample& rhs) {
              return lhs.x < rhs.x;
            });
  std::vector<unsigned long long> accumulated_widths(signature_count, 0);
  size_t sample_index = 0;
  for (size_t boundary_index = 0; boundary_index < candidates.size();
       ++boundary_index) {
    while (sample_index < sorted_samples.size() &&
           sorted_samples[sample_index].x < candidates[boundary_index]) {
      const StripePackingSample& sample = sorted_samples[sample_index];
      int& height = signature_heights[sample.signature_id];
      DaliExpects(height == 0 || height == sample.signature_height,
                  "Components in one packing signature have different heights");
      height = sample.signature_height;
      accumulated_widths[sample.signature_id] += sample.width;
      ++sample_index;
    }
    for (int signature = 0; signature < signature_count; ++signature) {
      prefix_widths[signature][boundary_index] = accumulated_widths[signature];
    }
  }
  while (sample_index < sorted_samples.size()) {
    const StripePackingSample& sample = sorted_samples[sample_index];
    int& height = signature_heights[sample.signature_id];
    DaliExpects(height == 0 || height == sample.signature_height,
                "Components in one packing signature have different heights");
    height = sample.signature_height;
    accumulated_widths[sample.signature_id] += sample.width;
    ++sample_index;
  }
  for (int signature = 0; signature < signature_count; ++signature) {
    prefix_widths[signature].back() = accumulated_widths[signature];
  }

  const int average_usable_width =
      region_width / config_.column_count - config_.spacing_per_column;
  if (average_usable_width <= 0) return result;
  double total_reference_height = 0.0;
  for (int signature = 0; signature < signature_count; ++signature) {
    unsigned long long width = accumulated_widths[signature];
    unsigned long long shelf_count =
        (width + average_usable_width - 1) / average_usable_width;
    total_reference_height += shelf_count * signature_heights[signature];
  }
  const double target_height =
      std::max(1.0, total_reference_height / config_.column_count);
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

        double required_height = 0.0;
        for (int signature = 0; signature < signature_count; ++signature) {
          unsigned long long width = prefix_widths[signature][right_index] -
                                     prefix_widths[signature][left_index];
          unsigned long long shelf_count =
              (width + usable_width - 1) / usable_width;
          required_height += shelf_count * signature_heights[signature];
        }
        const double normalized_height_error =
            (required_height - target_height) / target_height;
        const double normalized_pitch_error =
            (pitch - average_pitch) / average_pitch;
        const double candidate_cost =
            cost[column - 1][left_index] +
            normalized_height_error * normalized_height_error +
            1e-9 * normalized_pitch_error * normalized_pitch_error;
        if (candidate_cost < cost[column][right_index]) {
          cost[column][right_index] = candidate_cost;
          predecessor[column][right_index] = static_cast<int>(left_index);
        }
      }
    }
  }

  int candidate_index = static_cast<int>(candidates.size()) - 1;
  if (!std::isfinite(cost[config_.column_count][candidate_index])) {
    return result;
  }
  result.feasible = true;
  result.objective = cost[config_.column_count][candidate_index];
  result.boundaries.resize(config_.column_count + 1);
  for (int column = config_.column_count; column >= 0; --column) {
    result.boundaries[column] = candidates[candidate_index];
    if (column > 0) {
      candidate_index = predecessor[column][candidate_index];
      DaliExpects(candidate_index >= 0,
                  "Packed stripe boundary traceback is incomplete");
    }
  }
  return result;
}

}  // namespace dali
