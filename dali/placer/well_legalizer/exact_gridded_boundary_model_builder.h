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
#ifndef DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_BOUNDARY_MODEL_BUILDER_H_
#define DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_BOUNDARY_MODEL_BUILDER_H_

#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/exact_gridded_legalization_model.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Controls geometry and net selection for one cross-stripe subproblem. */
struct ExactGriddedBoundaryModelBuilderConfig {
  int net_ignore_threshold = 100;
  int minimum_p_well_height = 0;
  int minimum_n_well_height = 0;
};

/** Live rows represented by one stripe in a boundary model. */
struct ExactGriddedBoundaryRowSet {
  int stripe_id = -1;
  Stripe* stripe = nullptr;
  std::vector<GriddedRow*> rows;
};

/** A two-stripe model plus live objects needed for transactional application.
 */
struct ExactGriddedBoundaryBuildResult {
  ExactGriddedLegalizationModel model;
  std::vector<ExactGriddedBoundaryRowSet> row_sets;
  std::vector<Component*> components;
  std::vector<int> affected_net_ids;
};

/**
 * Build a compact exact model across one boundary between adjacent stripes.
 *
 * Every component occupying either closed row band becomes a solver variable.
 * A component may use either modeled stripe when its width and region count
 * fit. Components outside the two bands remain fixed net anchors, preserving
 * exact conditional HPWL for production nets below the fanout cutoff.
 */
class ExactGriddedBoundaryModelBuilder {
 public:
  ExactGriddedBoundaryModelBuilder(
      Circuit* circuit,
      const ExactGriddedBoundaryModelBuilderConfig& config = {});

  /** Build two closed row bands using inclusive sorted row indices. */
  ExactGriddedBoundaryBuildResult Build(Stripe* first_stripe,
                                        int first_stripe_id, int first_row,
                                        int first_last_row,
                                        Stripe* second_stripe,
                                        int second_stripe_id, int second_row,
                                        int second_last_row) const;

 private:
  /** Select sorted rows and reject a range that splits a component. */
  std::vector<GriddedRow*> SelectClosedRows(Stripe* stripe, int first_row,
                                            int last_row) const;

  Circuit* circuit_ = nullptr;
  ExactGriddedBoundaryModelBuilderConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_BOUNDARY_MODEL_BUILDER_H_
