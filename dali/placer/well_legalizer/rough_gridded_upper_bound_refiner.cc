/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "rough_gridded_upper_bound_refiner.h"

#include <utility>

#include "dali/common/elapsed_time.h"
#include "dali/common/logging.h"
#include "gridded_cell_well_legalizer.h"

namespace dali {

RoughGriddedUpperBoundRefiner::RoughGriddedUpperBoundRefiner(
    GriddedCellWellLegalizer* well_legalizer, bool enable_overflow_balancing)
    : well_legalizer_(well_legalizer),
      enable_overflow_balancing_(enable_overflow_balancing) {
  DaliExpects(well_legalizer_ != nullptr,
              "Rough gridded refiner requires a well legalizer");
}

void RoughGriddedUpperBoundRefiner::Initialize(double placement_density) {
  total_wall_time_ = 0.0;
  well_legalizer_->SetPlacementDensity(placement_density);
}

GlobalUpperBoundRefinement RoughGriddedUpperBoundRefiner::Refine(
    int iteration) {
  ElapsedTime timer;
  timer.RecordStartTime();
  ProvisionalGriddedPlacementResult provisional =
      well_legalizer_->RunProvisionalPlacement(enable_overflow_balancing_);
  timer.RecordEndTime();
  total_wall_time_ += timer.GetWallTime();

  std::vector<GlobalUpperBoundViolation> violations;
  violations.reserve(provisional.violations.size());
  size_t affected_component_count = 0;
  for (const ProvisionalGriddedPlacementViolation& provisional_violation :
       provisional.violations) {
    GlobalUpperBoundViolation violation;
    violation.lx = provisional_violation.lx;
    violation.ly = provisional_violation.ly;
    violation.ux = provisional_violation.ux;
    violation.uy = provisional_violation.uy;
    violation.overflow = provisional_violation.overflow_height;
    violation.component_ids = provisional_violation.component_ids;
    affected_component_count += violation.component_ids.size();
    violations.push_back(std::move(violation));
  }

  LOG(info) << "  Rough gridded upper bound, iteration " << iteration << ":\n"
            << "    feasible : " << provisional.feasible << "\n"
            << "    mode     : "
            << (provisional.used_scavenge ? "scavenge" : "configured") << "\n"
            << "    HPWL     : " << provisional.hpwl << "\n"
            << "    overflow : " << provisional.overflow << "\n"
            << "    violations: " << violations.size() << "\n"
            << "    affected components: " << affected_component_count << "\n"
            << "    balanced components: "
            << provisional.balanced_component_count << "\n"
            << "    wall time: " << timer.GetWallTime() << "s\n";

  return {provisional.feasible, provisional.hpwl, provisional.overflow,
          std::move(violations)};
}

double RoughGriddedUpperBoundRefiner::GetTime() const {
  return total_wall_time_;
}

void RoughGriddedUpperBoundRefiner::Close() {
  well_legalizer_->ClearProvisionalState();
}

}  // namespace dali
