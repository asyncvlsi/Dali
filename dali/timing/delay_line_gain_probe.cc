/*******************************************************************************
 *
 * Isolated same-placement gain measurement for delay-line timing feedback.
 *
 *******************************************************************************/

#include "dali/timing/delay_line_gain_probe.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace dali {

namespace {

constexpr unsigned long long kFnvOffsetBasis = 1469598103934665603ull;
constexpr unsigned long long kFnvPrime = 1099511628211ull;

unsigned long long HashBytes(unsigned long long hash, const void *data,
                             std::size_t size) {
  const unsigned char *bytes = static_cast<const unsigned char *>(data);
  for (std::size_t index = 0; index < size; ++index) {
    hash ^= static_cast<unsigned long long>(bytes[index]);
    hash *= kFnvPrime;
  }
  return hash;
}

unsigned long long HashDouble(unsigned long long hash, double value) {
  // Normalise the one bit pattern that compares equal to itself but differs
  // bitwise, so a restore to -0.0 from 0.0 is not reported as a mismatch.
  if (value == 0.0) value = 0.0;
  return HashBytes(hash, &value, sizeof(value));
}

} // namespace

unsigned long long PlacementDigest(
    const std::vector<ComponentPlacement> &placements) {
  unsigned long long digest = 0;
  for (const ComponentPlacement &placement : placements) {
    unsigned long long entry = kFnvOffsetBasis;
    entry = HashBytes(entry, &placement.component_id,
                      sizeof(placement.component_id));
    entry = HashDouble(entry, placement.llx);
    entry = HashDouble(entry, placement.lly);
    entry = HashBytes(entry, &placement.status, sizeof(placement.status));
    digest += entry;
  }
  return digest;
}

const char *ToString(ProbeOutcome outcome) {
  switch (outcome) {
    case ProbeOutcome::kAccepted:
      return "accepted";
    case ProbeOutcome::kRefusedNoSeparationChange:
      return "refused_no_separation_change";
    case ProbeOutcome::kRefusedAmbiguousAttribution:
      return "refused_ambiguous_attribution";
    case ProbeOutcome::kRefusedMissingAttribution:
      return "refused_missing_attribution";
    case ProbeOutcome::kRefusedIdentityChanged:
      return "refused_identity_changed";
    case ProbeOutcome::kRefusedNonFiniteTiming:
      return "refused_non_finite_timing";
    case ProbeOutcome::kRefusedNonPositiveGain:
      return "refused_non_positive_gain";
    case ProbeOutcome::kRefusedRestorationMismatch:
      return "refused_restoration_mismatch";
    case ProbeOutcome::kRefusedAreaBound:
      return "refused_area_bound";
    case ProbeOutcome::kRefusedTimingRefreshFailed:
      return "refused_timing_refresh_failed";
  }
  return "refused_unknown";
}

