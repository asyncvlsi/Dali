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

/** @file Placement-synchronized timing results consumed by Dali. */
#ifndef DALI_TIMING_TIMING_SNAPSHOT_H_
#define DALI_TIMING_TIMING_SNAPSHOT_H_

#include <string>
#include <utility>
#include <vector>

#include "dali/timing/delay_site_metadata.h"

namespace phydb {
class PhyDB;
struct PhydbPath;
} // namespace phydb

namespace dali {

struct TimingSnapshot;

/** One cell arc or net leg in a timing witness. */
struct TimingPathStep {
  TimingPathStep() = default;
  TimingPathStep(std::string source, std::string target, std::string net,
                 double edge_delay, std::string logical_source = {},
                 std::string logical_target = {}, std::string logical_net = {})
      : source_pin(std::move(source)), target_pin(std::move(target)),
        net_name(std::move(net)), delay(edge_delay),
        logical_source_pin(std::move(logical_source)),
        logical_target_pin(std::move(logical_target)),
        logical_net_name(std::move(logical_net)) {}

  std::string source_pin;
  std::string target_pin;
  std::string net_name;
  double delay = 0.0;
  std::string logical_source_pin;
  std::string logical_target_pin;
  std::string logical_net_name;
};

/** A timing witness represented without ACT or Cyclone pointers. */
struct TimingPathSnapshot {
  std::string root_pin;
  std::string terminal_pin;
  std::vector<TimingPathStep> steps;

  /** Return the sum of the delays reported for all witness steps. */
  double TotalDelay() const;
};

/** One relative-timing constraint and its current witnesses. */
struct RelativeTimingConstraintSnapshot {
  int constraint_id = -1;
  double slack = 0.0;
  /**
   * The fast end is a constant (driven by a cell with no inputs), so the
   * constraint can never be violated. Reported by the timer; its slack is
   * +inf and it is neither a violation nor unmeasured.
   */
  bool vacuous = false;
  TimingPathSnapshot fast_path;
  TimingPathSnapshot slow_path;
  std::vector<std::string> delay_repair_candidate_nets;
  std::vector<std::string> delay_repair_candidate_sites;

