#ifndef DALI_MULTI_REGION_PLACEMENT_VALIDATOR_H_
#define DALI_MULTI_REGION_PLACEMENT_VALIDATOR_H_
#include "gridded_placement_validator.h"
namespace dali {
struct MultiRegionPlacementLegalityReport {
  std::size_t inventory = 0, regions = 0, geometry = 0, alignment = 0,
              stretch = 0;
  GriddedPlacementLegalityReport physical;
  std::size_t violations() const {
    return inventory + regions + geometry + alignment + stretch +
           physical.TotalViolationCount();
  }
};
// Checks complete region ownership, stretching, NP alignment, full rectangles
// and the same physical-completion contract used by ordinary gridded placement.
MultiRegionPlacementLegalityReport
ValidateMultiRegionPlacement(Circuit &circuit,
                             std::vector<StripeColumn> &columns,
                             GriddedPlacementValidationConfig config = {});
} // namespace dali
#endif
