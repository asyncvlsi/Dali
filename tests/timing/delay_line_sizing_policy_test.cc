/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/
/*
 * The whole sizing decision, argued with directly.
 *
 * The policy takes values and returns a request or a refusal, so every rule it
 * applies can be checked here without a placer, a netlist, or a host. That
 * matters more than usual for this one: it is the first piece of this project
 * that chooses something on its own, and a chooser that is only ever exercised
 * through a full flow is a chooser nobody can hold to a rule.
 *
 * Three outcomes are distinguished throughout. A request is a change worth
 * making; a decline is a design that needs nothing, which is an ordinary result
 * and not a failure; and invalid is a measurement or limit that cannot be
 * trusted, which must stop the run rather than be rounded into a decision.
 */
#include "dali/timing/delay_line_sizing_policy.h"

#include <gtest/gtest.h>

#include <limits>

#include <string>
#include <vector>

namespace dali {
namespace {

SizingLimits Limits() {
  SizingLimits limits;
  limits.margin_ps = 25.0;
  limits.max_added_pairs = 64;
  limits.max_pairs = 256;
  limits.component_headroom = 1024;
  limits.net_headroom = 1024;
  return limits;
}

/** A site that needs `deficit` ps, at 10 ps per added pair. */
DelayLineSiteMeasurement Site(const std::string &name, int pairs,
                              double binding_slack) {
  DelayLineSiteMeasurement site;
  site.site = name;
  site.current_pairs = pairs;
  site.binding_slack_ps = binding_slack;
  site.has_attribution = true;
  site.ps_per_pair = 10.0;
  site.has_coefficient = true;
  site.characterized_min_pairs = 1;
  site.characterized_max_pairs = 64;
  return site;
}

TEST(DelayLineSizingPolicyTest, LargestDeficitWins) {
  // Margin 25: dl0 is short by 35, dl1 by 75, dl2 by 5.
  const std::vector<DelayLineSiteMeasurement> sites = {
      Site("dl0", 7, -10.0), Site("dl1", 9, -50.0), Site("dl2", 5, 20.0)};

  const SizingDecision decision = DecideDelayLineSizing(sites, Limits());

  ASSERT_EQ(decision.outcome, SizingOutcome::kRequest) << decision.reason;
  EXPECT_EQ(decision.site, "dl1");
  EXPECT_DOUBLE_EQ(decision.deficit_ps, 75.0);
}

// Selection must not depend on the order measurements arrived in.
TEST(DelayLineSizingPolicyTest, TiesBreakByNameNotByOrder) {
  std::vector<DelayLineSiteMeasurement> sites = {Site("dl7", 7, -10.0),
                                                 Site("dl3", 7, -10.0)};
  EXPECT_EQ(DecideDelayLineSizing(sites, Limits()).site, "dl3");
  std::swap(sites[0], sites[1]);
  EXPECT_EQ(DecideDelayLineSizing(sites, Limits()).site, "dl3");
}

// Rounded up, so the prediction covers the whole shortfall. 35 ps at 10 ps per
// pair is four pairs, not three and a half.
TEST(DelayLineSizingPolicyTest, SizingRoundsUpToCoverTheDeficit) {
  const SizingDecision decision =
      DecideDelayLineSizing({Site("dl0", 7, -10.0)}, Limits());

  ASSERT_EQ(decision.outcome, SizingOutcome::kRequest) << decision.reason;
  EXPECT_DOUBLE_EQ(decision.deficit_ps, 35.0);
  EXPECT_EQ(decision.AddedPairs(), 4);
  EXPECT_EQ(decision.requested_pairs, 11);
}

// plain_delay<P> is 2P inverters on 2P internal nets, so the caller can hold the
// host's delta to an exact expectation instead of accepting whatever arrives.
TEST(DelayLineSizingPolicyTest, ExpectedGrowthIsTwoPerAddedPair) {
  const SizingDecision decision =
      DecideDelayLineSizing({Site("dl0", 7, -10.0)}, Limits());
  ASSERT_EQ(decision.outcome, SizingOutcome::kRequest);
  EXPECT_EQ(decision.expected_added_components, 2 * decision.AddedPairs());
  EXPECT_EQ(decision.expected_added_nets, 2 * decision.AddedPairs());
}

// A design that needs nothing is a result, not a fault.
TEST(DelayLineSizingPolicyTest, DeclinesWhenEverySiteMeetsTheMargin) {
  const SizingDecision decision = DecideDelayLineSizing(
      {Site("dl0", 7, 30.0), Site("dl1", 9, 25.0)}, Limits());
  EXPECT_EQ(decision.outcome, SizingOutcome::kDecline) << decision.reason;
  EXPECT_EQ(decision.requested_pairs, 0);
}

// A site nothing was attributed to has an unknown slack, not a satisfied one.
// Calling that "needs nothing" would report success for a site never looked at.
TEST(DelayLineSizingPolicyTest, WhollyUnattributedEvidenceIsInvalid) {
  DelayLineSiteMeasurement unattributed = Site("dl0", 7, -1000.0);
  unattributed.has_attribution = false;
  const SizingDecision decision =
      DecideDelayLineSizing({unattributed}, Limits());
  EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid) << decision.reason;
}

TEST(DelayLineSizingPolicyTest, PartiallyAttributedEvidenceIsInvalid) {
  DelayLineSiteMeasurement blind = Site("dl1", 9, 0.0);
  blind.has_attribution = false;
  const SizingDecision decision =
      DecideDelayLineSizing({Site("dl0", 7, 30.0), blind}, Limits());
  EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid) << decision.reason;
  EXPECT_NE(decision.reason.find("unknown rather than satisfied"),
            std::string::npos);
}

