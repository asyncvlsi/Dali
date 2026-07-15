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
#ifndef DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_LEGALIZATION_MODEL_H_
#define DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_LEGALIZATION_MODEL_H_

#include <string>
#include <vector>

namespace dali {

/** P/N-well geometry for one cell region in unflipped bottom-up order. */
struct ExactGriddedCellRegion {
  int p_well_height = 0;
  int n_well_height = 0;
  bool n_well_above_p_well = true;
};

/** One movable component in an exact gridded legalization model. */
struct ExactGriddedCell {
  int component_id = -1;
  int width = 0;
  int initial_x = 0;
  int initial_y = 0;
  std::vector<ExactGriddedCellRegion> regions;
  std::vector<int> candidate_stripe_ids;
  // Negative ids omit the discrete placement hint. The initial X/Y values
  // remain useful independently for displacement and coordinate hints.
  int initial_stripe_id = -1;
  int initial_start_row = -1;
  bool initial_is_flipped = false;
};

/** One fixed stripe into which the solver may pack gridded rows. */
struct ExactGriddedStripe {
  int stripe_id = -1;
  int lx = 0;
  int ly = 0;
  int ux = 0;
  int uy = 0;
  int maximum_rows = 0;
  int left_boundary_margin = 0;
  int right_boundary_margin = 0;
  int minimum_p_well_height = 0;
  int minimum_n_well_height = 0;
};

/** Orientation-aware pin coordinates used by the full HPWL objective. */
struct ExactGriddedNetPin {
  // A negative component id represents a fixed pin at fixed_x/fixed_y.
  int component_id = -1;
  double offset_x_n = 0.0;
  double offset_y_n = 0.0;
  double offset_x_fs = 0.0;
  double offset_y_fs = 0.0;
  double fixed_x = 0.0;
  double fixed_y = 0.0;
};

/** Weighted net represented in Dali placement-grid units. */
struct ExactGriddedNet {
  std::vector<ExactGriddedNetPin> pins;
  double weight = 1.0;
};

/**
 * Complete input for exact gridded legalization within fixed stripe geometry.
 *
 * The solver chooses each component's stripe, starting row, N/FS orientation,
 * and X coordinate. Row count, row P/N-well heights, vertical stack location,
 * and left-to-right ordering are therefore decisions rather than input from a
 * heuristic legalizer.
 */
struct ExactGriddedLegalizationModel {
  std::vector<ExactGriddedCell> cells;
  std::vector<ExactGriddedStripe> stripes;
  std::vector<ExactGriddedNet> nets;
  double distance_scale_x = 1.0;
  double distance_scale_y = 1.0;

  /** Return an empty string when the model is structurally valid. */
  std::string Validate() const;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_LEGALIZATION_MODEL_H_
