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
#include "dali/timing/delay_line_sizing_policy.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace dali {

namespace {

/**
 * The largest pair count that can be handled without integer overflow.
 *
 * Every arithmetic step downstream doubles a pair count or adds two of them, so
 * the bound is set well below INT_MAX rather than at it. A deficit divided by a
 * tiny coefficient produces a huge real number, and converting that to int is
 * undefined rather than merely wrong; it is checked as a double, before the
 * conversion.
 */
constexpr int kMaxRepresentablePairs = 1 << 20;

SizingDecision Invalid(std::string reason) {
  SizingDecision decision;
  decision.outcome = SizingOutcome::kInvalid;
  decision.reason = std::move(reason);
  return decision;
}

SizingDecision Decline(std::string reason) {
  SizingDecision decision;
  decision.outcome = SizingOutcome::kDecline;
  decision.reason = std::move(reason);
  return decision;
}

} // namespace

SizingDecision DecideDelayLineSizing(
    const std::vector<DelayLineSiteMeasurement> &sites,
    const SizingLimits &limits) {
  // --- the limits themselves must be usable -------------------------------
  if (!std::isfinite(limits.margin_ps)) {
    return Invalid("the required margin is not a finite number");
  }
  if (!(limits.max_added_pairs > 0) || !(limits.max_pairs > 0)) {
    return Invalid("sizing limits are not configured");
  }

  // --- the evidence must be complete before it can be trusted -------------
  // Nothing measured is not the same as nothing needed. A decision taken on an
  // empty or partial view would report success for sites it never looked at.
  if (sites.empty()) {
    return Invalid("no delay-line site was measured, so there is no evidence "
                   "to decide on");
  }

  std::unordered_set<std::string> seen;
  for (const DelayLineSiteMeasurement &site : sites) {
    if (site.site.empty()) {
      return Invalid("a measured site has no name");
    }
    if (!seen.insert(site.site).second) {
      return Invalid("site '" + site.site +
                     "' was measured more than once; attribution is ambiguous");
    }
    if (site.current_pairs <= 0 ||
        site.current_pairs > kMaxRepresentablePairs) {
      return Invalid("site '" + site.site + "' reports " +
                     std::to_string(site.current_pairs) +
                     " pairs, which is not a usable delay-line size");
    }
    if (site.has_attribution && !std::isfinite(site.binding_slack_ps)) {
      return Invalid("site '" + site.site +
                     "' reports a non-finite binding slack");
    }
  }

  // Every site placement registered must appear, or the view is partial and
  // the largest deficit may be among the ones missing from it.
  for (const std::string &expected : limits.expected_sites) {
    if (seen.count(expected) == 0) {
      return Invalid("registered site '" + expected +
                     "' was not measured, so the evidence is incomplete");
    }
  }

  // A site nothing was attributed to has an unknown slack, not a satisfied one.
  // With no attribution anywhere there is no evidence at all.
  size_t attributed = 0;
  for (const DelayLineSiteMeasurement &site : sites) {
    if (site.has_attribution) ++attributed;
  }
  if (attributed == 0) {
    return Invalid("no constraint was attributed to any measured site, so no "
                   "site's slack is known");
  }
  if (attributed != sites.size()) {
    return Invalid(
        std::to_string(sites.size() - attributed) +
        " of " + std::to_string(sites.size()) +
        " measured sites have no attributed constraint, so their slack is "
        "unknown rather than satisfied");
  }

  // --- select the worst attributable site ---------------------------------
  const DelayLineSiteMeasurement *selected = nullptr;
  double selected_deficit = 0.0;
  for (const DelayLineSiteMeasurement &site : sites) {
    const double deficit = limits.margin_ps - site.binding_slack_ps;
    if (deficit <= 0.0) continue;
    if (selected == nullptr || deficit > selected_deficit ||
        (deficit == selected_deficit && site.site < selected->site)) {
      selected = &site;
      selected_deficit = deficit;
    }
  }

  // The only decline: complete, trusted evidence that nothing is short.
  if (selected == nullptr) {
    return Decline("every registered site was measured, attributed, and meets "
                   "the margin");
  }

  // --- the selected site must be predictable ------------------------------
  if (!selected->has_coefficient) {
    return Invalid("site '" + selected->site +
                   "' has no characterized ps-per-pair coefficient, so its "
                   "size cannot be predicted");
  }
  if (!std::isfinite(selected->ps_per_pair) || selected->ps_per_pair <= 0.0) {
    return Invalid("site '" + selected->site +
                   "' has a non-positive or non-finite ps-per-pair "
                   "coefficient");
  }
  if (selected->characterized_min_pairs <= 0 ||
      selected->characterized_max_pairs < selected->characterized_min_pairs) {
    return Invalid("site '" + selected->site +
                   "' has no usable characterization range");
  }
  if (selected->current_pairs < selected->characterized_min_pairs ||
      selected->current_pairs > selected->characterized_max_pairs) {
    return Invalid("site '" + selected->site + "' is at " +
                   std::to_string(selected->current_pairs) +
                   " pairs, outside its characterized range [" +
                   std::to_string(selected->characterized_min_pairs) + ", " +
                   std::to_string(selected->characterized_max_pairs) + "]");
  }

  SizingDecision decision;
  decision.site = selected->site;
  decision.current_pairs = selected->current_pairs;
  decision.deficit_ps = selected_deficit;

  // Rounded up, so the prediction covers the whole shortfall rather than most
  // of it. Checked as a double first: a tiny coefficient makes this enormous,
  // and converting an out-of-range double to int is undefined behaviour rather
  // than a large number.
  const double added_pairs_real =
      std::ceil(selected_deficit / selected->ps_per_pair);
  if (!std::isfinite(added_pairs_real) ||
      added_pairs_real > static_cast<double>(kMaxRepresentablePairs)) {
    return Invalid("site '" + selected->site + "' needs " +
                   std::to_string(selected_deficit) + " ps at " +
                   std::to_string(selected->ps_per_pair) +
                   " ps per pair, which is not a representable pair count");
  }
  const int added_pairs = static_cast<int>(added_pairs_real);
  if (added_pairs <= 0) {
    return Decline("site '" + selected->site +
                   "' needs less than one pair to reach the margin");
  }
  decision.requested_pairs = selected->current_pairs + added_pairs;
  // plain_delay<P> is 2P inverter components on 2P-1 internal nets. The counts
  // differ by one at any size, but k added pairs adds 2k of each, because the
  // constant cancels in the difference.
  decision.expected_added_components = 2 * added_pairs;
  decision.expected_added_nets = 2 * added_pairs;

  // --- every bound refuses rather than trims ------------------------------
  // A request quietly reduced to fit would be a different experiment from the
  // one the rule predicted, reported under the same name.
  if (added_pairs > limits.max_added_pairs) {
    return Invalid("site '" + selected->site + "' needs " +
                   std::to_string(added_pairs) + " added pairs, above the " +
                   std::to_string(limits.max_added_pairs) + " allowed");
  }
  if (decision.requested_pairs > limits.max_pairs) {
    return Invalid("site '" + selected->site + "' would reach " +
                   std::to_string(decision.requested_pairs) +
                   " pairs, above the " + std::to_string(limits.max_pairs) +
                   " allowed");
  }
  if (decision.requested_pairs <= decision.current_pairs) {
    return Invalid("site '" + selected->site +
                   "' would not grow, so there is nothing to request");
  }
  // The coefficient describes the range it was measured over and nothing
  // beyond it. A prediction that lands outside is refused, not extrapolated.
  if (decision.requested_pairs > selected->characterized_max_pairs) {
    return Invalid("site '" + selected->site + "' would reach " +
                   std::to_string(decision.requested_pairs) +
                   " pairs, beyond the characterized range [" +
                   std::to_string(selected->characterized_min_pairs) + ", " +
                   std::to_string(selected->characterized_max_pairs) +
                   "]; the coefficient does not describe that size");
  }
  if (static_cast<size_t>(decision.expected_added_components) >
      limits.component_headroom) {
    return Invalid("site '" + selected->site + "' needs " +
                   std::to_string(decision.expected_added_components) +
                   " components but only " +
                   std::to_string(limits.component_headroom) +
                   " are reserved");
  }
  if (static_cast<size_t>(decision.expected_added_nets) >
      limits.net_headroom) {
    return Invalid("site '" + selected->site + "' needs " +
                   std::to_string(decision.expected_added_nets) +
                   " nets but only " + std::to_string(limits.net_headroom) +
                   " are reserved");
  }

  decision.outcome = SizingOutcome::kRequest;
  decision.reason =
      "site '" + selected->site + "' is short of the margin by " +
      std::to_string(selected_deficit) + " ps at " +
      std::to_string(selected->ps_per_pair) + " ps per pair, so " +
      std::to_string(added_pairs) + " pair(s) are requested";
  return decision;
}


