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
#ifndef DALI_TIMING_TIMING_NET_WEIGHTS_H_
#define DALI_TIMING_TIMING_NET_WEIGHTS_H_

#include <string>
#include <unordered_map>
#include <vector>

namespace dali {

struct TimingWeightUpdate {
  int promoted = 0;
  int relieved = 0;
};

/**
 * Compound one feedback pass's net demand into the per-net multipliers.
 *
 * `demand` maps a net name to its signed count of violated constraints a
 * shorter net would help (positive) or hurt (negative). A net with positive
 * demand has its multiplier multiplied by `strength`, one with negative
 * demand divided by it, and every other net keeps what earlier passes gave
 * it. `multipliers` is indexed like `net_names`.
 *
 * Every multiplier stays in [1/cap, cap]: bounded both ways and never
 * negative, so timing can reshape the wirelength objective but never invert
 * or erase it.
 * The lower bound is 1/cap rather than 1 by measurement. Flooring at 1
 * removes the relief of slow-only nets, and asymmetric_fork_join's worst slack
 * fell from +121.148 to +21.208 ps (still passing); with [1/cap, cap] it is
 * +121.148 again and every other runnable benchmark is unchanged.
 *
 * Compounding, not replacement, is the point. Replacing let a pass forget the
 * nets of the constraints it had just fixed, and they came back: on
 * asymmetric_fork_join_input at strength 3 the passes went 4 -> 4 -> 2
 * violations and the next placement shipped 4; compounded, three passes
 * measured 4 -> 4 -> 0.
 */
TimingWeightUpdate CompoundTimingNetMultipliers(
    const std::vector<std::string> &net_names,
    const std::unordered_map<std::string, int> &demand, double strength,
    double cap, std::vector<double> &multipliers);

/**
 * Take a run's timing multipliers back to one before its feedback passes.
 *
 * The multipliers compound within a run; without this they also compounded
 * across `run placement` calls in one session, so a second run started from
 * the first run's weights. `weights` are the nets' current weights, indexed
 * like `base` and `multipliers`. A net whose weight is still base * multiplier
 * goes back to its base; a net whose weight something else changed since (a
 * later `weight-critical-cycle`, say) keeps that weight as its new base. If
 * the sizes disagree (the netlist changed), `base` and `multipliers` are
 * cleared so the next update captures a fresh base.
 */
void ResetTimingNetMultipliers(std::vector<double> &weights,
                               std::vector<double> &base,
                               std::vector<double> &multipliers);

/** What one timing-feedback pass measured on its legal placement. */
struct TimingPassMeasurement {
  /** Relative-timing violations, unmeasured forks included. */
  int relative_violations = 0;
  /** How far the critical-cycle period exceeds its target, 0 when met. */
  double period_excess = 0.0;
  /** Sum of the measured negative relative slacks; zero or negative. */
  double total_negative_slack = 0.0;
  /** Unweighted HPWL of the measured placement. */
  double unweighted_hpwl = 0.0;
};

/**
 * Whether pass `a` is a better placement to ship than pass `b`.
 *
 * Fewer relative violations first, since signoff fails on any; then less
 * period excess; then less total negative slack. A tie keeps `b`, so the
 * earlier of two equal passes wins and a pass that reproduces an earlier one
 * never displaces it.
 */
bool IsBetterTimingPass(const TimingPassMeasurement &a,
                        const TimingPassMeasurement &b);

/**
 * Whether a pass leaves nothing for feedback to do: no relative violation and
 * the period target met. Its weights would not change, so a further pass
 * would reproduce it exactly.
 */
bool IsConvergedTimingPass(const TimingPassMeasurement &measurement);

/**
 * Whether a placement stays inside the HPWL budget: at most `budget` times
 * the first pass's HPWL. A budget of 0 or less means no budget.
 */
bool IsWithinHpwlBudget(double unweighted_hpwl, double first_pass_hpwl,
                        double budget);

/**
 * Add performance demand: when the critical-cycle period misses its target,
 * every net on the critical cycle gains one unit of demand, the same unit a
 * violated relative fork gives a fast-only net. Nets are counted once however
 * often they appear on the cycle.
 */
void AddCriticalCycleDemand(const std::vector<std::string> &critical_cycle_nets,
                            double period_excess,
                            std::unordered_map<std::string, int> &demand);

}  // namespace dali

#endif  // DALI_TIMING_TIMING_NET_WEIGHTS_H_
