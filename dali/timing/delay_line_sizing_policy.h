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
 * Choosing which delay-line site to grow, and by how much.
 *
 * This is the whole of the decision. It takes measurements and limits by value
 * and returns a request or a refusal, so the rule can be argued with and tested
 * without a placer, a netlist, or a host attached to it. Nothing here reads the
 * circuit or calls anything: the placer measures, this decides, and the host
 * applies.
 *
 * The decision is deliberately a single shot. One checkpoint, one site, one
 * request, no revisiting -- so there is no loop that could oscillate and no
 * search that could be tuned after the fact into agreeing with its own result.
 *
 * The distinction between declining and refusing is load-bearing. Declining
 * says the design needs nothing and is an ordinary result. Refusing says the
 * evidence cannot support any decision, and must stop the run rather than be
 * rounded into one -- a missing measurement is not the same as a measurement of
 * zero, and treating it as one is how a sizing rule comes to report success on
 * a design it never looked at.
 */
#ifndef DALI_TIMING_DELAY_LINE_SIZING_POLICY_H_
#define DALI_TIMING_DELAY_LINE_SIZING_POLICY_H_

#include <cstddef>
#include <string>
#include <vector>

namespace dali {

/** What placement measured about one registered site at the checkpoint. */
struct DelayLineSiteMeasurement {
  std::string site;
  /** Inverter pairs the site is currently built from; chain elements / 2. */
  int current_pairs = 0;
  /** Minimum slack over the constraints attributed to this site, in ps. */
  double binding_slack_ps = 0.0;
  /** Whether any constraint could be attributed to this site at all. */
  bool has_attribution = false;
  /**
   * Picoseconds of binding slack one added pair is expected to buy.
   *
   * Characterized per site from clean rebuilds, not derived in-run: one size
   * and one slack cannot yield a slope, and pretending otherwise would make the
   * requested count a guess wearing a formula. A site without one cannot be
   * sized, and that is a refusal rather than an occasion to reach for some
   * other site's number.
   */
  double ps_per_pair = 0.0;
  bool has_coefficient = false;
  /**
   * The pair range the coefficient was actually measured over.
   *
   * A slope measured between two sizes says nothing about a third one far
   * outside them. Predicting beyond this range is refused rather than
   * extrapolated, because the failure would look exactly like a correct
   * prediction until the timing came back.
   */
  int characterized_min_pairs = 0;
  int characterized_max_pairs = 0;
  struct ResponsePoint {
    int target_pairs = 0;
    double covered_deficit_ps = 0.0;
  };
  /** Fixed common-anchor responses, ordered by increasing target size. */
  std::vector<ResponsePoint> response_points;
};

/** Everything the request must fit inside. */
struct SizingLimits {
  /** Slack the selected site must reach, in ps. */
  double margin_ps = 0.0;
  int max_added_pairs = 0;
  int max_pairs = 0;
  /** Spare capacity, in components and nets, reserved when the design loaded. */
  size_t component_headroom = 0;
  size_t net_headroom = 0;
  /**
   * Registered sites the caller expects to have measured.
   *
   * Supplied so a decision cannot be taken on a partial view. If placement
   * registered eight delay lines and only three were measured, the other five
   * are unknown rather than satisfied, and the largest deficit may well be
   * among them.
   */
  std::vector<std::string> expected_sites;
};

enum class SizingOutcome {
  /** A site was selected and sized. */
  kRequest,
  /** Complete, trusted evidence that no site needs anything. Not an error. */
  kDecline,
  /** The evidence cannot support a decision. Placement fails. */
  kInvalid,
};

/**
 * The request, or why there isn't one.
 *
 * `expected_added_components` and `expected_added_nets` are what the caller will
 * hold the host's delta to. `plain_delay<P>` expands to `2P` inverter
 * components on `2P - 1` internal nets, so adding `k` pairs adds `2k` of each:
 * the net count differs by one from the component count at any given size, but
 * the *growth* is the same because the constant cancels. Stating the
 * expectation up front is what turns "the host returned a delta" into "the host
 * returned the delta that was asked for".
 */
struct SizingDecision {
  SizingOutcome outcome = SizingOutcome::kDecline;
  std::string site;
  int current_pairs = 0;
  int requested_pairs = 0;
  int expected_added_components = 0;
  int expected_added_nets = 0;
  /** Measured shortfall that motivated the request, in ps. */
  double deficit_ps = 0.0;
  /** Human-readable account of the outcome, for the run log. */
  std::string reason;

  int AddedPairs() const { return requested_pairs - current_pairs; }
};

/**
 * What a whole-batch decision produced.
 *
 * Every measured site gets a verdict, including the ones that produce no
 * request: a site silently absent from a batch is indistinguishable from a site
 * nobody looked at, and the previous single-site policy could only ever say
 * something about the one site it picked.
 */
struct SiteVerdict {
  std::string site;
  SizingOutcome outcome = SizingOutcome::kDecline;
  int current_pairs = 0;
  int requested_pairs = 0;
  double binding_slack_ps = 0.0;
  double deficit_ps = 0.0;
  std::string reason;
};

struct BatchSizingDecision {
  /** kRequest if anything is to be changed; kInvalid if the evidence fails. */
  SizingOutcome outcome = SizingOutcome::kDecline;
  /** One entry per measured site, in site-name order, request or not. */
  std::vector<SiteVerdict> verdicts;
  /** Only the sites to be grown, in site-name order. */
  std::vector<SiteVerdict> requests;
  int expected_added_components = 0;
  int expected_added_nets = 0;
  std::string reason;
};

/**
 * Decide for every measured site at once.
 *
 * The single-site rule applied per site, plus the constraints that only exist
 * for a batch: no duplicate site, every request strictly increasing, and the
 * aggregate growth fitting the headroom that was reserved when the design
 * loaded. Aggregate headroom is checked against the sum, because eight requests
 * that each fit individually can still not fit together.
 *
 * A site with no characterized coefficient is not silently skipped. It is
 * reported ineligible with a reason, and whether that fails the run or merely
 * excludes the site is the caller's configured policy rather than this
 * function's opinion.
 */
BatchSizingDecision DecideDelayLineBatchSizing(
    const std::vector<DelayLineSiteMeasurement> &sites,
    const SizingLimits &limits, bool require_every_site_characterized);

/** Choose the smallest measured common-anchor response covering each deficit. */
BatchSizingDecision DecideDelayLineResponseBatch(
    const std::vector<DelayLineSiteMeasurement> &sites,
    const SizingLimits &limits, bool require_every_site_characterized);

/**
 * Decides once, from measurements taken at one checkpoint.
 *
 * Selects the site whose deficit against the common margin is largest, which is
 * the site holding the worst attributable binding constraint, and breaks ties by
 * name so the choice does not depend on measurement order. Sizes it by rounding
 * the deficit up over the site's characterized gain, then refuses the request
 * outright if it does not fit within every limit rather than trimming it into
 * something nobody asked for.
 */
SizingDecision DecideDelayLineSizing(
    const std::vector<DelayLineSiteMeasurement> &sites,
    const SizingLimits &limits);

} // namespace dali

#endif // DALI_TIMING_DELAY_LINE_SIZING_POLICY_H_