// Placement registered eight lines; measuring three of them is not evidence
// that the other five are fine.
TEST(DelayLineSizingPolicyTest, IncompleteSiteCoverageIsInvalid) {
  SizingLimits limits = Limits();
  limits.expected_sites = {"dl0", "dl1", "dl2"};
  const SizingDecision decision = DecideDelayLineSizing(
      {Site("dl0", 7, 30.0), Site("dl1", 9, 40.0)}, limits);
  EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid) << decision.reason;
  EXPECT_NE(decision.reason.find("dl2"), std::string::npos);
}

// The coefficient describes the sizes it was measured over. A prediction
// landing outside is refused rather than extrapolated, because an extrapolation
// looks exactly like a correct prediction until the timing comes back.
TEST(DelayLineSizingPolicyTest, PredictionBeyondCharacterizationIsInvalid) {
  DelayLineSiteMeasurement site = Site("dl0", 7, -10.0);  // needs 4 pairs -> 11
  site.characterized_min_pairs = 7;
  site.characterized_max_pairs = 10;
  const SizingDecision decision = DecideDelayLineSizing({site}, Limits());
  EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid) << decision.reason;
  EXPECT_NE(decision.reason.find("characterized range"), std::string::npos);
}

TEST(DelayLineSizingPolicyTest, CurrentSizeOutsideCharacterizationIsInvalid) {
  DelayLineSiteMeasurement site = Site("dl0", 7, -10.0);
  site.characterized_min_pairs = 12;
  site.characterized_max_pairs = 17;
  EXPECT_EQ(DecideDelayLineSizing({site}, Limits()).outcome,
            SizingOutcome::kInvalid);
}

// The same site, characterized wide enough to contain the prediction, is
// requested -- so the refusals above are on the range and not on something else.
TEST(DelayLineSizingPolicyTest, PredictionInsideCharacterizationIsRequested) {
  DelayLineSiteMeasurement site = Site("dl0", 7, -10.0);
  site.characterized_min_pairs = 7;
  site.characterized_max_pairs = 17;
  const SizingDecision decision = DecideDelayLineSizing({site}, Limits());
  ASSERT_EQ(decision.outcome, SizingOutcome::kRequest) << decision.reason;
  EXPECT_EQ(decision.requested_pairs, 11);
}

// A tiny coefficient against a large deficit produces a real number far outside
// int. Converting that is undefined behaviour, so it is caught as a double.
TEST(DelayLineSizingPolicyTest, UnrepresentablePairCountIsInvalid) {
  DelayLineSiteMeasurement site = Site("dl0", 7, -1e18);
  site.ps_per_pair = 1e-12;
  site.characterized_max_pairs = 1 << 20;
  const SizingDecision decision = DecideDelayLineSizing({site}, Limits());
  EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid) << decision.reason;
  EXPECT_NE(decision.reason.find("representable"), std::string::npos);
}

