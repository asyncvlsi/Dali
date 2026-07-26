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
#ifndef DALI_PLACER_WELL_LEGALIZER_ORTOOLS_GRIDDED_STRIPE_OPTIMIZER_H_
#define DALI_PLACER_WELL_LEGALIZER_ORTOOLS_GRIDDED_STRIPE_OPTIMIZER_H_

#include <stdint.h>
#include <utility>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/ortools_exact_gridded_legalizer.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Runtime and convergence controls for decomposed exact stripe refinement. */
struct OrToolsGriddedStripeOptimizerConfig {
  double maximum_time_seconds_per_stripe = 5.0;
  double maximum_total_time_seconds = 120.0;
  double minimum_relative_improvement = 1e-5;
  int maximum_sweeps = 2;
  int number_of_workers = 1;
  int net_ignore_threshold = 100;
  int minimum_p_well_height = 0;
  int minimum_n_well_height = 0;
  // Zero preserves row membership; positive values permit nearby row moves.
  int maximum_row_displacement = 0;
  // Negative values leave the number of changed assignments unrestricted.
  int maximum_row_assignment_changes = -1;
  // Penalize physical L1 movement from the input legal placement.
  double displacement_weight = 0.0;
  // Zero solves complete stripes. Positive values create overlapping bands.
  int target_components_per_model = 0;
  int maximum_components_per_model = 96;
  bool run_local_detailed_closure = false;
  bool use_solution_hint = true;
};

/** Diagnostics for one conditional stripe solve. */
struct OrToolsGriddedStripeSolveResult {
  int sweep = -1;
  int column_index = -1;
  int stripe_index = -1;
  int first_row_index = -1;
  int last_row_index = -1;
  int component_count = 0;
  int net_count = 0;
  int reassigned_component_count = 0;
  int64_t model_variable_count = 0;
  int64_t model_constraint_count = 0;
  double modeled_hpwl_before = 0.0;
  double modeled_hpwl_after = 0.0;
  double affected_hpwl_before = 0.0;
  double affected_hpwl_after = 0.0;
  double physical_displacement = 0.0;
  double solver_wall_time_seconds = 0.0;
  double best_objective_bound = 0.0;
  double relative_gap = 0.0;
  bool accepted = false;
  ExactGriddedLegalizationStatus status =
      ExactGriddedLegalizationStatus::kUnavailable;
};

/** Aggregate diagnostics from sequential bidirectional stripe sweeps. */
struct OrToolsGriddedStripeOptimizerResult {
  bool available = false;
  bool time_budget_exhausted = false;
  int completed_sweeps = 0;
  int attempted_models = 0;
  int solved_models = 0;
  int accepted_models = 0;
  int accepted_reassignment_models = 0;
  int accepted_reassigned_components = 0;
  double hpwl_before = 0.0;
  double hpwl_after = 0.0;
  double fixed_row_hpwl_improvement = 0.0;
  double reassignment_hpwl_improvement = 0.0;
  // Sum of accepted per-model movement; repeated moves count more than once.
  double accepted_physical_displacement = 0.0;
  double solver_wall_time_seconds = 0.0;
  std::vector<OrToolsGriddedStripeSolveResult> stripes;
};

/**
 * Improve finalized gridded stripes through conditional CP-SAT subproblems.
 *
 * By default, a solve reorders components within their current rows. An
 * optional row radius also permits nearby row reassignment while preserving
 * finalized row geometry. Pins outside the active stripe are constants, and
 * stripes are updated sequentially so every later model sees all accepted
 * earlier changes. A candidate is committed only when row legality holds,
 * modeled HPWL improves, and full affected-net HPWL passes the configured
 * non-regression guard.
 */
class OrToolsGriddedStripeOptimizer {
 public:
  OrToolsGriddedStripeOptimizer(
      Circuit* circuit, const OrToolsGriddedStripeOptimizerConfig& config = {});

  /** Run alternating stripe sweeps and retain accepted improvements. */
  OrToolsGriddedStripeOptimizerResult Optimize(
      std::vector<StripeColumn>* columns) const;

 private:
  /** Partition a stripe into overlapping, multi-region-safe row bands. */
  std::vector<std::pair<int, int>> BuildRowBands(const Stripe& stripe) const;

  /** Return HPWL over affected nets, optionally applying the fanout cutoff. */
  double AffectedNetHpwl(const std::vector<int>& net_ids,
                         bool apply_fanout_cutoff) const;

  /** Apply one private detailed-placement round to a stripe row band. */
  void RunLocalDetailedClosure(const std::vector<GriddedRow*>& rows) const;

  Circuit* circuit_ = nullptr;
  OrToolsGriddedStripeOptimizerConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_ORTOOLS_GRIDDED_STRIPE_OPTIMIZER_H_
