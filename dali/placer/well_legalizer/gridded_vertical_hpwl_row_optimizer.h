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
#ifndef DALI_PLACER_WELL_LEGALIZER_GRIDDED_VERTICAL_HPWL_ROW_OPTIMIZER_H_
#define DALI_PLACER_WELL_LEGALIZER_GRIDDED_VERTICAL_HPWL_ROW_OPTIMIZER_H_

#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/ortools_vertical_hpwl_row_assignment.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/**
 * Controls for the experimental decomposed vertical-HPWL row optimizer.
 *
 * Two-row disjoint windows are the retained research baseline. Larger or
 * overlapping windows can model more interactions but were substantially more
 * expensive and did not consistently improve final detailed-placement HPWL.
 */
struct GriddedVerticalHpwlRowOptimizerConfig {
  int rows_per_window = 2;
  int row_stride = 2;
  // Select the first row in the repeating window schedule. An offset of one
  // with two-row windows evaluates the complementary odd row pairs.
  int first_row_offset = 0;
  int maximum_sweeps = 1;
  int net_ignore_threshold = 100;
  double maximum_time_seconds_per_window = 0.05;
  double maximum_total_time_seconds = 60.0;
  int number_of_workers = 1;
  int coordinate_scale = 1000;
  double minimum_hpwl_improvement = 0.0;
  /**
   * Compare the solver assignment against the current rows after both receive
   * the same bounded local detailed-placement closure.
   *
   * Immediate row-assignment HPWL does not capture the relocation, swap, X
   * packing, and reordering opportunities exposed by a different row
   * population. This is deliberately opt-in while experiments establish its
   * quality/runtime ROI.
   */
  bool compare_local_detailed_closure = false;
  /** Bound expensive closure comparisons to the highest-potential windows. */
  int maximum_local_closure_windows = 64;
};

/** Aggregate diagnostics from experimental row-assignment windows. */
struct GriddedVerticalHpwlRowOptimizerResult {
  bool available = false;
  bool time_budget_exhausted = false;
  int completed_sweeps = 0;
  int attempted_windows = 0;
  int skipped_closure_windows = 0;
  int solved_windows = 0;
  int accepted_windows = 0;
  int reassigned_components = 0;
  double hpwl_before = 0.0;
  double hpwl_after = 0.0;
  int closure_rejected_windows = 0;
  double local_closure_baseline_gain = 0.0;
  double local_closure_candidate_gain = 0.0;
  double solver_wall_time_seconds = 0.0;
};

/**
 * Experimentally refine row membership using vertical weighted HPWL.
 *
 * The optimizer solves small row-only CP-SAT windows, reconstructs legal X
 * locations with Dali's row packer, and commits a window only when exact
 * affected-net HPWL improves. It intentionally remains opt-in because local
 * gains have not consistently predicted the final detailed-placement basin.
 */
class GriddedVerticalHpwlRowOptimizer {
 public:
  GriddedVerticalHpwlRowOptimizer(
      Circuit* circuit,
      const GriddedVerticalHpwlRowOptimizerConfig& config = {});

  /** Optimize all eligible stripes within the configured time budget. */
  GriddedVerticalHpwlRowOptimizerResult Optimize(
      std::vector<StripeColumn>* columns) const;

 private:
  struct Window {
    Stripe* stripe = nullptr;
    std::vector<GriddedRow*> rows;
    int first_row_index = 0;
  };

  /** Build fixed-size row windows from one stripe for the given sweep. */
  std::vector<Window> BuildWindows(Stripe* stripe, int sweep) const;

  /** Return the legal component lower-left Y for a finalized row. */
  double ComponentLlyInRow(const Component& component,
                           const GriddedRow& row) const;

  /** Rank a local row window by the low-fanout net HPWL it can affect. */
  double WindowPotential(const Window& window) const;

  /** Solve, pack, evaluate, and conditionally commit one window. */
  void OptimizeWindow(const Window& window,
                      GriddedVerticalHpwlRowOptimizerResult* result) const;

  /** Apply one bounded production-equivalent detailed round to a window. */
  void RunLocalDetailedClosure(const Window& window) const;

  Circuit* circuit_ = nullptr;
  GriddedVerticalHpwlRowOptimizerConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_GRIDDED_VERTICAL_HPWL_ROW_OPTIMIZER_H_