TEST(DelayLineSizingPolicyTest, NonFiniteMarginIsInvalid) {
  SizingLimits nan_margin = Limits();
  nan_margin.margin_ps = std::numeric_limits<double>::quiet_NaN();
  EXPECT_EQ(DecideDelayLineSizing({Site("dl0", 7, -10.0)}, nan_margin).outcome,
            SizingOutcome::kInvalid);
  SizingLimits infinite_margin = Limits();
  infinite_margin.margin_ps = std::numeric_limits<double>::infinity();
  EXPECT_EQ(
      DecideDelayLineSizing({Site("dl0", 7, -10.0)}, infinite_margin).outcome,
      SizingOutcome::kInvalid);
}

// A pair count near INT_MAX would overflow doubling it, so it is refused as a
// measurement rather than carried into the arithmetic.
TEST(DelayLineSizingPolicyTest, AbsurdCurrentSizeIsInvalid) {
  DelayLineSiteMeasurement site = Site("dl0", 7, -10.0);
  site.current_pairs = std::numeric_limits<int>::max();
  EXPECT_EQ(DecideDelayLineSizing({site}, Limits()).outcome,
            SizingOutcome::kInvalid);
}

// One size and one slack cannot yield a slope. A site without a characterized
// coefficient stops the run rather than borrowing another site's number.
TEST(DelayLineSizingPolicyTest, MissingCoefficientIsInvalid) {
  DelayLineSiteMeasurement site = Site("dl0", 7, -10.0);
  site.has_coefficient = false;
  const SizingDecision decision = DecideDelayLineSizing({site}, Limits());
  EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid) << decision.reason;
  EXPECT_NE(decision.reason.find("coefficient"), std::string::npos);
}

TEST(DelayLineSizingPolicyTest, NonPositiveCoefficientIsInvalid) {
  DelayLineSiteMeasurement site = Site("dl0", 7, -10.0);
  site.ps_per_pair = 0.0;
  EXPECT_EQ(DecideDelayLineSizing({site}, Limits()).outcome,
            SizingOutcome::kInvalid);
  site.ps_per_pair = -5.0;
  EXPECT_EQ(DecideDelayLineSizing({site}, Limits()).outcome,
            SizingOutcome::kInvalid);
}

// The same site measured twice leaves no principled way to choose between the
// two readings, so the policy refuses rather than picking one.
TEST(DelayLineSizingPolicyTest, AmbiguousAttributionIsInvalid) {
  const SizingDecision decision = DecideDelayLineSizing(
      {Site("dl0", 7, -10.0), Site("dl0", 9, -80.0)}, Limits());
  EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid) << decision.reason;
  EXPECT_NE(decision.reason.find("ambiguous"), std::string::npos);
}

TEST(DelayLineSizingPolicyTest, MalformedMeasurementsAreInvalid) {
  DelayLineSiteMeasurement unnamed = Site("", 7, -10.0);
  EXPECT_EQ(DecideDelayLineSizing({unnamed}, Limits()).outcome,
            SizingOutcome::kInvalid);

  DelayLineSiteMeasurement sizeless = Site("dl0", 0, -10.0);
  EXPECT_EQ(DecideDelayLineSizing({sizeless}, Limits()).outcome,
            SizingOutcome::kInvalid);

  DelayLineSiteMeasurement infinite = Site("dl0", 7, 0.0);
  infinite.binding_slack_ps = std::numeric_limits<double>::quiet_NaN();
  EXPECT_EQ(DecideDelayLineSizing({infinite}, Limits()).outcome,
            SizingOutcome::kInvalid);
}

// Each bound refuses rather than trimming: a request quietly reduced to fit
// would be a different experiment reported under the same name.
TEST(DelayLineSizingPolicyTest, EveryBoundRefusesRatherThanTrims) {
  struct Case {
    const char *what;
    SizingLimits limits;
  };
  SizingLimits added = Limits();
  added.max_added_pairs = 3;  // the site needs 4
  SizingLimits total = Limits();
  total.max_pairs = 9;  // 7 + 4 = 11
  SizingLimits components = Limits();
  components.component_headroom = 7;  // needs 8
  SizingLimits nets = Limits();
  nets.net_headroom = 7;  // needs 8

  for (const Case &test_case : {Case{"max added pairs", added},
                                Case{"max pairs", total},
                                Case{"component headroom", components},
                                Case{"net headroom", nets}}) {
    const SizingDecision decision =
        DecideDelayLineSizing({Site("dl0", 7, -10.0)}, test_case.limits);
    EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid)
        << test_case.what << ": " << decision.reason;
    EXPECT_EQ(decision.requested_pairs, 0)
        << test_case.what << " produced a trimmed request";
  }
}

