/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 ******************************************************************************/
/** @file Online delay-line sizing from measurements of the current run. */
#ifndef DALI_TIMING_ADAPTIVE_DELAY_LINE_SIZING_H_
#define DALI_TIMING_ADAPTIVE_DELAY_LINE_SIZING_H_

#include <cstddef>
#include <string>
#include <vector>

#include "dali/placer/global_placer/placement_checkpoint.h"

namespace dali {

/** One final-legal timing sample for one registered delay-line site. */
struct AdaptiveDelayLineSample {
  std::string site;
  int pairs = 0;
  double binding_slack_ps = 0.0;
  bool attributed = false;
  std::string binding_identity;
};

/** Ordered samples for one site, all from the same placement operation. */
struct AdaptiveDelayLineHistory {
  std::string site;
  int initial_pairs = 0;
  std::vector<AdaptiveDelayLineSample> samples;
};

/** Recipe-owned safety limits for the bounded online controller. */
struct AdaptiveDelayLineLimits {
  double margin_ps = 0.0;
  int probe_pairs = 0;
  int max_step_pairs = 0;
  int max_added_pairs = 0;
  int max_pairs = 0;
  int max_nonpositive_probes = 0;
  std::size_t component_headroom = 0;
  std::size_t net_headroom = 0;
};

enum class AdaptiveSizingOutcome { kRequest, kClosed, kInvalid };

/** Why one site's next step was selected. */
struct AdaptiveSiteDecision {
  std::string site;
  int current_pairs = 0;
  int requested_pairs = 0;
  double binding_slack_ps = 0.0;
  double measured_gain_ps_per_pair = 0.0;
  bool is_probe = false;
  std::string reason;
};

/** One atomic batch, or a terminal outcome. */
struct AdaptiveSizingDecision {
  AdaptiveSizingOutcome outcome = AdaptiveSizingOutcome::kInvalid;
  std::vector<AdaptiveSiteDecision> sites;
  TopologyChangeBatch batch;
  std::string reason;
};

/** Decide the next bounded insertion from measurements already taken. */
AdaptiveSizingDecision DecideAdaptiveDelayLineSizing(
    const std::vector<AdaptiveDelayLineHistory> &histories,
    const AdaptiveDelayLineLimits &limits);

} // namespace dali

#endif // DALI_TIMING_ADAPTIVE_DELAY_LINE_SIZING_H_
