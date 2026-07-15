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
#ifndef DALI_PLACER_WELL_LEGALIZER_ORTOOLS_COMPACT_GRIDDED_LEGALIZER_H_
#define DALI_PLACER_WELL_LEGALIZER_ORTOOLS_COMPACT_GRIDDED_LEGALIZER_H_

#include "dali/placer/well_legalizer/ortools_exact_gridded_legalizer.h"

namespace dali {

/**
 * Solve whole-design gridded legalization with a compact CP-SAT model.
 *
 * A sparse integer chooses each component's row slot. Element constraints
 * select stripe bounds and row geometry, while one global two-dimensional
 * non-overlap constraint enforces cell legality. This avoids enumerating a
 * Boolean variable for every possible stripe, row, and orientation tuple.
 */
class OrToolsCompactGriddedLegalizer {
 public:
  /** Return true when Dali was built with the supported OR-Tools backend. */
  static bool IsAvailable();

  /** Solve the supplied model and return its incumbent and optimality bound. */
  ExactGriddedLegalizationResult Solve(
      const ExactGriddedLegalizationModel& model,
      const ExactGriddedLegalizationConfig& config = {}) const;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_ORTOOLS_COMPACT_GRIDDED_LEGALIZER_H_
