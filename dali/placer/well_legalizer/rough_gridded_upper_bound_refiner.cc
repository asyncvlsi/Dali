/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "rough_gridded_upper_bound_refiner.h"

#include "dali/common/elapsed_time.h"
#include "dali/common/logging.h"
#include "gridded_cell_well_legalizer.h"

namespace dali {

RoughGriddedUpperBoundRefiner::RoughGriddedUpperBoundRefiner(
    GriddedCellWellLegalizer* well_legalizer)
    : well_legalizer_(well_legalizer) {
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
      well_legalizer_->RunProvisionalPlacement();
  timer.RecordEndTime();
  total_wall_time_ += timer.GetWallTime();

  LOG(info) << "  Rough gridded upper bound, iteration " << iteration << ":\n"
            << "    feasible : " << provisional.feasible << "\n"
            << "    mode     : "
            << (provisional.used_scavenge ? "scavenge" : "configured") << "\n"
            << "    HPWL     : " << provisional.hpwl << "\n"
            << "    overflow : " << provisional.overflow << "\n"
            << "    wall time: " << timer.GetWallTime() << "s\n";

  return {provisional.feasible, provisional.hpwl, provisional.overflow};
}

double RoughGriddedUpperBoundRefiner::GetTime() const {
  return total_wall_time_;
}

void RoughGriddedUpperBoundRefiner::Close() {
  well_legalizer_->ClearProvisionalState();
}

}  // namespace dali
