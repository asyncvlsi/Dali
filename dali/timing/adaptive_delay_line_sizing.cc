/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 ******************************************************************************/
#include "dali/timing/adaptive_delay_line_sizing.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace dali {
namespace {

AdaptiveSizingDecision Invalid(const std::string &reason) {
  AdaptiveSizingDecision decision;
  decision.outcome = AdaptiveSizingOutcome::kInvalid;
  decision.reason = reason;
  return decision;
}

int ConsecutiveNonpositiveGains(const AdaptiveDelayLineHistory &history) {
  int count = 0;
  for (std::size_t index = history.samples.size(); index > 1; --index) {
    const AdaptiveDelayLineSample &current = history.samples[index - 1];
    const AdaptiveDelayLineSample &previous = history.samples[index - 2];
    if (current.pairs <= previous.pairs ||
        current.binding_identity != previous.binding_identity) {
      break;
    }
    const double gain = (current.binding_slack_ps - previous.binding_slack_ps) /
                        static_cast<double>(current.pairs - previous.pairs);
    if (std::isfinite(gain) && gain > 0.0)
      break;
    ++count;
  }
  return count;
}

} // namespace

AdaptiveSizingDecision DecideAdaptiveDelayLineSizing(
    const std::vector<AdaptiveDelayLineHistory> &histories,
    const AdaptiveDelayLineLimits &limits) {
  if (!std::isfinite(limits.margin_ps) || limits.probe_pairs < 1 ||
      limits.max_step_pairs < 1 || limits.max_added_pairs < 1 ||
      limits.max_pairs < 1 || limits.max_nonpositive_probes < 0) {
    return Invalid("adaptive sizing limits are not configured");
  }
  if (limits.probe_pairs > limits.max_step_pairs) {
    return Invalid("the calibration probe exceeds the per-epoch step bound");
  }
  if (histories.empty())
    return Invalid("no delay-line sites were measured");

  std::set<std::string> names;
  AdaptiveSizingDecision decision;
  decision.outcome = AdaptiveSizingOutcome::kClosed;
  int total_added_pairs = 0;

  for (const AdaptiveDelayLineHistory &history : histories) {
    if (history.site.empty() || history.initial_pairs < 1 ||
        !names.insert(history.site).second || history.samples.empty()) {
      return Invalid("adaptive sizing histories are malformed or duplicated");
    }
    const AdaptiveDelayLineSample &current = history.samples.back();
    if (current.site != history.site || current.pairs < history.initial_pairs ||
        !std::isfinite(current.binding_slack_ps) || !current.attributed ||
        current.binding_identity.empty()) {
      return Invalid("site '" + history.site +
                     "' has an untrustworthy current measurement");
    }
    if (current.binding_slack_ps >= limits.margin_ps)
      continue;

    int step = limits.probe_pairs;
    bool probe = true;
    double gain = 0.0;
    std::string reason = "initial online calibration probe";
    if (history.samples.size() >= 2) {
      const AdaptiveDelayLineSample &previous =
          history.samples[history.samples.size() - 2];
      if (current.pairs < previous.pairs) {
        return Invalid("site '" + history.site +
                       "' lost pairs between measurements");
      }
      if (current.pairs == previous.pairs) {
        if (previous.binding_slack_ps < limits.margin_ps) {
          return Invalid("site '" + history.site +
                         "' did not grow after a violating measurement");
        }
        reason =
            "previously closed site regressed; recalibrating with a small probe";
      } else if (current.binding_identity != previous.binding_identity) {
        reason = "binding path changed; recalibrating with a small probe";
      } else {
        gain = (current.binding_slack_ps - previous.binding_slack_ps) /
               static_cast<double>(current.pairs - previous.pairs);
        if (std::isfinite(gain) && gain > 0.0) {
          const double deficit = limits.margin_ps - current.binding_slack_ps;
          step = static_cast<int>(std::ceil(deficit / gain));
          step = std::max(1, std::min(step, limits.max_step_pairs));
          probe = false;
          reason = "bounded prediction from the latest measured response";
        } else {
          const int nonpositive = ConsecutiveNonpositiveGains(history);
          if (nonpositive > limits.max_nonpositive_probes) {
            return Invalid("site '" + history.site +
                           "' made no positive timing progress");
          }
          reason = "non-positive response; retrying one bounded probe";
        }
      }
    }
    const int already_added = current.pairs - history.initial_pairs;
    if (already_added + step > limits.max_added_pairs ||
        current.pairs + step > limits.max_pairs) {
      return Invalid("site '" + history.site +
                     "' cannot grow within its configured bounds");
    }

    AdaptiveSiteDecision site;
    site.site = history.site;
    site.current_pairs = current.pairs;
    site.requested_pairs = current.pairs + step;
    site.binding_slack_ps = current.binding_slack_ps;
    site.measured_gain_ps_per_pair = gain;
    site.is_probe = probe;
    site.reason = reason;
    decision.sites.push_back(site);

    TopologyChangeRequest request;
    request.site = site.site;
    request.current_pairs = site.current_pairs;
    request.requested_pairs = site.requested_pairs;
    request.expected_added_components = 2 * step;
    request.expected_added_nets = 2 * step;
    decision.batch.requests.push_back(std::move(request));
    total_added_pairs += step;
  }

  if (decision.sites.empty()) {
    decision.reason = "every attributed delay-line site meets the margin";
    return decision;
  }
  if (2 * total_added_pairs > static_cast<int>(limits.component_headroom) ||
      2 * total_added_pairs > static_cast<int>(limits.net_headroom)) {
    return Invalid("the adaptive batch exceeds reserved topology headroom");
  }
  std::sort(
      decision.sites.begin(), decision.sites.end(),
      [](const AdaptiveSiteDecision &left, const AdaptiveSiteDecision &right) {
        return left.site < right.site;
      });
  std::sort(decision.batch.requests.begin(), decision.batch.requests.end(),
            [](const TopologyChangeRequest &left,
               const TopologyChangeRequest &right) {
              return left.site < right.site;
            });
  decision.outcome = AdaptiveSizingOutcome::kRequest;
  decision.reason = "one or more sites require another measured insertion";
  return decision;
}

} // namespace dali
