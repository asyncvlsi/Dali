/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *******************************************************************************/
#include "dali/placer/global_placer/placement_checkpoint.h"

#include "dali/common/logging.h"
#include "dali/placer/global_placer/global_placer.h"

#include "dali/circuit/topology_delta.h"

namespace dali {

ScopedCheckpointObserver::ScopedCheckpointObserver(
    GlobalPlacer &placer, PlacementCheckpointObserver *observer)
    : placer_(placer) {
  placer_.SetCheckpointObserver(observer);
}

ScopedCheckpointObserver::~ScopedCheckpointObserver() {
  placer_.SetCheckpointObserver(nullptr);
}

CheckpointDecision RecordingCheckpointObserver::Observe(
    const PlacementCheckpoint &checkpoint) {
  checkpoints_.push_back(checkpoint);
  LOG(debug) << "PLACEMENT_CHECKPOINT iteration " << checkpoint.iteration
             << " accepted_hpwl " << checkpoint.accepted_hpwl
             << " hpwl_change_fraction " << checkpoint.hpwl_change_fraction
             << " physical_upper_bound_count "
             << checkpoint.physical_upper_bound_count << "\n";
  return CheckpointDecision::kContinue;
}

} // namespace dali