namespace {

/** Per-site verdict, using exactly the single-site rule. */
SiteVerdict JudgeOneSite(const DelayLineSiteMeasurement &site,
                         const SizingLimits &limits) {
  SiteVerdict verdict;
  verdict.site = site.site;
  verdict.current_pairs = site.current_pairs;
  verdict.binding_slack_ps = site.binding_slack_ps;
  verdict.deficit_ps = limits.margin_ps - site.binding_slack_ps;

  if (verdict.deficit_ps <= 0.0) {
    verdict.outcome = SizingOutcome::kDecline;
    verdict.reason = "meets the margin";
    return verdict;
  }
  if (!site.has_coefficient) {
    verdict.outcome = SizingOutcome::kInvalid;
    verdict.reason = "no characterized ps-per-pair coefficient for this site";
    return verdict;
  }
  if (!std::isfinite(site.ps_per_pair) || site.ps_per_pair <= 0.0) {
    verdict.outcome = SizingOutcome::kInvalid;
    verdict.reason = "non-positive or non-finite coefficient";
    return verdict;
  }
  if (site.characterized_min_pairs <= 0 ||
      site.characterized_max_pairs < site.characterized_min_pairs) {
    verdict.outcome = SizingOutcome::kInvalid;
    verdict.reason = "no usable characterization range";
    return verdict;
  }
  if (site.current_pairs < site.characterized_min_pairs ||
      site.current_pairs > site.characterized_max_pairs) {
    verdict.outcome = SizingOutcome::kInvalid;
    verdict.reason = "current size " + std::to_string(site.current_pairs) +
                     " is outside the characterized range [" +
                     std::to_string(site.characterized_min_pairs) + ", " +
                     std::to_string(site.characterized_max_pairs) + "]";
    return verdict;
  }

  const double added_real = std::ceil(verdict.deficit_ps / site.ps_per_pair);
  if (!std::isfinite(added_real) ||
      added_real > static_cast<double>(kMaxRepresentablePairs)) {
    verdict.outcome = SizingOutcome::kInvalid;
    verdict.reason = "the required pair count is not representable";
    return verdict;
  }
  const int added = static_cast<int>(added_real);
  if (added <= 0) {
    verdict.outcome = SizingOutcome::kDecline;
    verdict.reason = "less than one pair needed";
    return verdict;
  }
  verdict.requested_pairs = site.current_pairs + added;

  // Every bound refuses rather than trims. A request quietly reduced to fit is
  // a different request, and it would reach the margin by accident or not at
  // all while looking like a decision.
  if (added > limits.max_added_pairs) {
    verdict.outcome = SizingOutcome::kInvalid;
    verdict.reason = std::to_string(added) + " added pairs, above the " +
                     std::to_string(limits.max_added_pairs) + " allowed";
    return verdict;
  }
  if (verdict.requested_pairs > limits.max_pairs) {
    verdict.outcome = SizingOutcome::kInvalid;
    verdict.reason = "would reach " + std::to_string(verdict.requested_pairs) +
                     " pairs, above the configured " +
                     std::to_string(limits.max_pairs);
    return verdict;
  }
  if (verdict.requested_pairs > site.characterized_max_pairs) {
    verdict.outcome = SizingOutcome::kInvalid;
    verdict.reason = "would reach " + std::to_string(verdict.requested_pairs) +
                     " pairs, beyond the characterized range";
    return verdict;
  }
  if (verdict.requested_pairs <= verdict.current_pairs) {
    verdict.outcome = SizingOutcome::kInvalid;
    verdict.reason = "would not grow the site";
    return verdict;
  }
  verdict.outcome = SizingOutcome::kRequest;
  return verdict;
}

} // namespace

