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
#ifndef DALI_PLACER_WELL_LEGALIZER_GRIDDED_ROW_LOCATION_OPTIMIZER_H_
#define DALI_PLACER_WELL_LEGALIZER_GRIDDED_ROW_LOCATION_OPTIMIZER_H_

#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Summary of one gridded row-location optimization run. */
struct GriddedRowLocationResult {
  int sweeps = 0;
  int groups_considered = 0;
  int groups_moved = 0;
  double hpwl_before = 0.0;
  double hpwl_after = 0.0;
};

/**
 * Shift contiguous gridded-row groups within existing vertical whitespace.
 *
 * Rows in a contiguous group move together, preserving their order, well
 * heights, and abutment. Candidate locations minimize the exact vertical HPWL
 * of nets crossing the group boundary, and a move is committed only when that
 * affected-net cost improves.
 */
class GriddedRowLocationOptimizer {
 public:
  explicit GriddedRowLocationOptimizer(Circuit* circuit) : circuit_(circuit) {}

  /** Optimize all stripes and return quality and activity statistics. */
  GriddedRowLocationResult Optimize(std::vector<StripeColumn>* columns);

 private:
  struct RowGroup {
    Stripe* stripe = nullptr;
    int first_row = 0;
    int last_row = 0;
  };

  std::vector<RowGroup> CollectRowGroups(Stripe* stripe) const;
  bool OptimizeGroup(const RowGroup& group);

  Circuit* circuit_ = nullptr;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_GRIDDED_ROW_LOCATION_OPTIMIZER_H_
