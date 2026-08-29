/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 *******************************************************************************/

/** @file Dali-owned deterministic candidate policies for timing-driven flow. */
#ifndef DALI_TIMING_TIMING_DRIVEN_CANDIDATE_POLICY_H_
#define DALI_TIMING_TIMING_DRIVEN_CANDIDATE_POLICY_H_

#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

#include "dali/timing/timing_driven_placement_controller.h"

namespace dali {

/**
 * Replays a configured candidate sequence and owns its iteration cursor.
 *
 * The policy receives no lifecycle configuration. It cannot inspect paths,
 * technology settings, or host objects while choosing the next candidate.
 */
class DeterministicTimingDrivenCandidatePolicy final
    : public TimingDrivenCandidatePolicy {
public:
  explicit DeterministicTimingDrivenCandidatePolicy(
      std::vector<TimingDrivenPlacementCandidate> candidates)
      : candidates_(std::move(candidates)) {}

  std::optional<TimingDrivenPlacementCandidate> NextCandidate(
      TimingDrivenPlacementCandidate current_candidate,
      std::optional<TimingDrivenPlacementMeasurement> current_measurement,
      std::vector<TimingDrivenTrialRecord> history) override;

  /** Return how many configured candidates have already been proposed. */
  std::size_t proposed_count() const { return next_candidate_; }

private:
  std::vector<TimingDrivenPlacementCandidate> candidates_;
  std::size_t next_candidate_ = 0;
};

} // namespace dali

#endif // DALI_TIMING_TIMING_DRIVEN_CANDIDATE_POLICY_H_
