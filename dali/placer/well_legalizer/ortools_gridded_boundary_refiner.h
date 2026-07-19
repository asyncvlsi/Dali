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
#ifndef DALI_PLACER_WELL_LEGALIZER_ORTOOLS_GRIDDED_BOUNDARY_REFINER_H_
#define DALI_PLACER_WELL_LEGALIZER_ORTOOLS_GRIDDED_BOUNDARY_REFINER_H_

#include <utility>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/ortools_gridded_boundary_optimizer.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Runtime and window controls for design-level cross-stripe refinement. */
struct OrToolsGriddedBoundaryRefinerConfig {
  double maximum_time_seconds_per_model = 0.1;
  double maximum_total_time_seconds = 120.0;
  double minimum_relative_improvement = 1e-5;
  int maximum_sweeps = 1;
  int maximum_components_per_model = 64;
  int maximum_assignment_changes = 4;
  int number_of_workers = 1;
  int net_ignore_threshold = 100;
  int minimum_p_well_height = 0;
  int minimum_n_well_height = 0;
  bool run_local_detailed_closure = false;
  bool use_solution_hint = true;
};

/** Location and result of one attempted boundary window. */
struct OrToolsGriddedBoundaryWindowResult {
  int sweep = -1;
  int first_column = -1;
  int second_column = -1;
  int first_stripe = -1;
  int second_stripe = -1;
  int first_row = -1;
  int first_last_row = -1;
  int second_row = -1;
  int second_last_row = -1;
  OrToolsGriddedBoundaryOptimizerResult optimization;
};

/** Aggregate diagnostics from sequential cross-stripe sweeps. */
struct OrToolsGriddedBoundaryRefinerResult {
  bool available = false;
  bool time_budget_exhausted = false;
  int candidate_windows = 0;
  int oversized_windows = 0;
  int attempted_models = 0;
  int solved_models = 0;
  int accepted_models = 0;
  int accepted_cross_stripe_models = 0;
  int accepted_cross_stripe_components = 0;
  int completed_sweeps = 0;
  double hpwl_before = 0.0;
  double hpwl_after = 0.0;
  double local_hpwl_improvement = 0.0;
  double cross_stripe_hpwl_improvement = 0.0;
  double solver_wall_time_seconds = 0.0;
  std::vector<OrToolsGriddedBoundaryWindowResult> windows;
};

/**
 * Refine adjacent stripe columns through compact exact boundary windows.
 *
 * A window starts from one row in the left stripe and its nearest-Y row in the
 * right stripe. Each seed expands to include every row occupied by a selected
 * multi-region component. Oversized closures are skipped, and accepted
 * windows are applied sequentially so later models observe earlier changes.
 */
class OrToolsGriddedBoundaryRefiner {
 public:
  OrToolsGriddedBoundaryRefiner(
      Circuit* circuit, const OrToolsGriddedBoundaryRefinerConfig& config = {});

  /** Run configured alternating boundary sweeps. */
  OrToolsGriddedBoundaryRefinerResult Optimize(
      std::vector<StripeColumn>* columns) const;

 private:
  struct BoundaryTarget {
    int first_column = -1;
    int second_column = -1;
    int first_stripe = -1;
    int second_stripe = -1;
    int first_stripe_id = -1;
    int second_stripe_id = -1;
    int first_seed_row = -1;
    int second_seed_row = -1;
    Stripe* first = nullptr;
    Stripe* second = nullptr;
  };

  /** Return row pointers in ascending physical Y order. */
  std::vector<GriddedRow*> SortedRows(Stripe* stripe) const;

  /** Expand one seed row until no selected component crosses the range. */
  std::pair<int, int> ClosedRowRange(Stripe* stripe, int seed_row) const;

  /** Return the row whose center is nearest to `y`. */
  int NearestRowIndex(const std::vector<GriddedRow*>& rows, double y) const;

  /** Enumerate vertically aligned seeds across adjacent stripe columns. */
  std::vector<BoundaryTarget> BuildTargets(
      std::vector<StripeColumn>* columns) const;

  /** Count distinct components in two closed row ranges. */
  int ComponentCount(const BoundaryTarget& target,
                     const std::pair<int, int>& first_range,
                     const std::pair<int, int>& second_range) const;

  Circuit* circuit_ = nullptr;
  OrToolsGriddedBoundaryRefinerConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_ORTOOLS_GRIDDED_BOUNDARY_REFINER_H_