  /** Stable path endpoints independent of the timer's numeric constraint id. */
  std::string SemanticIdentity() const;
};

/**
 * Replace expanded cell endpoints inside replaceable sites with site identity.
 *
 * A parameterized site's terminal cell name changes when its size changes even
 * though the source-level endpoint does not. Endpoints outside a declared site
 * remain exact physical `component:pin` names. A collision after this
 * abstraction is rejected by the identity writer rather than hidden.
 */
void CanonicalizeReplaceableSiteEndpoints(
    TimingSnapshot *snapshot,
    const std::vector<std::string> &replaceable_site_prefixes);

// Preserve the original public name for callers that only consume violations.
using RelativeTimingViolationSnapshot = RelativeTimingConstraintSnapshot;

/** First-order relative-timing effect of changing one physical net. */
struct TimingNetCandidateSnapshot {
  std::string net_name;
  std::vector<int> improved_constraint_ids;
  std::vector<int> degraded_constraint_ids;
  double improved_negative_slack = 0.0;
  double degraded_negative_slack = 0.0;
  double improved_path_delay = 0.0;
  double degraded_path_delay = 0.0;
  std::vector<std::string> driver_pins;
  std::vector<std::string> load_pins;
  std::vector<std::string> logical_net_names;
  std::vector<std::string> logical_driver_pins;
  std::vector<std::string> logical_load_pins;
};

/**
 * Return slow-witness nets absent from the fast witness.
 *
 * Adding delay to a net shared by both witnesses cannot improve their delay
 * difference. The returned names preserve slow-path order and are unique.
 */
std::vector<std::string>
FindDelayRepairCandidateNets(const TimingPathSnapshot &fast_path,
                             const TimingPathSnapshot &slow_path);

/**
 * Return declared delay sites found on the slow witness but not the fast one.
 *
 * A delay meta component may have no independently named physical net, so a
 * net-only plan cannot identify it. Its declared logical-path prefix supplies
 * the stable repair identity instead of a timing implementation guessing from
 * instance-name spelling.
 */
std::vector<std::string> FindDelayRepairCandidateSites(
    const TimingPathSnapshot &fast_path, const TimingPathSnapshot &slow_path,
    const std::vector<DelayRepairSite> &declared_sites);

/** One declared site and the worst timing slack that calls for its adjustment.
 */
struct TimingRepairSitePlanItem {
  std::string id;
  std::string process_name;
  std::string instance_name;
  std::string parameter_name;
  int initial_parameter_value = 0;
  double worst_slack = 0.0;
};

/**
 * Build a deterministic, source-level repair plan from captured violations.
 *
 * Each declaration appears at most once. A plan item keeps the declaration's
 * original parameter and the most-negative related slack so the host can
 * apply the design-specific sizing rule without re-reading a JSON plan.
 */
std::vector<TimingRepairSitePlanItem>
BuildTimingRepairSitePlan(const TimingSnapshot &snapshot,
                          const std::vector<DelayRepairSite> &declared_sites);

/**
 * Rank slow-only nets from violated constraints against every known witness.
 *
 * Delay on a slow-only net increases relative slack; delay on a fast-only net
 * decreases it. Shared or absent nets have no first-order effect. The result is
 * advisory because the actual delay and post-edit timing must still be
 * measured.
 */
std::vector<TimingNetCandidateSnapshot> BuildDelayRepairCandidates(
    const std::vector<RelativeTimingConstraintSnapshot> &constraints);

/**
 * Rank fast-only nets whose physical delay can be reduced by placement.
 *
 * The result uses the same global constraint accounting as delay repair, but
 * reverses the path roles because shortening a fast-only net improves slack.
 */
std::vector<TimingNetCandidateSnapshot> BuildFastPathPlacementCandidates(
    const std::vector<RelativeTimingConstraintSnapshot> &constraints);

/** One physically mapped net leg on the critical cycle. */
struct CriticalCycleNetStep {
  std::string net_name;
  double delay = 0.0;
};

/** Timing state measured after synchronizing the current placement. */
/**
 * Worst slack over the constraints a declared delay site can repair.
 *
 * Reported for every declared site, including sites whose constraints all
 * pass. A sizing loop needs both directions: a site with surplus slack can be
 * made *smaller*, which shortens the period, and a plan listing only
 * violations can never say so. Slack is the minimum over the constraints where
 * this site lies on the slow path, so it is exactly the limit on how far the
 * site may shrink before something breaks.
 */
struct DelaySiteSlackSnapshot {
  std::string site_id;
  double worst_slack = 0.0;
  int constraint_count = 0;
  int violating_count = 0;
};

struct TimingSnapshot {
  bool has_critical_cycle = false;
  double critical_cycle_period = 0.0;
  int critical_cycle_unroll_factor = 0;
  std::vector<CriticalCycleNetStep> critical_cycle_nets;
  int relative_constraint_count = 0;
  int worst_relative_constraint_id = -1;
  double worst_relative_slack = 0.0;
  double relative_total_negative_slack = 0.0;
  /**
   * Constraints whose slack could not be measured at all -- an end of the fork
   * has no path, so the timer reports a non-finite slack rather than inventing
   * one. They are violations for reporting purposes (an unmeasured constraint
   * is not a satisfied one) but are kept out of the slack sums, which would
   * otherwise be -inf and useless as an optimisation target.
   */
  int relative_unmeasured_count = 0;
  /**
   * Constraints the timer proved vacuous: the fast end never transitions, so
   * "fast before slow" holds on every circuit. Counted and reported on their
   * own, never as violations and never as unmeasured.
   */
  int relative_vacuous_count = 0;
  std::vector<RelativeTimingViolationSnapshot> relative_violations;
  /**
   * Every relative constraint, violating or not.
   *
   * A controller that only ever sees violations can add delay but never take
   * it back, because a constraint that already passes carries the information
   * about how much it could give up. The witnesses are captured for every
   * constraint regardless, so keeping them costs only the retention.
   */
  std::vector<RelativeTimingViolationSnapshot> relative_constraints;
  std::vector<TimingNetCandidateSnapshot> delay_repair_candidates;
  std::vector<TimingNetCandidateSnapshot> fast_path_placement_candidates;
  std::vector<DelaySiteSlackSnapshot> delay_site_slack;
};

/**
 * How one witness path divides between intrinsic cell arcs and interconnect.
 *
 * The split is the timing graph's own, not a guess from names: a step carrying
 * a net name is a leg across that net, and a step without one is an arc inside
 * a cell. That is the same discriminator SlowOnlyRepairNets already uses to
 * decide which nets a delay element could lengthen.
 */
struct TimingPathDecomposition {
  double cell_delay = 0.0;
  double wire_delay = 0.0;
  double total_delay = 0.0;
  int cell_steps = 0;
  int wire_steps = 0;
  int total_steps = 0;
};

/** Whether a constraint names one replaceable site, none, or several. */
enum class ConstraintAttributionState { kUnattributed, kUnique, kAmbiguous };

const char *ToString(ConstraintAttributionState state);

/**
 * Value-only cell/wire decomposition of one relative-timing constraint.
 *
 * Exists to answer whether a negative slack is dominated by wire, which
 * placement can shorten, or by intrinsic logic, which it cannot. Carries no
 * pointer into the timer, the design or phyDB, so it survives re-elaboration
 * and can be serialized as evidence.
 */
struct TimingConstraintDecomposition {
  int constraint_id = -1;
  std::string semantic_identity;
  TimingPathDecomposition fast;
  TimingPathDecomposition slow;
  double slack = 0.0;
  /** `(slow_total - fast_total) - slack`, the accepted convention's residual. */
  double reconciliation_residual = 0.0;
  std::vector<std::string> candidate_sites;
  ConstraintAttributionState attribution = ConstraintAttributionState::kUnattributed;
  /** Set only when exactly one candidate site exists. */
  std::string unique_site;
};

/**
 * Reconciliation tolerance, in picoseconds.
 *
 * Gate 0 measured the worst floating-point residual between the timer's slack
 * and `slow_total - fast_total` at 4.60e-4 ps over the accepted 512-constraint
 * sample. This sits at roughly twice that, so accumulated rounding passes and a
 * real decomposition error does not.
 */
constexpr double kTimingDecompositionTolerancePs = 1e-3;

/**
 * Split one witness path by graph edge type.
 *
 * Fails on a non-finite or negative step delay rather than producing a total
 * that cannot be reconciled or serialized; a zero-delay step is legitimate and
 * keeps its classification.
 */
bool DecomposeTimingPath(const TimingPathSnapshot &path,
                         TimingPathDecomposition *decomposition,
                         std::string *error);

/** Convert one captured constraint into its value-only decomposition. */
bool DecomposeTimingConstraint(
    const RelativeTimingConstraintSnapshot &constraint,
    TimingConstraintDecomposition *decomposition, std::string *error);

/** Write a machine-readable delay-repair plan without changing the design. */
bool WriteTimingRepairPlanJson(const TimingSnapshot &snapshot,
                               const std::string &file_name);

/**
 * Write every constraint's cell/wire decomposition.
 *
 * Validates the whole snapshot before opening the destination, so a rejected
 * report leaves no partial file behind.
 */
bool WriteTimingDecompositionJson(const TimingSnapshot &snapshot,
                                  const std::string &file_name);

/** Write every relative constraint's stable endpoint identity and slack. */
bool WriteTimingConstraintIdentitiesJson(const TimingSnapshot &snapshot,
                                         const std::string &file_name);

/** Defined by the implementation; indexes pins to the nets carrying them. */
class PinNetIndex;

/** Convert timing results exposed by phyDB into Dali-owned records. */
class TimingSnapshotBuilder {
public:
  explicit TimingSnapshotBuilder(
      phydb::PhyDB *phy_db,
      const std::vector<DelayRepairSite> *declared_delay_sites = nullptr)
      : phy_db_(phy_db), declared_delay_sites_(declared_delay_sites) {}

  /** Capture timing results from the host's most recent timing analysis. */
  TimingSnapshot Capture() const;

  /** Capture only stable constraint endpoints and slacks, without witnesses. */
  TimingSnapshot CaptureConstraintIdentities() const;

  /**
   * Capture stable constraint endpoints without reading timing results.
   *
   * Used immediately after a topology rebuild, before the new cells have been
   * placed. Constraint identity is structural and is available as soon as the
   * timing graph is linked; asking for slack here would run a redundant timing
   * analysis on provisional geometry.
   */
  TimingSnapshot CaptureConstraintEndpointIdentities() const;

private:
  /** Convert one phyDB witness while resolving stable pin and net names. */
  TimingPathSnapshot CapturePath(phydb::PhydbPath &path,
                                 const PinNetIndex &pin_net_index) const;

  phydb::PhyDB *phy_db_ = nullptr;
  const std::vector<DelayRepairSite> *declared_delay_sites_ = nullptr;
};

} // namespace dali

#endif // DALI_TIMING_TIMING_SNAPSHOT_H_
