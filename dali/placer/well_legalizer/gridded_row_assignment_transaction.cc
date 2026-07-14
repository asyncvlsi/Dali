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
#include "dali/placer/well_legalizer/gridded_row_assignment_transaction.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "dali/circuit/circuit.h"
#include "dali/common/logging.h"

namespace dali {

GriddedRowAssignmentTransaction::GriddedRowAssignmentTransaction(
    Circuit* circuit, const std::vector<GriddedRow*>& rows)
    : circuit_(circuit) {
  DaliExpects(circuit_ != nullptr,
              "Gridded row assignment requires a valid circuit");
  row_snapshots_.reserve(rows.size());
  for (GriddedRow* row : rows) {
    DaliExpects(row != nullptr,
                "Gridded row assignment cannot capture a null row");
    RowSnapshot snapshot;
    snapshot.row = row;
    snapshot.component_order = row->Components();
    snapshot.component_lx.reserve(row->Components().size());
    snapshot.component_ly.reserve(row->Components().size());
    snapshot.component_orient.reserve(row->Components().size());
    for (Component* component : row->Components()) {
      snapshot.component_lx.push_back(component->LLX());
      snapshot.component_ly.push_back(component->LLY());
      snapshot.component_orient.push_back(component->Orient());
      affected_net_ids_.insert(affected_net_ids_.end(),
                               component->NetList().begin(),
                               component->NetList().end());
    }
    row_snapshots_.push_back(std::move(snapshot));
  }
  std::sort(affected_net_ids_.begin(), affected_net_ids_.end());
  affected_net_ids_.erase(
      std::unique(affected_net_ids_.begin(), affected_net_ids_.end()),
      affected_net_ids_.end());
  hpwl_before_ = AffectedNetHpwl();
}

bool GriddedRowAssignmentTransaction::ImprovesHpwl(
    double minimum_improvement) const {
  DaliExpects(minimum_improvement >= 0,
              "Minimum HPWL improvement cannot be negative");
  return HpwlImprovement() > minimum_improvement;
}

double GriddedRowAssignmentTransaction::HpwlImprovement() const {
  return hpwl_before_ - AffectedNetHpwl();
}

void GriddedRowAssignmentTransaction::Restore() const {
  for (const RowSnapshot& snapshot : row_snapshots_) {
    snapshot.row->Components() = snapshot.component_order;
    for (std::size_t i = 0; i < snapshot.component_order.size(); ++i) {
      snapshot.component_order[i]->SetLLX(snapshot.component_lx[i]);
      snapshot.component_order[i]->SetLLY(snapshot.component_ly[i]);
      snapshot.component_order[i]->SetOrient(snapshot.component_orient[i]);
    }
  }
}

double GriddedRowAssignmentTransaction::AffectedNetHpwl() const {
  double hpwl = 0;
  for (int net_id : affected_net_ids_) {
    hpwl += circuit_->NetWeightedHPWL(net_id);
  }
  return hpwl;
}

}  // namespace dali
