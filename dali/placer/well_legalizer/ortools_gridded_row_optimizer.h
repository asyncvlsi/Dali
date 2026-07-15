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
#ifndef DALI_PLACER_WELL_LEGALIZER_ORTOOLS_GRIDDED_ROW_OPTIMIZER_H_
#define DALI_PLACER_WELL_LEGALIZER_ORTOOLS_GRIDDED_ROW_OPTIMIZER_H_

#include <unordered_map>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/ortools_fixed_row_displacement_optimizer.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Runtime and model-size controls for one fixed-row CP-SAT refinement pass. */
struct OrToolsGriddedRowOptimizerConfig {
  double maximum_time_seconds_per_model = 0.05;
  double maximum_total_time_seconds = 10.0;
  // Batches split only at row boundaries, so one large row may exceed this.
  int target_components_per_model = 128;
  int number_of_workers = 1;
  int net_ignore_threshold = 100;
  double displacement_weight = 0.0;
};

/** Aggregate result from independently refining bounded row batches. */
struct OrToolsGriddedRowOptimizerResult {
  bool available = false;
  int attempted_models = 0;
  int solved_models = 0;
  int accepted_models = 0;
  int improved_models = 0;
  bool time_budget_exhausted = false;
  double hpwl_before = 0.0;
  double hpwl_after = 0.0;
  double solver_wall_time_seconds = 0.0;
};

/**
 * Refine legal gridded-row X coordinates with optional OR-Tools CP-SAT.
 *
 * Row assignment and left-to-right order remain fixed. Small batches of rows
 * are solved against the current locations of pins outside each batch, making
 * the pass a bounded coordinate-descent step rather than a replacement
 * legalizer.
 */
class OrToolsGriddedRowOptimizer {
 public:
  OrToolsGriddedRowOptimizer(
      Circuit* circuit, const OrToolsGriddedRowOptimizerConfig& config = {});

  /** Optimize every non-empty row batch and retain only legal, non-regressing
   * solutions. */
  OrToolsGriddedRowOptimizerResult Optimize(
      std::vector<StripeColumn>* columns) const;

 private:
  /** Build one solver model and the lookup data needed to validate it. */
  FixedRowDisplacementModel BuildRowBatchModel(
      const std::vector<const GriddedRow*>& rows,
      std::unordered_map<int, Component*>* components_by_id,
      std::vector<int>* affected_net_ids) const;

  /** Optimize one row batch and update aggregate counters. */
  void OptimizeRowBatch(const std::vector<const GriddedRow*>& rows,
                        const FixedRowDisplacementSolverConfig& solver_config,
                        const OrToolsFixedRowDisplacementOptimizer& optimizer,
                        OrToolsGriddedRowOptimizerResult* aggregate) const;

  /** Return exact HPWL for the nets affected by one model. */
  double AffectedNetHpwl(const std::vector<int>& net_ids) const;

  Circuit* circuit_ = nullptr;
  OrToolsGriddedRowOptimizerConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_ORTOOLS_GRIDDED_ROW_OPTIMIZER_H_