// The same limits with the bound raised must produce the request, so the cases
// above are known to fail on the bound and not on something incidental.
TEST(DelayLineSizingPolicyTest, TheSameSiteIsRequestedWhenBoundsAllowIt) {
  const SizingDecision decision =
      DecideDelayLineSizing({Site("dl0", 7, -10.0)}, Limits());
  ASSERT_EQ(decision.outcome, SizingOutcome::kRequest) << decision.reason;
  EXPECT_EQ(decision.AddedPairs(), 4);
  EXPECT_EQ(decision.expected_added_components, 8);
}

TEST(DelayLineSizingPolicyTest, UnconfiguredLimitsAreInvalid) {
  SizingLimits unset;
  EXPECT_EQ(DecideDelayLineSizing({Site("dl0", 7, -10.0)}, unset).outcome,
            SizingOutcome::kInvalid);
}

// Nothing measured is not the same as nothing needed.
TEST(DelayLineSizingPolicyTest, NoMeasuredSitesIsInvalid) {
  EXPECT_EQ(DecideDelayLineSizing({}, Limits()).outcome,
            SizingOutcome::kInvalid);
}

// The one decline: complete, attributed, in-coverage evidence that nothing is
// short of the margin.
TEST(DelayLineSizingPolicyTest, DeclineRequiresCompleteTrustedEvidence) {
  SizingLimits limits = Limits();
  limits.expected_sites = {"dl0", "dl1"};
  const SizingDecision decision = DecideDelayLineSizing(
      {Site("dl0", 7, 30.0), Site("dl1", 9, 25.0)}, limits);
  EXPECT_EQ(decision.outcome, SizingOutcome::kDecline) << decision.reason;
}


// --- the batch -------------------------------------------------------------
//
// A batch exists because the netlist may change only once. Two mutations would
// be two ACT re-elaborations, and the second would be sizing against a design
// the first had already altered.

TEST(DelayLineSizingPolicyTest, EveryShortSiteIsRequestedInNameOrder) {
  // Margin 25: dl2 short by 35, dl0 by 75, dl1 meets it.
  const std::vector<DelayLineSiteMeasurement> sites = {
      Site("dl2", 7, -10.0), Site("dl0", 9, -50.0), Site("dl1", 5, 30.0)};

  const BatchSizingDecision decision =
      DecideDelayLineBatchSizing(sites, Limits(), true);

  ASSERT_EQ(decision.outcome, SizingOutcome::kRequest) << decision.reason;
  ASSERT_EQ(decision.requests.size(), 2U);
  // Sorted by name, not by the order they were measured in: the same decision
  // must produce the same request every time it is taken.
  EXPECT_EQ(decision.requests[0].site, "dl0");
  EXPECT_EQ(decision.requests[1].site, "dl2");
  EXPECT_EQ(decision.requests[0].requested_pairs, 9 + 8);
  EXPECT_EQ(decision.requests[1].requested_pairs, 7 + 4);
  // Growth is two components and two nets per added pair, summed.
  EXPECT_EQ(decision.expected_added_components, 2 * (8 + 4));
  EXPECT_EQ(decision.expected_added_nets, 2 * (8 + 4));

  // Every measured site is accounted for, including the one that needs nothing.
  ASSERT_EQ(decision.verdicts.size(), 3U);
  EXPECT_EQ(decision.verdicts[1].site, "dl1");
  EXPECT_EQ(decision.verdicts[1].outcome, SizingOutcome::kDecline);
}

// Control 1: nothing to do means the host is never called, which is what makes
// "no change" different from "changed nothing".
TEST(DelayLineSizingPolicyTest, ABatchWithNothingToDoDeclines) {
  const std::vector<DelayLineSiteMeasurement> sites = {
      Site("dl0", 7, 30.0), Site("dl1", 9, 40.0)};

  const BatchSizingDecision decision =
      DecideDelayLineBatchSizing(sites, Limits(), true);

  EXPECT_EQ(decision.outcome, SizingOutcome::kDecline);
  EXPECT_TRUE(decision.requests.empty());
  EXPECT_EQ(decision.expected_added_components, 0);
}