BatchSizingDecision DecideDelayLineBatchSizing(
    const std::vector<DelayLineSiteMeasurement> &sites,
    const SizingLimits &limits, bool require_every_site_characterized) {
  BatchSizingDecision decision;
  auto invalid = [&decision](std::string reason) {
    decision.outcome = SizingOutcome::kInvalid;
    decision.reason = std::move(reason);
    return decision;
  };

  if (!std::isfinite(limits.margin_ps)) {
    return invalid("the required margin is not a finite number");
  }
  if (!(limits.max_added_pairs > 0) || !(limits.max_pairs > 0)) {
    return invalid("sizing limits are not configured");
  }
  if (sites.empty()) {
    return invalid("no delay-line site was measured, so there is no evidence "
                   "to decide on");
  }

  std::unordered_set<std::string> seen;
  for (const DelayLineSiteMeasurement &site : sites) {
    if (site.site.empty()) return invalid("a measured site has no name");
    if (!seen.insert(site.site).second) {
      return invalid("site '" + site.site +
                     "' was measured more than once; attribution is ambiguous");
    }
    if (site.current_pairs <= 0 ||
        site.current_pairs > kMaxRepresentablePairs) {
      return invalid("site '" + site.site + "' reports " +
                     std::to_string(site.current_pairs) +
                     " pairs, which is not a usable delay-line size");
    }
    if (!site.has_attribution) {
      return invalid("site '" + site.site +
                     "' has no attributed constraint, so its slack is unknown "
                     "rather than satisfied");
    }
    if (!std::isfinite(site.binding_slack_ps)) {
      return invalid("site '" + site.site +
                     "' reports a non-finite binding slack");
    }
  }
  for (const std::string &expected : limits.expected_sites) {
    if (seen.count(expected) == 0) {
      return invalid("registered site '" + expected +
                     "' was not measured, so the evidence is incomplete");
    }
  }

  std::vector<DelayLineSiteMeasurement> ordered = sites;
  std::sort(ordered.begin(), ordered.end(),
            [](const DelayLineSiteMeasurement &left,
               const DelayLineSiteMeasurement &right) {
              return left.site < right.site;
            });

  for (const DelayLineSiteMeasurement &site : ordered) {
    SiteVerdict verdict = JudgeOneSite(site, limits);
    if (verdict.outcome == SizingOutcome::kInvalid) {
      // An uncharacterized site short of its margin is a real gap in the
      // evidence, and whether it stops the run is configured rather than
      // decided here. Either way it is named.
      if (require_every_site_characterized) {
        decision.verdicts.push_back(verdict);
        return invalid("site '" + verdict.site + "' cannot be sized: " +
                       verdict.reason);
      }
    }
    if (verdict.outcome == SizingOutcome::kRequest) {
      decision.expected_added_components += 2 * (verdict.requested_pairs -
                                                 verdict.current_pairs);
      decision.expected_added_nets += 2 * (verdict.requested_pairs -
                                           verdict.current_pairs);
      decision.requests.push_back(verdict);
    }
    decision.verdicts.push_back(std::move(verdict));
  }

  // Eight requests that each fit on their own can still not fit together, so
  // the headroom is checked against the sum and not against any one of them.
  if (static_cast<size_t>(decision.expected_added_components) >
      limits.component_headroom) {
    return invalid("the batch adds " +
                   std::to_string(decision.expected_added_components) +
                   " components but only " +
                   std::to_string(limits.component_headroom) +
                   " of headroom was reserved");
  }
  if (static_cast<size_t>(decision.expected_added_nets) >
      limits.net_headroom) {
    return invalid("the batch adds " +
                   std::to_string(decision.expected_added_nets) +
                   " nets but only " + std::to_string(limits.net_headroom) +
                   " of headroom was reserved");
  }

  if (decision.requests.empty()) {
    decision.outcome = SizingOutcome::kDecline;
    decision.reason = "no measured site both needs and can take more pairs";
    return decision;
  }
  decision.outcome = SizingOutcome::kRequest;
  return decision;
}

