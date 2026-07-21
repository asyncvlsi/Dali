/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#ifndef DALI_PLACER_WELL_LEGALIZER_GRIDDED_PLACEMENT_VALIDATOR_H_
#define DALI_PLACER_WELL_LEGALIZER_GRIDDED_PLACEMENT_VALIDATOR_H_

#include <cstddef>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Physical-completion features expected in a finalized gridded placement. */
struct GriddedPlacementValidationConfig {
  bool check_component_orientation = true;
  bool expect_well_taps = false;
  bool expect_end_caps = false;
  int space_to_well_tap = 0;
  int pre_end_cap_width = 0;
  int post_end_cap_width = 0;
  // Exact well-tap-count check: total taps must equal well_tap_count_per_row
  // times the row count. Meaningful only for fixed-count patterns; disable it
  // for patterns whose per-row tap count varies (rely on coverage instead).
  bool check_exact_well_tap_count = true;
  int well_tap_count_per_row = 2;

  // Pattern-agnostic latch-up coverage: verify every movable cell lies within
  // MaxPlugDist of some well tap, independent of how taps were placed. Unlike
  // the row-end tap-spacing check, this catches under-tapped interior regions
  // for arbitrary tap patterns (checkerboard, every-other-row, mini-row, ...).
  bool check_well_tap_coverage = false;
  // Latch-up budget in microns; when <= 0, derived from the tech N-well layer's
  // MaxPlugDist.
  double max_plug_dist = 0.0;
};

/** Categorized violations found in a finalized gridded placement. */
struct GriddedPlacementLegalityReport {
  size_t movable_component_count = 0;
  size_t assigned_component_count = 0;
  size_t unassigned_component_count = 0;
  size_t duplicate_assignment_count = 0;
  size_t invalid_component_reference_count = 0;
  size_t row_boundary_violation_count = 0;
  size_t row_overlap_count = 0;
  size_t component_boundary_violation_count = 0;
  size_t component_overlap_count = 0;
  size_t component_y_violation_count = 0;
  size_t component_orientation_violation_count = 0;
  size_t physical_completion_violation_count = 0;
  size_t missing_well_tap_count = 0;
  size_t well_tap_geometry_violation_count = 0;
  size_t well_tap_spacing_violation_count = 0;
  size_t missing_end_cap_count = 0;
  size_t end_cap_geometry_violation_count = 0;
  size_t end_cap_tap_overlap_count = 0;
  size_t physical_component_count_violation_count = 0;
  size_t well_tap_coverage_violation_count = 0;
  // Worst observed cell-to-nearest-tap distance in microns (diagnostic only;
  // does not affect legality).
  double max_well_tap_coverage_gap = 0.0;

  /** Return the total number of categorized legality violations. */
  size_t TotalViolationCount() const;

  /** Return true when no gridded-placement legality violation was found. */
  bool IsLegal() const { return TotalViolationCount() == 0; }

  /** Return true when every movable cell is within MaxPlugDist of a well tap.
   * Meaningful only when coverage checking was enabled; otherwise trivially
   * true. Lets callers gate on the latch-up rule alone. */
  bool IsWellTapCoverageLegal() const {
    return well_tap_coverage_violation_count == 0;
  }
};

/** Validates row ownership, geometry, orientation, and boundary-cell layout. */
class GriddedPlacementValidator {
 public:
  GriddedPlacementValidator(Circuit* circuit,
                            const std::vector<StripeColumn>* columns,
                            GriddedPlacementValidationConfig config = {});

  /** Validate the complete current gridded placement. */
  GriddedPlacementLegalityReport Validate() const;

 private:
  /** Verify each movable cell is within MaxPlugDist of a well tap, for any
   * tap-placement pattern. Accumulates into the report. */
  void ValidateWellTapCoverage(GriddedPlacementLegalityReport& report) const;

  Circuit* circuit_ = nullptr;
  const std::vector<StripeColumn>* columns_ = nullptr;
  GriddedPlacementValidationConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_GRIDDED_PLACEMENT_VALIDATOR_H_
