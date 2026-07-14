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
#ifndef DALI_PLACER_WELL_LEGALIZER_GRIDDED_ROW_ASSIGNMENT_TRANSACTION_H_
#define DALI_PLACER_WELL_LEGALIZER_GRIDDED_ROW_ASSIGNMENT_TRANSACTION_H_

#include <vector>

#include "dali/placer/well_legalizer/gridded_row.h"

namespace dali {

class Circuit;

/**
 * Snapshot and evaluate one trial assignment across gridded rows.
 *
 * Cross-row detailed-placement operations temporarily change component order,
 * location, and orientation in several rows. This transaction records that
 * state together with the unchanged union of incident nets. Callers may then
 * evaluate exact weighted HPWL and restore the original rows when a trial is
 * not beneficial.
 */
class GriddedRowAssignmentTransaction {
 public:
  /** Capture the supplied rows and their exact affected-net HPWL. */
  GriddedRowAssignmentTransaction(Circuit* circuit,
                                  const std::vector<GriddedRow*>& rows);

  /** Return true when the trial reduces affected-net HPWL by the threshold. */
  bool ImprovesHpwl(double minimum_improvement) const;

  /** Return exact affected-net HPWL before the trial minus its current value.
   */
  double HpwlImprovement() const;

  /** Restore component membership, order, location, and orientation. */
  void Restore() const;

 private:
  struct RowSnapshot {
    GriddedRow* row = nullptr;
    std::vector<Component*> component_order;
    std::vector<double> component_lx;
    std::vector<double> component_ly;
    std::vector<ComponentOrient> component_orient;
  };

  /** Compute exact weighted HPWL over the captured affected-net set. */
  double AffectedNetHpwl() const;

  Circuit* circuit_ = nullptr;
  std::vector<RowSnapshot> row_snapshots_;
  std::vector<int> affected_net_ids_;
  double hpwl_before_ = 0;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_GRIDDED_ROW_ASSIGNMENT_TRANSACTION_H_
