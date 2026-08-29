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
#ifndef DALI_CIRCUIT_COMPONENT_PLACEMENT_STATE_H_
#define DALI_CIRCUIT_COMPONENT_PLACEMENT_STATE_H_

#include <vector>

#include "dali/circuit/component.h"

namespace dali {

/**
 * The placement state of every component: location, orientation, status.
 *
 * Taken before the timing-feedback passes and restored before each re-run of
 * global placement, so that every pass starts from the same state and differs
 * from the others only in its net weights. Without it each pass inherited the
 * previous pass's legalized coordinates and orientations. The seeded
 * initializer then produced a different start (after-init HPWL 33013.5 um in
 * pass 0, 32995.1 um in pass 1 on asymmetric_fork_join_io), and pass 1, with
 * weights identical to pass 0, turned 0 measured violations into 1 (-16 ps):
 * the loop measured one placement and shipped another.
 *
 * Only placement state is covered. A pass that adds or removes components
 * cannot be rolled back this way; Restore refuses rather than misassign.
 */
class ComponentPlacementState {
 public:
  explicit ComponentPlacementState(const std::vector<Component> &components);

  /** Put every component back; false, changing nothing, if the count
   *  differs. */
  bool Restore(std::vector<Component> &components) const;

 private:
  struct State {
    double llx;
    double lly;
    ComponentOrient orient;
    PlaceStatus status;
  };
  std::vector<State> states_;
};

}  // namespace dali

#endif  // DALI_CIRCUIT_COMPONENT_PLACEMENT_STATE_H_
