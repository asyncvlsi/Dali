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
#ifndef DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_WHOLE_DESIGN_MODEL_BUILDER_H_
#define DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_WHOLE_DESIGN_MODEL_BUILDER_H_

#include <cstdint>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/exact_gridded_legalization_model.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Configuration shared by all stripes in a whole-design exact model. */
struct ExactGriddedWholeDesignBuilderConfig {
  int net_ignore_threshold = 100;
  int minimum_p_well_height = 0;
  int minimum_n_well_height = 0;
  /** Allow a component to move to any physically compatible stripe. */
  bool allow_cross_stripe_moves = true;
  /** Include every physically possible row slot instead of current rows only.
   */
  bool use_full_row_slot_capacity = true;
};

/** Size information used to judge whether a solver formulation can scale. */
struct ExactGriddedWholeDesignBuildStats {
  int stripe_count = 0;
  int active_row_count = 0;
  int row_slot_count = 0;
  int component_count = 0;
  int net_count = 0;
  std::int64_t enumerated_placement_choice_upper_bound = 0;
};

/** Exact model and its construction statistics. */
struct ExactGriddedWholeDesignBuildResult {
  ExactGriddedLegalizationModel model;
  ExactGriddedWholeDesignBuildStats stats;
};

/**
 * Build one exact legalization model for every movable component.
 *
 * Existing gridded rows provide a known legal placement hint. Each component
 * may move to every stripe with enough horizontal and vertical capacity. The
 * physical stripe height determines the number of row slots, so the model is
 * not restricted to the number of rows created by the heuristic legalizer.
 */
class ExactGriddedWholeDesignModelBuilder {
 public:
  ExactGriddedWholeDesignModelBuilder(
      Circuit* circuit,
      const ExactGriddedWholeDesignBuilderConfig& config = {});

  /** Build and validate the whole-design model from finalized stripe rows. */
  ExactGriddedWholeDesignBuildResult Build(
      std::vector<StripeColumn>* columns) const;

 private:
  Circuit* circuit_ = nullptr;
  ExactGriddedWholeDesignBuilderConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_WHOLE_DESIGN_MODEL_BUILDER_H_