TEST(DelayLineSizingPolicyTest, ADuplicateSiteInvalidatesTheBatch) {
  const std::vector<DelayLineSiteMeasurement> sites = {
      Site("dl0", 7, -10.0), Site("dl0", 9, -50.0)};

  const BatchSizingDecision decision =
      DecideDelayLineBatchSizing(sites, Limits(), true);

  EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid);
  EXPECT_NE(decision.reason.find("more than once"), std::string::npos);
}

// Control 3: an uncharacterized site short of its margin is a gap in the
// evidence, not a site that needs nothing. Under the strict policy it stops the
// run; under the permissive one it is excluded by name and never silently.
TEST(DelayLineSizingPolicyTest, AnUncharacterizedShortSiteIsNeverSilent) {
  DelayLineSiteMeasurement bare = Site("dl1", 9, -50.0);
  bare.has_coefficient = false;
  const std::vector<DelayLineSiteMeasurement> sites = {Site("dl0", 7, -10.0),
                                                       bare};

  const BatchSizingDecision strict =
      DecideDelayLineBatchSizing(sites, Limits(), true);
  EXPECT_EQ(strict.outcome, SizingOutcome::kInvalid);
  EXPECT_NE(strict.reason.find("dl1"), std::string::npos);

  const BatchSizingDecision permissive =
      DecideDelayLineBatchSizing(sites, Limits(), false);
  ASSERT_EQ(permissive.outcome, SizingOutcome::kRequest) << permissive.reason;
  ASSERT_EQ(permissive.requests.size(), 1U);
  EXPECT_EQ(permissive.requests[0].site, "dl0");
  // Excluded, but present in the verdicts with a reason.
  ASSERT_EQ(permissive.verdicts.size(), 2U);
  EXPECT_EQ(permissive.verdicts[1].site, "dl1");
  EXPECT_EQ(permissive.verdicts[1].outcome, SizingOutcome::kInvalid);
  EXPECT_NE(permissive.verdicts[1].reason.find("no characterized"),
            std::string::npos);
}

TEST(DelayLineSizingPolicyTest, ACoefficientOutsideItsRangeInvalidatesASite) {
  DelayLineSiteMeasurement narrow = Site("dl1", 9, -50.0);
  narrow.characterized_min_pairs = 20;   // the site is at 9
  narrow.characterized_max_pairs = 40;
  const std::vector<DelayLineSiteMeasurement> sites = {narrow};

  const BatchSizingDecision decision =
      DecideDelayLineBatchSizing(sites, Limits(), true);
  EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid);
  EXPECT_NE(decision.reason.find("outside the characterized range"),
            std::string::npos);
}

// Control 8: eight requests that each fit on their own need not fit together.
TEST(DelayLineSizingPolicyTest, AggregateHeadroomIsCheckedAgainstTheSum) {
  SizingLimits limits = Limits();
  // Each site needs 8 added pairs: 16 components each, 32 together.
  limits.component_headroom = 20;
  const std::vector<DelayLineSiteMeasurement> sites = {
      Site("dl0", 9, -50.0), Site("dl1", 9, -50.0)};

  const BatchSizingDecision decision =
      DecideDelayLineBatchSizing(sites, limits, true);
  EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid);
  EXPECT_NE(decision.reason.find("headroom"), std::string::npos);

  // Each one alone fits, which is exactly why the sum has to be checked.
  const BatchSizingDecision alone = DecideDelayLineBatchSizing(
      {Site("dl0", 9, -50.0)}, limits, true);
  EXPECT_EQ(alone.outcome, SizingOutcome::kRequest) << alone.reason;
}

TEST(DelayLineSizingPolicyTest, ABatchBoundRefusesRatherThanTrimming) {
  SizingLimits limits = Limits();
  limits.max_pairs = 12;                        // dl0 would reach 17
  const BatchSizingDecision decision =
      DecideDelayLineBatchSizing({Site("dl0", 9, -50.0)}, limits, true);

  EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid);
  EXPECT_EQ(decision.verdicts[0].requested_pairs, 17)
      << "the refused request must be reported as it was computed, not trimmed "
         "to whatever would have fitted";
}

TEST(DelayLineSizingPolicyTest, AnUnmeasuredRegisteredSiteInvalidatesTheBatch) {
  SizingLimits limits = Limits();
  limits.expected_sites = {"dl0", "dl1"};
  const BatchSizingDecision decision =
      DecideDelayLineBatchSizing({Site("dl0", 7, -10.0)}, limits, true);

  EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid);
  EXPECT_NE(decision.reason.find("dl1"), std::string::npos);
}

