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
#ifndef DALI_PLACER_WELL_LEGALIZER_ORTOOLS_GRIDDED_BOUNDARY_OPTIMIZER_H_
#define DALI_PLACER_WELL_LEGALIZER_ORTOOLS_GRIDDED_BOUNDARY_OPTIMIZER_H_

#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/ortools_exact_gridded_legalizer.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Solver and acceptance controls for one compact cross-stripe model. */
struct OrToolsGriddedBoundaryOptimizerConfig {
  double maximum_time_seconds = 1.0;
  int number_of_workers = 1;
  int net_ignore_threshold = 100;
  int minimum_p_well_height = 0;
  int minimum_n_well_height = 0;
  int maximum_row_displacement = 0;
  int maximum_assignment_changes = 4;
  bool use_solution_hint = true;
};

/** Diagnostics from one transactional cross-stripe optimization. */
struct OrToolsGriddedBoundaryOptimizerResult {
  ExactGriddedLegalizationStatus status =
      ExactGriddedLegalizationStatus::kUnavailable;
  bool available = false;
  bool accepted = false;
  int component_count = 0;
  int net_count = 0;
  int reassigned_component_count = 0;
  int cross_stripe_component_count = 0;
  double modeled_hpwl_before = 0.0;
  double modeled_hpwl_after = 0.0;
  double affected_hpwl_before = 0.0;
  double affected_hpwl_after = 0.0;
  double solver_wall_time_seconds = 0.0;
  double relative_gap = 0.0;
};

/**
 * Optimize one closed row-band pair across a gridded stripe boundary.
 *
 * The complete solver solution is applied to both row stacks atomically. It is
 * retained only when every affected row remains legal and both modeled and
 * full affected-net HPWL strictly improve; otherwise the original assignment,
 * coordinates, and orientations are restored.
 */
class OrToolsGriddedBoundaryOptimizer {
 public:
  OrToolsGriddedBoundaryOptimizer(
      Circuit* circuit,
      const OrToolsGriddedBoundaryOptimizerConfig& config = {});

  /** Optimize two inclusive closed row bands and apply an accepted result. */
  OrToolsGriddedBoundaryOptimizerResult Optimize(
      Stripe* first_stripe, int first_stripe_id, int first_row,
      int first_last_row, Stripe* second_stripe, int second_stripe_id,
      int second_row, int second_last_row) const;

 private:
  /** Return affected-net HPWL, optionally applying the fanout cutoff. */
  double AffectedNetHpwl(const std::vector<int>& net_ids,
                         bool apply_fanout_cutoff) const;

  Circuit* circuit_ = nullptr;
  OrToolsGriddedBoundaryOptimizerConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_ORTOOLS_GRIDDED_BOUNDARY_OPTIMIZER_H_