BatchSizingDecision DecideDelayLineResponseBatch(
    const std::vector<DelayLineSiteMeasurement> &sites,
    const SizingLimits &limits, bool require_every_site_characterized) {
  BatchSizingDecision decision;
  auto invalid = [&decision](std::string reason) {
    decision.outcome = SizingOutcome::kInvalid;
    decision.reason = std::move(reason);
    return decision;
  };

  if (!std::isfinite(limits.margin_ps) || limits.max_added_pairs <= 0 ||
      limits.max_pairs <= 0) {
    return invalid("response sizing limits are not configured");
  }
  if (sites.empty()) return invalid("no delay-line site was measured");

  std::unordered_set<std::string> seen;
  for (const DelayLineSiteMeasurement &site : sites) {
    if (site.site.empty() || site.current_pairs <= 0 ||
        !site.has_attribution || !std::isfinite(site.binding_slack_ps)) {
      return invalid("site measurements are incomplete or malformed");
    }
    if (!seen.insert(site.site).second) {
      return invalid("site '" + site.site + "' was measured more than once");
    }
  }
  for (const std::string &expected : limits.expected_sites) {
    if (seen.count(expected) == 0) {
      return invalid("registered site '" + expected + "' was not measured");
    }
  }

  std::vector<DelayLineSiteMeasurement> ordered = sites;
  std::sort(ordered.begin(), ordered.end(),
            [](const DelayLineSiteMeasurement &left,
               const DelayLineSiteMeasurement &right) {
              return left.site < right.site;
            });
  for (const DelayLineSiteMeasurement &site : ordered) {
    SiteVerdict verdict;
    verdict.site = site.site;
    verdict.current_pairs = site.current_pairs;
    verdict.binding_slack_ps = site.binding_slack_ps;
    verdict.deficit_ps = limits.margin_ps - site.binding_slack_ps;
    if (verdict.deficit_ps <= 0.0) {
      verdict.outcome = SizingOutcome::kDecline;
      verdict.reason = "meets the margin";
      decision.verdicts.push_back(std::move(verdict));
      continue;
    }

    int previous_target = site.current_pairs;
    double previous_coverage = -1.0;
    const DelayLineSiteMeasurement::ResponsePoint *chosen = nullptr;
    bool malformed = false;
    for (const auto &point : site.response_points) {
      if (point.target_pairs <= previous_target ||
          !std::isfinite(point.covered_deficit_ps) ||
          point.covered_deficit_ps <= previous_coverage) {
        malformed = true;
        break;
      }
      previous_target = point.target_pairs;
      previous_coverage = point.covered_deficit_ps;
      if (chosen == nullptr && point.covered_deficit_ps >= verdict.deficit_ps) {
        chosen = &point;
      }
    }
    if (malformed) {
      return invalid("site '" + site.site +
                     "' has unordered or non-monotone response evidence");
    }
    if (chosen == nullptr) {
      verdict.outcome = SizingOutcome::kInvalid;
      verdict.reason = site.response_points.empty()
                           ? "no incremental response is characterized"
                           : "deficit exceeds every characterized response";
      decision.verdicts.push_back(verdict);
      if (require_every_site_characterized) {
        return invalid("site '" + site.site + "' cannot be sized: " +
                       verdict.reason);
      }
      continue;
    }

    verdict.requested_pairs = chosen->target_pairs;
    const int added = verdict.requested_pairs - verdict.current_pairs;
    if (added <= 0 || added > limits.max_added_pairs ||
        verdict.requested_pairs > limits.max_pairs) {
      return invalid("site '" + site.site +
                     "' response does not fit the configured pair bounds");
    }
    verdict.outcome = SizingOutcome::kRequest;
    verdict.reason = "smallest measured response covering the deficit";
    decision.expected_added_components += 2 * added;
    decision.expected_added_nets += 2 * added;
    decision.requests.push_back(verdict);
    decision.verdicts.push_back(std::move(verdict));
  }

  if (static_cast<size_t>(decision.expected_added_components) >
          limits.component_headroom ||
      static_cast<size_t>(decision.expected_added_nets) > limits.net_headroom) {
    return invalid("the response batch exceeds reserved topology headroom");
  }
  if (decision.requests.empty()) {
    decision.outcome = SizingOutcome::kDecline;
    decision.reason = "no measured site needs a characterized response";
    return decision;
  }
  decision.outcome = SizingOutcome::kRequest;
  return decision;
}

} // namespace dali
