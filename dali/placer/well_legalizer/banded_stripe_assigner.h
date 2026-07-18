/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#ifndef DALI_PLACER_WELL_LEGALIZER_BANDED_STRIPE_ASSIGNER_H_
#define DALI_PLACER_WELL_LEGALIZER_BANDED_STRIPE_ASSIGNER_H_

#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/gridded_capacity_estimator.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Controls the Y-banded transport from global placement to stripe ownership.
 */
struct BandedStripeAssignmentConfig {
  int band_count = 32;
  bool require_projected_hpwl_improvement = true;
  double minimum_projected_hpwl_improvement = 0.0;
  int net_ignore_threshold = 100;
  GriddedCapacityConfig capacity;
};

/** Diagnostics for one nonlinear stripe-ownership assignment. */
struct BandedStripeAssignmentResult {
  int populated_band_count = 0;
  int assigned_component_count = 0;
  int proposed_move_count = 0;
  int moved_component_count = 0;
  int rejected_hpwl_move_count = 0;
  int rejected_capacity_move_count = 0;
  /** Number of band/column targets whose input ownership exceeds its budget. */
  int initially_overloaded_target_count = 0;
  double projected_hpwl_improvement = 0.0;
  double average_column_displacement = 0.0;
  int maximum_column_displacement = 0;
};

/**
 * Assign components to fixed stripes through Y-banded monotone transport.
 *
 * Each horizontal band independently maps cumulative standalone gridded demand
 * to cumulative local stripe capacity. The mapping preserves component X order
 * inside the band, but its effective column cutlines may differ between bands.
 *
 * Standalone demand conservatively overestimates true row demand because cells
 * can share well height after clustering. The assigner therefore normalizes
 * each band's physical stripe area into a demand budget. A proposed move may
 * enter a target only when it remains within that budget; final clustering is
 * still the authority for complete row and well legality.
 */
class BandedStripeAssigner {
 public:
  BandedStripeAssigner(Circuit* circuit, BandedStripeAssignmentConfig config);

  /** Replace geometric ownership in `columns` with banded transport ownership.
   */
  BandedStripeAssignmentResult Assign(std::vector<StripeColumn>* columns) const;

 private:
  /** Return usable stripe area overlapping one horizontal band. */
  unsigned long long BandCapacity(const StripeColumn& column, int band_lly,
                                  int band_ury) const;

  /** Estimate affected-net HPWL change after projecting into a target column.
   */
  double ProjectedHpwlDelta(Component* component,
                            const StripeColumn& target_column) const;

  Circuit* circuit_ = nullptr;
  BandedStripeAssignmentConfig config_;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_BANDED_STRIPE_ASSIGNER_H_
