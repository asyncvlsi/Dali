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
#include "dali/timing/timing_net_weights.h"

#include <gtest/gtest.h>

using dali::CompoundTimingNetMultipliers;
using dali::ResetTimingNetMultipliers;

// A net promoted in two passes carries strength^2, and a net the second pass
// no longer names keeps the weight the first pass gave it -- the property
// whose absence made the feedback loop oscillate.
TEST(TimingNetWeightsTest, MultipliersCompoundAndPersist) {
  const std::vector<std::string> nets{"fast", "slow", "fixed", "idle"};
  std::vector<double> multipliers;

  auto first = CompoundTimingNetMultipliers(
      nets, {{"fast", 2}, {"slow", -1}, {"fixed", 1}}, 3.0, 100.0,
      multipliers);
  EXPECT_EQ(first.promoted, 2);
  EXPECT_EQ(first.relieved, 1);
  EXPECT_DOUBLE_EQ(multipliers[0], 3.0);
  EXPECT_DOUBLE_EQ(multipliers[1], 1.0 / 3.0);
  EXPECT_DOUBLE_EQ(multipliers[2], 3.0);
  EXPECT_DOUBLE_EQ(multipliers[3], 1.0);

  auto second = CompoundTimingNetMultipliers(
      nets, {{"fast", 1}, {"fixed", -1}}, 3.0, 100.0, multipliers);
  EXPECT_EQ(second.promoted, 1);
  EXPECT_EQ(second.relieved, 1);
  EXPECT_DOUBLE_EQ(multipliers[0], 9.0);
  EXPECT_DOUBLE_EQ(multipliers[1], 1.0 / 3.0);
  EXPECT_DOUBLE_EQ(multipliers[2], 1.0);  // relief undoes promotion
  EXPECT_DOUBLE_EQ(multipliers[3], 1.0);
}

TEST(TimingNetWeightsTest, ZeroDemandAndUnknownNetsChangeNothing) {
  const std::vector<std::string> nets{"a"};
  std::vector<double> multipliers{2.0};
  auto update = CompoundTimingNetMultipliers(nets, {{"a", 0}, {"b", 5}}, 3.0,
                                             8.0, multipliers);
  EXPECT_EQ(update.promoted, 0);
  EXPECT_EQ(update.relieved, 0);
  EXPECT_DOUBLE_EQ(multipliers[0], 2.0);
}

TEST(TimingNetWeightsTest, MultipliersStayWithinTheCapBothWays) {
  const std::vector<std::string> nets{"hot", "cold"};
  std::vector<double> multipliers;
  for (int pass = 0; pass < 3; ++pass) {
    CompoundTimingNetMultipliers(nets, {{"hot", 1}, {"cold", -1}}, 3.0, 8.0,
                                 multipliers);
  }
  EXPECT_DOUBLE_EQ(multipliers[0], 8.0);        // 27 uncapped
  EXPECT_DOUBLE_EQ(multipliers[1], 1.0 / 8.0);  // 1/27 uncapped
}

// A second run starts from base weights, except where something other than
// the timing loop changed a weight, which becomes the new base.
TEST(TimingNetWeightsTest, ResetReturnsToBaseAndAdoptsOutsideChanges) {
  std::vector<double> base{1.0, 2.0, 1.0};
  std::vector<double> multipliers{9.0, 3.0, 1.0};
  std::vector<double> weights{9.0, 6.0, 4.0};  // net 2 re-weighted outside
  ResetTimingNetMultipliers(weights, base, multipliers);
  EXPECT_EQ(weights, (std::vector<double>{1.0, 2.0, 4.0}));
  EXPECT_EQ(base, (std::vector<double>{1.0, 2.0, 4.0}));
  EXPECT_EQ(multipliers, (std::vector<double>{1.0, 1.0, 1.0}));
}

TEST(TimingNetWeightsTest, ResetAfterANetlistChangeStartsAFreshBase) {
  std::vector<double> base{1.0};
  std::vector<double> multipliers{3.0};
  std::vector<double> weights{3.0, 1.0};
  ResetTimingNetMultipliers(weights, base, multipliers);
  EXPECT_TRUE(base.empty());
  EXPECT_TRUE(multipliers.empty());
  EXPECT_EQ(weights, (std::vector<double>{3.0, 1.0}));
}

// Which measured pass ships: fewer violations beat less negative slack, and a
// pass that only ties an earlier one never displaces it.
TEST(TimingNetWeightsTest, BetterPassOrdersViolationsThenPeriodThenSlack) {
  using dali::IsBetterTimingPass;
  using dali::TimingPassMeasurement;
  const TimingPassMeasurement three_small{3, 0.0, -10.0, 100.0};
  const TimingPassMeasurement one_large{1, 0.0, -900.0, 100.0};
  EXPECT_TRUE(IsBetterTimingPass(one_large, three_small));
  EXPECT_FALSE(IsBetterTimingPass(three_small, one_large));
  const TimingPassMeasurement met{0, 0.0, 0.0, 100.0};
  const TimingPassMeasurement slow{0, 50.0, 0.0, 100.0};
  EXPECT_TRUE(IsBetterTimingPass(met, slow));
  const TimingPassMeasurement worse_slack{1, 0.0, -20.0, 100.0};
  const TimingPassMeasurement better_slack{1, 0.0, -5.0, 100.0};
  EXPECT_TRUE(IsBetterTimingPass(better_slack, worse_slack));
  EXPECT_FALSE(IsBetterTimingPass(met, met));
}

TEST(TimingNetWeightsTest, ConvergedNeedsNoViolationAndThePeriodMet) {
  EXPECT_TRUE(dali::IsConvergedTimingPass({0, 0.0, 0.0, 1.0}));
  EXPECT_FALSE(dali::IsConvergedTimingPass({1, 0.0, -1.0, 1.0}));
  EXPECT_FALSE(dali::IsConvergedTimingPass({0, 12.5, 0.0, 1.0}));
}

TEST(TimingNetWeightsTest, HpwlBudgetIsRelativeToTheFirstPass) {
  EXPECT_TRUE(dali::IsWithinHpwlBudget(1e9, 100.0, 0.0));
  EXPECT_TRUE(dali::IsWithinHpwlBudget(110.0, 100.0, 1.1));
  EXPECT_FALSE(dali::IsWithinHpwlBudget(110.1, 100.0, 1.1));
}

// A violated period target gives each critical-cycle net one unit of demand,
// counted once per net, and adds to what relative forks already gave it.
TEST(TimingNetWeightsTest, CriticalCycleDemandOnlyWhenThePeriodMisses) {
  std::unordered_map<std::string, int> demand{{"a", -1}};
  dali::AddCriticalCycleDemand({"a", "b", "b"}, 0.0, demand);
  EXPECT_EQ(demand, (std::unordered_map<std::string, int>{{"a", -1}}));
  dali::AddCriticalCycleDemand({"a", "b", "b"}, 7.0, demand);
  EXPECT_EQ(demand, (std::unordered_map<std::string, int>{{"a", 0}, {"b", 1}}));
}
