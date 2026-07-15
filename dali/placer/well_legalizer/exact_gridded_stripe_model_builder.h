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
#ifndef DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_STRIPE_MODEL_BUILDER_H_
#define DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_STRIPE_MODEL_BUILDER_H_

#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/exact_gridded_legalization_model.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Controls geometry and net selection for one exact stripe subproblem. */
struct ExactGriddedStripeModelBuilderConfig {
  int net_ignore_threshold = 100;
  int minimum_p_well_height = 0;
  int minimum_n_well_height = 0;
};

/** A stripe-local model plus live objects needed to validate its solution. */
struct ExactGriddedStripeBuildResult {
  ExactGriddedLegalizationModel model;
  std::vector<GriddedRow*> rows;
  std::vector<Component*> components;
  std::vector<int> affected_net_ids;
};

/**
 * Build one exact subproblem from a finalized gridded stripe.
 *
 * Every component assigned to the stripe becomes a solver variable with its
 * current row as the discrete-placement hint. Pins belonging to components
 * outside the stripe remain fixed at their live circuit coordinates, making
 * the local objective an exact conditional HPWL objective for the selected
 * production nets.
 */
class ExactGriddedStripeModelBuilder {
 public:
  ExactGriddedStripeModelBuilder(
      Circuit* circuit,
      const ExactGriddedStripeModelBuilderConfig& config = {});

  /** Build and validate the subproblem for `stripe`. */
  ExactGriddedStripeBuildResult Build(Stripe* stripe, int stripe_id) const;

  /**
   * Build a closed subproblem over sorted row indices `[first_row, last_row]`.
   *
   * The range must contain every row occupied by each selected multi-region
   * component; splitting a component across the boundary is rejected.
   */
  ExactGriddedStripeBuildResult BuildRowBand(Stripe* stripe, int stripe_id,
                                             int first_row, int last_row) const;

 private:
  /** Build a model from an already sorted, closed row set. */
  ExactGriddedStripeBuildResult BuildRows(
      Stripe* stripe, int stripe_id,
      std::vector<GriddedRow*> selected_rows) const;

  Circuit* circuit_ = nullptr;
  ExactGriddedStripeModelBuilderConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_STRIPE_MODEL_BUILDER_H_
