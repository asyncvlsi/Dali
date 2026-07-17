/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "rough_gridded_upper_bound_refiner.h"

#include <utility>

#include "dali/common/elapsed_time.h"
#include "dali/common/logging.h"
#include "gridded_cell_well_legalizer.h"

namespace dali {

constexpr int kProvisionalRowGeometryIterationCount = 10;

/** Convert well-legalizer diagnostics to the global-refiner interface. */
std::vector<GlobalUpperBoundViolation> ConvertProvisionalViolations(
    const std::vector<ProvisionalGriddedPlacementViolation>&
        provisional_violations) {
  std::vector<GlobalUpperBoundViolation> violations;
  violations.reserve(provisional_violations.size());
  for (const ProvisionalGriddedPlacementViolation& provisional_violation :
       provisional_violations) {
    GlobalUpperBoundViolation violation;
    violation.lx = provisional_violation.lx;
    violation.ly = provisional_violation.ly;
    violation.ux = provisional_violation.ux;
    violation.uy = provisional_violation.uy;
    violation.overflow = provisional_violation.overflow_height;
    violation.component_ids = provisional_violation.component_ids;
    violations.push_back(std::move(violation));
  }
  return violations;
}

/** Count component references across a collection of violation regions. */
size_t CountAffectedComponents(
    const std::vector<GlobalUpperBoundViolation>& violations) {
  size_t affected_component_count = 0;
  for (const GlobalUpperBoundViolation& violation : violations) {
    affected_component_count += violation.component_ids.size();
  }
  return affected_component_count;
}

RoughGriddedUpperBoundRefiner::RoughGriddedUpperBoundRefiner(
    GriddedCellWellLegalizer* well_legalizer, bool enable_overflow_balancing,
    bool rollback_destabilizing_feedback)
    : well_legalizer_(well_legalizer),
      enable_overflow_balancing_(enable_overflow_balancing),
      rollback_destabilizing_feedback_(rollback_destabilizing_feedback) {
  DaliExpects(well_legalizer_ != nullptr,
              "Rough gridded refiner requires a well legalizer");
}

void RoughGriddedUpperBoundRefiner::Initialize(double placement_density) {
  total_wall_time_ = 0.0;
  row_geometry_feedback_enabled_ = true;
  previous_refinement_used_row_geometry_ = false;
  well_legalizer_->SetPlacementDensity(placement_density);
}

GlobalUpperBoundRefinement RoughGriddedUpperBoundRefiner::Refine(
    int iteration) {
  ElapsedTime timer;
  timer.RecordStartTime();
  const bool refine_row_geometry =
      row_geometry_feedback_enabled_ &&
      iteration < kProvisionalRowGeometryIterationCount;
  ProvisionalGriddedPlacementResult provisional =
      well_legalizer_->RunProvisionalPlacement(enable_overflow_balancing_,
                                               refine_row_geometry);
  timer.RecordEndTime();
  total_wall_time_ += timer.GetWallTime();

  std::vector<GlobalUpperBoundViolation> initial_violations =
      ConvertProvisionalViolations(provisional.initial_violations);
  std::vector<GlobalUpperBoundViolation> violations =
      ConvertProvisionalViolations(provisional.violations);

  LOG(info) << "  Rough gridded upper bound, iteration " << iteration << ":\n"
            << "    feasible : " << provisional.feasible << "\n"
            << "    mode     : "
            << (provisional.used_scavenge ? "scavenge" : "configured") << "\n"
            << "    HPWL     : " << provisional.hpwl << "\n"
            << "    initial overflow: " << provisional.initial_overflow << "\n"
            << "    initial violations: " << initial_violations.size() << "\n"
            << "    initially affected components: "
            << CountAffectedComponents(initial_violations) << "\n"
            << "    overflow : " << provisional.overflow << "\n"
            << "    violations: " << violations.size() << "\n"
            << "    affected components: "
            << CountAffectedComponents(violations) << "\n"
            << "    balanced components: "
            << provisional.balanced_component_count << "\n"
            << "    wall time: " << timer.GetWallTime() << "s\n";

  GlobalUpperBoundRefinement refinement;
  refinement.feasible = provisional.feasible;
  refinement.rollback_previous_anchor_feedback =
      rollback_destabilizing_feedback_ && !provisional.feasible &&
      previous_refinement_used_row_geometry_;
  if (refinement.rollback_previous_anchor_feedback) {
    row_geometry_feedback_enabled_ = false;
    LOG(info) << "    provisional row geometry destabilized physical "
                 "feasibility; disable it and roll back its anchor feedback\n";
  }
  previous_refinement_used_row_geometry_ =
      provisional.feasible && refine_row_geometry;
  refinement.hpwl = provisional.hpwl;
  refinement.initial_overflow = provisional.initial_overflow;
  refinement.initial_violations = std::move(initial_violations);
  refinement.overflow = provisional.overflow;
  refinement.violations = std::move(violations);
  refinement.anchor_component_ids =
      std::move(provisional.balanced_component_ids);
  refinement.component_rows = std::move(provisional.component_rows);
  return refinement;
}

double RoughGriddedUpperBoundRefiner::GetTime() const {
  return total_wall_time_;
}

void RoughGriddedUpperBoundRefiner::Close() {
  well_legalizer_->ClearProvisionalState();
}

}  // namespace dali
