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
#ifndef DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_LEGALIZATION_WINDOW_ANALYZER_H_
#define DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_LEGALIZATION_WINDOW_ANALYZER_H_

#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/ortools_exact_gridded_legalizer.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Runtime and model-size limits for bounded exact legalization analysis. */
struct ExactGriddedWindowAnalyzerConfig {
  int target_components_per_window = 48;
  int maximum_components_per_window = 96;
  int maximum_windows = 24;
  int net_ignore_threshold = 100;
  int minimum_p_well_height = 0;
  int minimum_n_well_height = 0;
  double maximum_time_seconds_per_window = 0.25;
  int number_of_workers = 1;
};

/** Exact-solver diagnostics for one closed, contiguous stripe-row window. */
struct ExactGriddedWindowResult {
  int column_index = -1;
  int stripe_index = -1;
  int first_row_index = -1;
  int last_row_index = -1;
  int component_count = 0;
  int net_count = 0;
  double current_weighted_hpwl = 0.0;
  double solved_weighted_hpwl = 0.0;
  double best_objective_bound = 0.0;
  double relative_gap = 0.0;
  double wall_time_seconds = 0.0;
  ExactGriddedLegalizationStatus status =
      ExactGriddedLegalizationStatus::kUnavailable;
};

/** Aggregate diagnostics from independently solved legalization windows. */
struct ExactGriddedWindowAnalysis {
  bool available = false;
  int candidate_windows = 0;
  int oversized_windows = 0;
  int attempted_windows = 0;
  int solved_windows = 0;
  int optimal_windows = 0;
  int positive_bound_windows = 0;
  double solved_current_hpwl_sum = 0.0;
  double solved_incumbent_hpwl_sum = 0.0;
  double bounded_current_hpwl_sum = 0.0;
  double positive_lower_bound_sum = 0.0;
  double solver_wall_time_seconds = 0.0;
  std::vector<ExactGriddedWindowResult> windows;
};

/**
 * Measure local legal-placement headroom with exact CP-SAT subproblems.
 *
 * Every window contains complete multi-region components; no component crosses
 * a window boundary. Windows are solved independently and never modify the
 * circuit. Aggregate HPWL sums are diagnostic rather than globally additive,
 * because a net crossing two windows can appear in both local objectives.
 */
class ExactGriddedLegalizationWindowAnalyzer {
 public:
  ExactGriddedLegalizationWindowAnalyzer(
      Circuit* circuit, const ExactGriddedWindowAnalyzerConfig& config = {});

  /** Analyze the highest-HPWL bounded windows in the supplied legal rows. */
  ExactGriddedWindowAnalysis Analyze(std::vector<StripeColumn>* columns) const;

 private:
  struct WindowCandidate {
    int column_index = -1;
    int stripe_index = -1;
    int first_row_index = -1;
    int last_row_index = -1;
    int lx = 0;
    int ly = 0;
    int ux = 0;
    int uy = 0;
    int left_boundary_margin = 0;
    int right_boundary_margin = 0;
    double current_weighted_hpwl = 0.0;
    std::vector<Component*> components;
  };

  /** Partition one stripe into closed windows with no split component. */
  std::vector<WindowCandidate> BuildStripeWindows(Stripe* stripe,
                                                  int column_index,
                                                  int stripe_index,
                                                  int* oversized_windows) const;

  /** Return current HPWL for the production nets represented by a window. */
  double CurrentWindowHpwl(const std::vector<Component*>& components) const;

  Circuit* circuit_ = nullptr;
  ExactGriddedWindowAnalyzerConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_LEGALIZATION_WINDOW_ANALYZER_H_
