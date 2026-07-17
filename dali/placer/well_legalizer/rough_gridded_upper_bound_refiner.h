/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#ifndef DALI_PLACER_WELL_LEGALIZER_ROUGH_GRIDDED_UPPER_BOUND_REFINER_H_
#define DALI_PLACER_WELL_LEGALIZER_ROUGH_GRIDDED_UPPER_BOUND_REFINER_H_

#include "dali/placer/global_placer/global_upper_bound_refiner.h"

namespace dali {

class GriddedCellWellLegalizer;

/** Uses gridded-row clustering to refine global-placement upper bounds. */
class RoughGriddedUpperBoundRefiner : public GlobalUpperBoundRefiner {
 public:
  /** Construct a non-owning adapter around Dali's well legalizer. */
  explicit RoughGriddedUpperBoundRefiner(
      GriddedCellWellLegalizer* well_legalizer,
      bool enable_overflow_balancing = false,
      bool rollback_destabilizing_feedback = true);

  /** Forward target density to the reusable well legalizer. */
  void Initialize(double placement_density) override;

  /** Run one provisional gridded legalization pass. */
  GlobalUpperBoundRefinement Refine(int iteration) override;

  /** Return total wall time spent in provisional legalization. */
  double GetTime() const override;

  /** Release temporary gridded-row state while preserving coordinates. */
  void Close() override;

 private:
  GriddedCellWellLegalizer* well_legalizer_ = nullptr;
  bool enable_overflow_balancing_ = false;
  bool rollback_destabilizing_feedback_ = true;
  bool row_geometry_feedback_enabled_ = true;
  bool previous_refinement_used_row_geometry_ = false;
  double total_wall_time_ = 0.0;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_ROUGH_GRIDDED_UPPER_BOUND_REFINER_H_
