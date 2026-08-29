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
#include "dali/timing/topology_checkpoint_coordinator.h"

#include <cmath>

namespace dali {

bool TopologyCheckpointCoordinator::Offer(const CheckpointSample &sample) {
  if (attempted_) {
    last_reason_ = "a decision has already been attempted this run";
    return false;
  }
  if (config_.stability_window <= 0) {
    last_reason_ = "the stability window is not configured";
    window_.clear();
    return false;
  }

  // A sample that cannot be compared cannot contribute to a claim that the
  // placement held still, and it invalidates the samples either side of it:
  // consecutive means consecutive.
  if (!std::isfinite(sample.accepted_hpwl) ||
      !std::isfinite(sample.hpwl_change_fraction)) {
    window_.clear();
    last_reason_ = "accepted HPWL or its change fraction is not finite";
    return false;
  }

  // Before warm-up the placement is still travelling, so nothing is recorded:
  // a window that spanned the warm-up boundary would be counting iterations
  // that were excluded for a reason.
  if (sample.iteration < config_.warmup_iteration) {
    window_.clear();
    last_reason_ = "iteration " + std::to_string(sample.iteration) +
                   " is before the configured warm-up of " +
                   std::to_string(config_.warmup_iteration);
    return false;
  }

  if (std::fabs(sample.hpwl_change_fraction) > config_.stability_fraction) {
    window_.clear();
    last_reason_ = "iteration " + std::to_string(sample.iteration) +
                   " moved the accepted HPWL by " +
                   std::to_string(sample.hpwl_change_fraction) +
                   ", beyond the configured " +
                   std::to_string(config_.stability_fraction);
    return false;
  }

  window_.push_back(sample);
  if (static_cast<int>(window_.size()) > config_.stability_window) {
    window_.erase(window_.begin());
  }
  if (static_cast<int>(window_.size()) < config_.stability_window) {
    last_reason_ = "only " + std::to_string(window_.size()) + " of " +
                   std::to_string(config_.stability_window) +
                   " consecutive settled iterations so far";
    return false;
  }

  last_reason_.clear();
  return true;
}

} // namespace dali
