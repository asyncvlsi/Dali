/*******************************************************************************
 *
 * Isolated same-placement gain measurement for delay-line timing feedback.
 *
 * The controller used to estimate how much a row of separation buys by dividing
 * a slack difference by a separation difference, with the two samples taken from
 * different global-placement iterations. Everything else in the design moves
 * between those iterations, so the quotient answered a question nobody asked. On
 * the width-1 fixture a spreading step moved dl0's slack from +35.83 to -90.09
 * ps across a one-row change, and the estimator reported 125.9 ps/row against a
 * true value near 10 -- then computed a zero step from its own inflated number
 * and stopped moving.
 *
 * A probe replaces the quotient with a measurement. Both samples come from the
 * same accepted placement: freeze the coordinates, measure, apply one line's
 * proposed shape and nothing else, refresh placement-synchronized timing,
 * measure again, and put the coordinates back exactly. Nothing between the two
 * samples is allowed to move except the line being probed.
 *
 * The two halves are separate on purpose. EvaluateIsolatedGain is a pure
 * function over what a probe observed, so every acceptance and refusal rule can
 * be tested without a circuit; RunIsolatedGainProbes sequences the transaction
 * through injected hooks, so restoration and ordering can be tested without a
 * placement flow. Dali supplies the real hooks.
 *
 *******************************************************************************/

#ifndef DALI_TIMING_DELAY_LINE_GAIN_PROBE_H_
#define DALI_TIMING_DELAY_LINE_GAIN_PROBE_H_

#include <functional>
#include <string>
#include <vector>

namespace dali {

/**
 * One component's placement, in the only detail a probe has to put back.
 *
 * Status travels with the coordinates because a shape may only move movable
 * cells, and a restore that returned position but not placement status would
 * leave the circuit subtly different from the baseline it claims to have
 * restored.
 */
struct ComponentPlacement {
  int component_id = -1;
  double llx = 0.0;
  double lly = 0.0;
  int status = 0;
};

/**
 * Order-independent digest of a set of component placements.
 *
 * Bit patterns rather than rounded values: a restore that lands within a
 * tolerance of the baseline is not the baseline, and a probe that reported it
 * as one would be hiding exactly the drift this exists to catch. Combining
 * per-component hashes by addition keeps the digest independent of the order
 * components are snapshotted in.
 */
unsigned long long PlacementDigest(
    const std::vector<ComponentPlacement> &placements);

/** Why a probe's gain may or may not be used. */
enum class ProbeOutcome {
  kAccepted,
  kRefusedNoSeparationChange,
  kRefusedAmbiguousAttribution,
  kRefusedMissingAttribution,
  kRefusedIdentityChanged,
  kRefusedNonFiniteTiming,
  kRefusedNonPositiveGain,
  kRefusedRestorationMismatch,
  kRefusedAreaBound,
  kRefusedTimingRefreshFailed,
};

const char *ToString(ProbeOutcome outcome);

/** Everything the acceptance rules read from one same-placement probe. */
struct IsolatedGainInput {
  int baseline_separation = -1;
  int trial_separation = -1;
  double baseline_slack = 0.0;
  double trial_slack = 0.0;
  std::string baseline_identity;
  std::string trial_identity;
  int baseline_attributed_constraints = 0;
  int trial_attributed_constraints = 0;
  bool attribution_unique = true;
  bool within_area_bound = true;
  bool timing_refresh_succeeded = true;
  unsigned long long baseline_digest = 0;
  unsigned long long restored_digest = 0;
};

struct IsolatedGainResult {
  ProbeOutcome outcome = ProbeOutcome::kAccepted;
  double gain_ps_per_row = 0.0;

  bool usable() const { return outcome == ProbeOutcome::kAccepted; }
};

/**
 * Decide whether a probe measured a gain, and what it was.
 *
 * Refusals are typed rather than folded into a zero, because a caller that
 * cannot distinguish "this line gains nothing" from "this measurement is not
 * trustworthy" will eventually treat the second as the first. Nothing here
 * smooths, averages or clamps: a contaminated sample is refused, not repaired,
 * and the caller is expected to leave the separation alone rather than fall
 * back on a stale number from a placement that no longer exists.
 */
IsolatedGainResult EvaluateIsolatedGain(const IsolatedGainInput &input);

/** What the caller knows about a site before its shape is tried. */
struct ProbeSiteRequest {
  std::string site;
  std::vector<int> constraint_ids;
  int baseline_separation = -1;
  int trial_separation = -1;
  double baseline_slack = 0.0;
  std::string baseline_identity;
  int baseline_attributed_constraints = 0;
  bool baseline_attribution_unique = true;
  bool within_area_bound = true;
};

/** What one timing measurement says about a site. */
struct ProbeMeasurement {
  double slack = 0.0;
  std::string identity;
  int attributed_constraints = 0;
  bool attribution_unique = true;
};

/**
 * The circuit-facing operations a probe needs, injected so the transaction can
 * be tested without a placement flow.
 *
 * `apply_shape` must move only the named line, `refresh_timing` must be the
 * placement-synchronized refresh and must not run a placement solve, and
 * `restore` must put back exactly what `snapshot` returned.
 */
struct ProbeHooks {
  std::function<std::vector<ComponentPlacement>(const std::string &)> snapshot;
  std::function<bool(const std::string &, int)> apply_shape;
  std::function<bool()> refresh_timing;
  std::function<ProbeMeasurement(const std::string &)> measure;
  std::function<void(const std::vector<ComponentPlacement> &)> restore;
};

/** One site's isolated measurement, and the evidence it rests on. */
struct IsolatedGainProbe {
  std::string site;
  std::vector<int> constraint_ids;
  std::string binding_identity;
  int baseline_separation = -1;
  int trial_separation = -1;
  double baseline_slack = 0.0;
  double trial_slack = 0.0;
  double gain_ps_per_row = 0.0;
  unsigned long long baseline_digest = 0;
  unsigned long long restored_digest = 0;
  ProbeOutcome outcome = ProbeOutcome::kAccepted;
};

/**
 * Probe every site against one baseline placement, in site-name order.
 *
 * Each site is restored before the next one is tried, so site B measures the
 * same placement site A did rather than site A's trial geometry. Restoration
 * happens on every path out of a probe, including a failed shape application
 * and a failed timing refresh, because the alternative is leaving a trial shape
 * in the circuit for global placement to resume from.
 *
 * Deterministic order is part of the contract, not an implementation detail: a
 * probe order that depended on registration order would make the accepted
 * separations depend on the order the recipe happened to name its lines.
 */
std::vector<IsolatedGainProbe> RunIsolatedGainProbes(
    std::vector<ProbeSiteRequest> requests, const ProbeHooks &hooks);

} // namespace dali

#endif // DALI_TIMING_DELAY_LINE_GAIN_PROBE_H_