DelayLineSiteMeasurement ResponseSite(
    const std::string &name, int pairs, double slack,
    std::initializer_list<DelayLineSiteMeasurement::ResponsePoint> points) {
  DelayLineSiteMeasurement site = Site(name, pairs, slack);
  site.has_coefficient = false;
  site.response_points = points;
  return site;
}

TEST(DelayLineSizingPolicyTest, ResponseBatchChoosesSmallestCoveringPoint) {
  SizingLimits limits = Limits();
  limits.margin_ps = 0.0;
  const auto site = ResponseSite("dl3", 7, -340.0,
                                 {{10, 460.0}, {12, 760.0}});
  const BatchSizingDecision decision =
      DecideDelayLineResponseBatch({site}, limits, true);
  ASSERT_EQ(decision.outcome, SizingOutcome::kRequest) << decision.reason;
  ASSERT_EQ(decision.requests.size(), 1U);
  EXPECT_EQ(decision.requests[0].requested_pairs, 10);
  EXPECT_EQ(decision.expected_added_components, 6);
}

TEST(DelayLineSizingPolicyTest, ResponseBatchUsesLaterPointWhenNeeded) {
  SizingLimits limits = Limits();
  limits.margin_ps = 0.0;
  const auto site = ResponseSite("dl3", 7, -500.0,
                                 {{10, 460.0}, {12, 760.0}});
  const BatchSizingDecision decision =
      DecideDelayLineResponseBatch({site}, limits, true);
  ASSERT_EQ(decision.outcome, SizingOutcome::kRequest) << decision.reason;
  EXPECT_EQ(decision.requests[0].requested_pairs, 12);
}

TEST(DelayLineSizingPolicyTest, ResponseBatchRefusesExtrapolation) {
  SizingLimits limits = Limits();
  limits.margin_ps = 0.0;
  const auto site = ResponseSite("dl3", 7, -800.0,
                                 {{10, 460.0}, {12, 760.0}});
  const BatchSizingDecision decision =
      DecideDelayLineResponseBatch({site}, limits, true);
  EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid);
  EXPECT_NE(decision.reason.find("exceeds"), std::string::npos);
}

TEST(DelayLineSizingPolicyTest, ResponseEvidenceMustBeOrderedAndMonotone) {
  SizingLimits limits = Limits();
  limits.margin_ps = 0.0;
  for (const auto &site : {
           ResponseSite("dl3", 7, -300.0, {{12, 700.0}, {10, 800.0}}),
           ResponseSite("dl3", 7, -300.0, {{10, 700.0}, {12, 600.0}})}) {
    const BatchSizingDecision decision =
        DecideDelayLineResponseBatch({site}, limits, true);
    EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid);
    EXPECT_NE(decision.reason.find("non-monotone"), std::string::npos);
  }
}

TEST(DelayLineSizingPolicyTest, MissingResponseIsExplicitlyIneligible) {
  SizingLimits limits = Limits();
  limits.margin_ps = 0.0;
  const auto characterized =
      ResponseSite("dl0", 7, -100.0, {{10, 500.0}});
  const auto missing = ResponseSite("dl1", 95, -100.0, {});
  const BatchSizingDecision decision = DecideDelayLineResponseBatch(
      {characterized, missing}, limits, false);
  ASSERT_EQ(decision.outcome, SizingOutcome::kRequest) << decision.reason;
  ASSERT_EQ(decision.verdicts.size(), 2U);
  EXPECT_EQ(decision.verdicts[1].site, "dl1");
  EXPECT_EQ(decision.verdicts[1].outcome, SizingOutcome::kInvalid);
  EXPECT_NE(decision.verdicts[1].reason.find("no incremental response"),
            std::string::npos);
}

TEST(DelayLineSizingPolicyTest, ResponseBatchChecksAggregateHeadroom) {
  SizingLimits limits = Limits();
  limits.margin_ps = 0.0;
  limits.component_headroom = 10;
  const auto dl0 = ResponseSite("dl0", 7, -100.0, {{10, 500.0}});
  const auto dl2 = ResponseSite("dl2", 7, -100.0, {{10, 500.0}});
  const BatchSizingDecision decision =
      DecideDelayLineResponseBatch({dl0, dl2}, limits, true);
  EXPECT_EQ(decision.outcome, SizingOutcome::kInvalid);
  EXPECT_NE(decision.reason.find("headroom"), std::string::npos);
}

} // namespace
} // namespace dali
