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
#ifndef DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_LEGALIZATION_UTIL_H_
#define DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_LEGALIZATION_UTIL_H_

#include <string>

#include "dali/placer/well_legalizer/exact_gridded_legalization_model.h"

namespace dali {

/** Return one cell region in physical bottom-up order for N or FS. */
ExactGriddedCellRegion GetExactGriddedPhysicalRegion(
    const ExactGriddedCell& cell, int physical_region_index, bool is_flipped);

/**
 * Check a discrete placement against alternating gridded-row orientations.
 *
 * `required_first_row_orient_n` receives the row-zero phase required by the
 * placement, rather than the orientation of the component's starting row.
 */
bool ExactGriddedCandidateMatchesAlternatingRows(
    const ExactGriddedCell& cell, int start_row, bool is_flipped,
    bool* required_first_row_orient_n);

/** Return the first physical constraint violated by a complete model hint. */
std::string ValidateExactSolutionHint(
    const ExactGriddedLegalizationModel& model);

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_EXACT_GRIDDED_LEGALIZATION_UTIL_H_
