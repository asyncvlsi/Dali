#include "dali/timing/adaptive_delay_line_sizing.h"

#include <gtest/gtest.h>

namespace dali {
namespace {

AdaptiveDelayLineLimits Limits() {
  AdaptiveDelayLineLimits limits;
  limits.margin_ps = 25.0;
  limits.probe_pairs = 2;
  limits.max_step_pairs = 8;
  limits.max_added_pairs = 20;
  limits.max_pairs = 120;
  limits.max_nonpositive_probes = 1;
  limits.component_headroom = 100;
  limits.net_headroom = 100;
  return limits;
}

AdaptiveDelayLineSample Sample(const std::string &site, int pairs, double slack,
                               const std::string &identity = "path") {
  return {site, pairs, slack, true, identity};
}

AdaptiveDelayLineHistory History(const std::string &site, int initial,
                                 std::vector<AdaptiveDelayLineSample> samples) {
  return {site, initial, std::move(samples)};
}

TEST(AdaptiveDelayLineSizingTest, FirstShortSampleRequestsProbe) {
  const auto decision = DecideAdaptiveDelayLineSizing(
      {History("dl0", 7, {Sample("dl0", 7, -100.0)})}, Limits());
  ASSERT_EQ(decision.outcome, AdaptiveSizingOutcome::kRequest);
  ASSERT_EQ(decision.sites.size(), 1u);
  EXPECT_TRUE(decision.sites[0].is_probe);
  EXPECT_EQ(decision.sites[0].requested_pairs, 9);
}

TEST(AdaptiveDelayLineSizingTest, PositiveResponsePredictsBoundedStep) {
  const auto decision = DecideAdaptiveDelayLineSizing(
      {History("dl0", 7, {Sample("dl0", 7, -100.0), Sample("dl0", 9, -60.0)})},
      Limits());
  ASSERT_EQ(decision.outcome, AdaptiveSizingOutcome::kRequest);
  EXPECT_FALSE(decision.sites[0].is_probe);
  EXPECT_DOUBLE_EQ(decision.sites[0].measured_gain_ps_per_pair, 20.0);
  EXPECT_EQ(decision.sites[0].requested_pairs, 14);
}

TEST(AdaptiveDelayLineSizingTest, PredictionIsCappedPerEpoch) {
  const auto decision = DecideAdaptiveDelayLineSizing(
      {History("dl0", 7, {Sample("dl0", 7, -100.0), Sample("dl0", 9, -98.0)})},
      Limits());
  ASSERT_EQ(decision.outcome, AdaptiveSizingOutcome::kRequest);
  EXPECT_EQ(decision.sites[0].requested_pairs, 17);
}

TEST(AdaptiveDelayLineSizingTest, ChangedBindingPathForcesNewProbe) {
  const auto decision = DecideAdaptiveDelayLineSizing(
      {History("dl0", 7,
               {Sample("dl0", 7, -100.0, "a"), Sample("dl0", 9, -20.0, "b")})},
      Limits());
  ASSERT_EQ(decision.outcome, AdaptiveSizingOutcome::kRequest);
  EXPECT_TRUE(decision.sites[0].is_probe);
  EXPECT_EQ(decision.sites[0].requested_pairs, 11);
}

TEST(AdaptiveDelayLineSizingTest, OneNonpositiveProbeIsRetried) {
  const auto decision = DecideAdaptiveDelayLineSizing(
      {History("dl0", 7, {Sample("dl0", 7, -50.0), Sample("dl0", 9, -55.0)})},
      Limits());
  ASSERT_EQ(decision.outcome, AdaptiveSizingOutcome::kRequest);
  EXPECT_TRUE(decision.sites[0].is_probe);
}

TEST(AdaptiveDelayLineSizingTest, RepeatedNonpositiveResponseIsInvalid) {
  const auto decision = DecideAdaptiveDelayLineSizing(
      {History("dl0", 7,
               {Sample("dl0", 7, -50.0), Sample("dl0", 9, -55.0),
                Sample("dl0", 11, -60.0)})},
      Limits());
  EXPECT_EQ(decision.outcome, AdaptiveSizingOutcome::kInvalid);
}

TEST(AdaptiveDelayLineSizingTest, ClosedSitesProduceNoRequest) {
  const auto decision = DecideAdaptiveDelayLineSizing(
      {History("dl0", 7, {Sample("dl0", 7, 25.0)}),
       History("dl1", 7, {Sample("dl1", 7, 40.0)})},
      Limits());
  EXPECT_EQ(decision.outcome, AdaptiveSizingOutcome::kClosed);
  EXPECT_TRUE(decision.batch.IsEmpty());
}

TEST(AdaptiveDelayLineSizingTest, ClosedAndShortSitesAreHandledTogether) {
  const auto decision = DecideAdaptiveDelayLineSizing(
      {History("dl1", 7, {Sample("dl1", 7, -10.0)}),
       History("dl0", 7, {Sample("dl0", 7, 30.0)})},
      Limits());
  ASSERT_EQ(decision.outcome, AdaptiveSizingOutcome::kRequest);
  ASSERT_EQ(decision.batch.requests.size(), 1u);
  EXPECT_EQ(decision.batch.requests[0].site, "dl1");
}

TEST(AdaptiveDelayLineSizingTest, RegressedClosedSiteRequestsFreshProbe) {
  const auto decision = DecideAdaptiveDelayLineSizing(
      {History("dl0", 7,
               {Sample("dl0", 9, 30.0, "a"),
                Sample("dl0", 9, -10.0, "b")})},
      Limits());
  ASSERT_EQ(decision.outcome, AdaptiveSizingOutcome::kRequest);
  ASSERT_EQ(decision.sites.size(), 1u);
  EXPECT_TRUE(decision.sites[0].is_probe);
  EXPECT_EQ(decision.sites[0].requested_pairs, 11);
  EXPECT_EQ(decision.sites[0].reason,
            "previously closed site regressed; recalibrating with a small "
            "probe");
}

TEST(AdaptiveDelayLineSizingTest, UngrownViolatingSiteIsInvalid) {
  const auto decision = DecideAdaptiveDelayLineSizing(
      {History("dl0", 7,
               {Sample("dl0", 9, -20.0), Sample("dl0", 9, -10.0)})},
      Limits());
  EXPECT_EQ(decision.outcome, AdaptiveSizingOutcome::kInvalid);
  EXPECT_EQ(decision.reason,
            "site 'dl0' did not grow after a violating measurement");
}

TEST(AdaptiveDelayLineSizingTest, RequestsAreSortedAndGrowthIsExact) {
  const auto decision = DecideAdaptiveDelayLineSizing(
      {History("dl7", 7, {Sample("dl7", 7, -10.0)}),
       History("dl0", 7, {Sample("dl0", 7, -10.0)})},
      Limits());
  ASSERT_EQ(decision.batch.requests.size(), 2u);
  EXPECT_EQ(decision.batch.requests[0].site, "dl0");
  EXPECT_EQ(decision.batch.ExpectedAddedComponents(), 8);
  EXPECT_EQ(decision.batch.ExpectedAddedNets(), 8);
}

TEST(AdaptiveDelayLineSizingTest, MissingAttributionIsInvalid) {
  auto sample = Sample("dl0", 7, -10.0);
  sample.attributed = false;
  EXPECT_EQ(
      DecideAdaptiveDelayLineSizing({History("dl0", 7, {sample})}, Limits())
          .outcome,
      AdaptiveSizingOutcome::kInvalid);
}

TEST(AdaptiveDelayLineSizingTest, MissingSemanticIdentityIsInvalid) {
  EXPECT_EQ(DecideAdaptiveDelayLineSizing(
                {History("dl0", 7, {Sample("dl0", 7, -10.0, "")})}, Limits())
                .outcome,
            AdaptiveSizingOutcome::kInvalid);
}

TEST(AdaptiveDelayLineSizingTest, PerSiteGrowthBoundsRefuseInsteadOfTrim) {
  auto limits = Limits();
  limits.max_added_pairs = 3;
  const auto decision = DecideAdaptiveDelayLineSizing(
      {History("dl0", 7, {Sample("dl0", 7, -100.0), Sample("dl0", 9, -60.0)})},
      limits);
  EXPECT_EQ(decision.outcome, AdaptiveSizingOutcome::kInvalid);
}

TEST(AdaptiveDelayLineSizingTest, AggregateHeadroomIsChecked) {
  auto limits = Limits();
  limits.component_headroom = 6;
  limits.net_headroom = 6;
  const auto decision = DecideAdaptiveDelayLineSizing(
      {History("dl0", 7, {Sample("dl0", 7, -10.0)}),
       History("dl1", 7, {Sample("dl1", 7, -10.0)})},
      limits);
  EXPECT_EQ(decision.outcome, AdaptiveSizingOutcome::kInvalid);
}

TEST(AdaptiveDelayLineSizingTest, MalformedLimitsAndHistoriesAreInvalid) {
  auto limits = Limits();
  limits.probe_pairs = 0;
  EXPECT_EQ(DecideAdaptiveDelayLineSizing(
                {History("dl0", 7, {Sample("dl0", 7, -10.0)})}, limits)
                .outcome,
            AdaptiveSizingOutcome::kInvalid);
  EXPECT_EQ(DecideAdaptiveDelayLineSizing({}, Limits()).outcome,
            AdaptiveSizingOutcome::kInvalid);

  limits = Limits();
  limits.probe_pairs = limits.max_step_pairs + 1;
  EXPECT_EQ(DecideAdaptiveDelayLineSizing(
                {History("dl0", 7, {Sample("dl0", 7, -10.0)})}, limits)
                .outcome,
            AdaptiveSizingOutcome::kInvalid);
}

} // namespace
} // namespace dali
