/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 *******************************************************************************/

#include "dali/timing/timing_driven_candidate_policy.h"

#include <utility>

namespace dali {

std::optional<TimingDrivenPlacementCandidate>
DeterministicTimingDrivenCandidatePolicy::NextCandidate(
    TimingDrivenPlacementCandidate current_candidate,
    std::optional<TimingDrivenPlacementMeasurement> current_measurement,
    std::vector<TimingDrivenTrialRecord> history) {
  static_cast<void>(current_candidate);
  static_cast<void>(current_measurement);
  static_cast<void>(history);
  if (next_candidate_ >= candidates_.size()) {
    return std::nullopt;
  }
  return candidates_[next_candidate_++];
}

} // namespace dali
