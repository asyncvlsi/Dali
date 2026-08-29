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

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace dali {

TimingWeightUpdate CompoundTimingNetMultipliers(
    const std::vector<std::string> &net_names,
    const std::unordered_map<std::string, int> &demand, double strength,
    double cap, std::vector<double> &multipliers) {
  TimingWeightUpdate update;
  multipliers.resize(net_names.size(), 1.0);
  for (std::size_t index = 0; index < net_names.size(); ++index) {
    auto found = demand.find(net_names[index]);
    if (found == demand.end() || found->second == 0) continue;
    if (found->second > 0) {
      multipliers[index] = std::min(cap, multipliers[index] * strength);
      ++update.promoted;
    } else {
      multipliers[index] = std::max(1.0 / cap, multipliers[index] / strength);
      ++update.relieved;
    }
  }
  return update;
}

void ResetTimingNetMultipliers(std::vector<double> &weights,
                               std::vector<double> &base,
                               std::vector<double> &multipliers) {
  if (base.size() != weights.size() || multipliers.size() != weights.size()) {
    base.clear();
    multipliers.clear();
    return;
  }
  for (std::size_t index = 0; index < weights.size(); ++index) {
    const double ours = base[index] * multipliers[index];
    if (std::abs(weights[index] - ours) <= 1e-9 * std::max(1.0, ours)) {
      weights[index] = base[index];
    } else {
      base[index] = weights[index];
    }
    multipliers[index] = 1.0;
  }
}

bool IsBetterTimingPass(const TimingPassMeasurement &a,
                        const TimingPassMeasurement &b) {
  if (a.relative_violations != b.relative_violations) {
    return a.relative_violations < b.relative_violations;
  }
  if (a.period_excess != b.period_excess) {
    return a.period_excess < b.period_excess;
  }
  return a.total_negative_slack > b.total_negative_slack;
}

bool IsConvergedTimingPass(const TimingPassMeasurement &measurement) {
  return measurement.relative_violations == 0 &&
         measurement.period_excess <= 0.0;
}

bool IsWithinHpwlBudget(double unweighted_hpwl, double first_pass_hpwl,
                        double budget) {
  return budget <= 0.0 || unweighted_hpwl <= budget * first_pass_hpwl;
}

void AddCriticalCycleDemand(const std::vector<std::string> &critical_cycle_nets,
                            double period_excess,
                            std::unordered_map<std::string, int> &demand) {
  if (period_excess <= 0.0) return;
  std::unordered_set<std::string> counted;
  for (const std::string &net : critical_cycle_nets) {
    if (!net.empty() && counted.insert(net).second) demand[net] += 1;
  }
}

}  // namespace dali
