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
/**
 * When a placement is settled enough to change its netlist.
 *
 * A topology change is worth measuring only against a placement that has
 * stopped moving. Early iterations still have cells travelling far enough that
 * the slack attributed to a delay line describes where things happen to be
 * rather than where they will end up, and a size chosen from that is a size
 * chosen from noise.
 *
 * So eligibility is read off the trajectory rather than configured as an
 * answer: the upper bound must be physical and accepted, the warm-up must have
 * passed, and the accepted HPWL must have held still -- within a configured
 * fraction -- across three consecutive accepted physical iterations. The three
 * iterations that satisfied it are reported, so the choice can be checked
 * against the log instead of taken on trust.
 *
 * This is kept apart from the sizing policy and from Dali itself because it is
 * a statement about placement, not about delay lines: it would be the same rule
 * whatever the netlist change turned out to be.
 */
#ifndef DALI_TIMING_TOPOLOGY_CHECKPOINT_COORDINATOR_H_
#define DALI_TIMING_TOPOLOGY_CHECKPOINT_COORDINATOR_H_

#include <cstddef>
#include <string>
#include <vector>

namespace dali {

/** One accepted physical upper bound, as the placer reported it. */
struct CheckpointSample {
  int iteration = 0;
  double accepted_hpwl = 0.0;
  /** Change from the previous accepted physical upper bound, as a fraction. */
  double hpwl_change_fraction = 0.0;
};

struct CheckpointEligibilityConfig {
  /** First iteration that may be considered at all. */
  int warmup_iteration = 0;
  /** How still the accepted HPWL must be, as an absolute fraction. */
  double stability_fraction = 0.01;
  /** Consecutive accepted physical iterations that must satisfy it. */
  int stability_window = 3;
};

/**
 * Tracks accepted physical upper bounds and reports when one is eligible.
 *
 * Exactly one decision attempt is permitted per run, whatever its outcome. A
 * second attempt would be a second checkpoint, which this milestone does not
 * have, and allowing a retry after a decline would turn one decision into a
 * search.
 */
class TopologyCheckpointCoordinator {
 public:
  explicit TopologyCheckpointCoordinator(CheckpointEligibilityConfig config)
      : config_(config) {}

  /**
   * Records one accepted physical upper bound and says whether it is eligible.
   *
   * A non-finite sample is not merely ineligible: it breaks the window, because
   * a value that cannot be compared cannot be shown to have held still.
   */
  bool Offer(const CheckpointSample &sample);

  /** Marks the single permitted decision attempt as used. */
  void MarkAttempted() { attempted_ = true; }
  bool HasAttempted() const { return attempted_; }

  /** The three samples that satisfied the window, oldest first. */
  const std::vector<CheckpointSample> &QualifyingSamples() const {
    return window_;
  }

  /** Why the most recent offer was not eligible, for the run log. */
  const std::string &LastReason() const { return last_reason_; }

 private:
  CheckpointEligibilityConfig config_;
  std::vector<CheckpointSample> window_;
  bool attempted_ = false;
  std::string last_reason_;
};

} // namespace dali

#endif // DALI_TIMING_TOPOLOGY_CHECKPOINT_COORDINATOR_H_