IsolatedGainResult EvaluateIsolatedGain(const IsolatedGainInput &input) {
  IsolatedGainResult result;
  const auto refuse = [&result](ProbeOutcome outcome) {
    result.outcome = outcome;
    result.gain_ps_per_row = 0.0;
    return result;
  };

  // Restoration is checked first. If the coordinates did not come back, no
  // other conclusion from this probe is worth drawing, and the caller needs to
  // hear about the corruption rather than about a gain.
  if (input.baseline_digest != input.restored_digest) {
    return refuse(ProbeOutcome::kRefusedRestorationMismatch);
  }
  if (!input.timing_refresh_succeeded) {
    return refuse(ProbeOutcome::kRefusedTimingRefreshFailed);
  }
  if (!input.within_area_bound) {
    return refuse(ProbeOutcome::kRefusedAreaBound);
  }
  // A separation the area clamp reduced back to where it started is a no-op,
  // not a measurement of zero gain; dividing by that zero is how a clamp turns
  // into an infinity.
  if (input.trial_separation == input.baseline_separation) {
    return refuse(ProbeOutcome::kRefusedNoSeparationChange);
  }
  if (input.baseline_attributed_constraints == 0 ||
      input.trial_attributed_constraints == 0) {
    return refuse(ProbeOutcome::kRefusedMissingAttribution);
  }
  if (!input.attribution_unique) {
    return refuse(ProbeOutcome::kRefusedAmbiguousAttribution);
  }
  // The binding path has to be the same path on both sides. Two equally tight
  // but different constraints would produce a difference that is real and
  // means nothing about this line's separation.
  if (input.baseline_identity.empty() || input.trial_identity.empty() ||
      input.baseline_identity != input.trial_identity) {
    return refuse(ProbeOutcome::kRefusedIdentityChanged);
  }
  if (!std::isfinite(input.baseline_slack) ||
      !std::isfinite(input.trial_slack)) {
    return refuse(ProbeOutcome::kRefusedNonFiniteTiming);
  }

  const double gain = (input.trial_slack - input.baseline_slack) /
                      static_cast<double>(input.trial_separation -
                                          input.baseline_separation);
  if (!std::isfinite(gain) || gain <= 0.0) {
    return refuse(ProbeOutcome::kRefusedNonPositiveGain);
  }

  result.outcome = ProbeOutcome::kAccepted;
  result.gain_ps_per_row = gain;
  return result;
}

std::vector<IsolatedGainProbe> RunIsolatedGainProbes(
    std::vector<ProbeSiteRequest> requests, const ProbeHooks &hooks) {
  std::sort(requests.begin(), requests.end(),
            [](const ProbeSiteRequest &left, const ProbeSiteRequest &right) {
              return left.site < right.site;
            });

  std::vector<IsolatedGainProbe> probes;
  probes.reserve(requests.size());
  for (const ProbeSiteRequest &request : requests) {
    IsolatedGainProbe probe;
    probe.site = request.site;
    probe.constraint_ids = request.constraint_ids;
    probe.binding_identity = request.baseline_identity;
    probe.baseline_separation = request.baseline_separation;
    probe.trial_separation = request.trial_separation;
    probe.baseline_slack = request.baseline_slack;

    const std::vector<ComponentPlacement> baseline = hooks.snapshot(
        request.site);
    probe.baseline_digest = PlacementDigest(baseline);

    IsolatedGainInput input;
    input.baseline_separation = request.baseline_separation;
    input.trial_separation = request.trial_separation;
    input.baseline_slack = request.baseline_slack;
    input.baseline_identity = request.baseline_identity;
    input.baseline_attributed_constraints =
        request.baseline_attributed_constraints;
    input.within_area_bound = request.within_area_bound;
    input.baseline_digest = probe.baseline_digest;

    const bool shape_applied =
        hooks.apply_shape(request.site, request.trial_separation);
    const bool refreshed = shape_applied && hooks.refresh_timing();
    if (refreshed) {
      const ProbeMeasurement measurement = hooks.measure(request.site);
      probe.trial_slack = measurement.slack;
      input.trial_slack = measurement.slack;
      input.trial_identity = measurement.identity;
      input.trial_attributed_constraints = measurement.attributed_constraints;
      input.attribution_unique = request.baseline_attribution_unique &&
                                 measurement.attribution_unique;
    } else {
      input.timing_refresh_succeeded = false;
    }

    // Unconditional: a probe that failed to apply its shape or to refresh
    // timing has still moved coordinates, or may have, and leaving them for the
    // next site to inherit is the failure this transaction exists to prevent.
    hooks.restore(baseline);
    input.restored_digest = PlacementDigest(hooks.snapshot(request.site));
    probe.restored_digest = input.restored_digest;

    const IsolatedGainResult result = EvaluateIsolatedGain(input);
    probe.outcome = result.outcome;
    probe.gain_ps_per_row = result.gain_ps_per_row;
    probes.push_back(std::move(probe));
  }
  return probes;
}

} // namespace dali
