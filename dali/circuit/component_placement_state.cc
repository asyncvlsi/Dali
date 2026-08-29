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
#include "dali/circuit/component_placement_state.h"

namespace dali {

ComponentPlacementState::ComponentPlacementState(
    const std::vector<Component> &components) {
  states_.reserve(components.size());
  for (const Component &component : components) {
    states_.push_back({component.LLX(), component.LLY(), component.Orient(),
                       component.Status()});
  }
}

bool ComponentPlacementState::Restore(
    std::vector<Component> &components) const {
  if (components.size() != states_.size()) return false;
  for (std::size_t i = 0; i < components.size(); ++i) {
    components[i].SetOrient(states_[i].orient);
    components[i].SetLoc(states_[i].llx, states_[i].lly);
    components[i].SetPlacementStatus(states_[i].status);
  }
  return true;
}

}  // namespace dali
