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
#ifndef DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_LEGALIZATION_MODEL_BUILDER_H_
#define DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_LEGALIZATION_MODEL_BUILDER_H_

#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/exact_gridded_legalization_model.h"

namespace dali {

/** One movable component and the stripes available to the exact solver. */
struct ExactGriddedComponentDomain {
  Component* component = nullptr;
  std::vector<int> candidate_stripe_ids;
};

/** Controls which production nets are copied into an exact local model. */
struct ExactGriddedModelBuilderConfig {
  // Nets at or above the threshold are omitted, matching Dali's placement
  // convention for high-fanout nets that are unlikely to carry locality.
  int net_ignore_threshold = 100;
};

/**
 * Build an exact local legalization model from a live Dali circuit.
 *
 * Components in `domains` become solver variables. Other components connected
 * to an affected net remain fixed at their current pin locations, so a bounded
 * window retains its full external HPWL context. The builder deliberately does
 * not choose candidate stripes; the caller defines that neighborhood.
 */
class ExactGriddedLegalizationModelBuilder {
 public:
  ExactGriddedLegalizationModelBuilder(
      Circuit* circuit, const ExactGriddedModelBuilderConfig& config = {});

  /** Translate component domains and stripe geometry into a validated model. */
  ExactGriddedLegalizationModel Build(
      const std::vector<ExactGriddedComponentDomain>& domains,
      const std::vector<ExactGriddedStripe>& stripes) const;

 private:
  Circuit* circuit_ = nullptr;
  ExactGriddedModelBuilderConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_LEGALIZATION_MODEL_BUILDER_H_
