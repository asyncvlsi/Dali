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

/** @file Transactional timing-driven placement policy and host boundary. */
#ifndef DALI_TIMING_TIMING_DRIVEN_PLACEMENT_CONTROLLER_H_
#define DALI_TIMING_TIMING_DRIVEN_PLACEMENT_CONTROLLER_H_

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dali {

struct GriddedPlacementLegalityReport;

struct TimingDrivenPlacementCandidate {
  std::map<std::string, int> delay_parameters;
  std::map<std::string, std::string> replacement_processes;
};

struct TimingDrivenConstraintMeasurement {
  int constraint_id = -1;
  std::string site_id;
  double slack_ps = 0.0;
};

/** Fixed placement geometry that every candidate in one run must echo. */
struct TimingDrivenPlacementDieGrid {
  double die_llx = 0.0;
  double die_lly = 0.0;
  double die_urx = 0.0;
  double die_ury = 0.0;
  double grid_x = 0.0;
  double grid_y = 0.0;
  bool valid = false;
};

/** Value copy of every counter in GriddedPlacementLegalityReport. */
struct TimingDrivenPlacementLegalitySummary {
  std::size_t movable_component_count = 0;
  std::size_t assigned_component_count = 0;
  std::size_t unassigned_component_count = 0;
  std::size_t duplicate_assignment_count = 0;
  std::size_t invalid_component_reference_count = 0;
  std::size_t row_boundary_violation_count = 0;
  std::size_t row_overlap_count = 0;
  std::size_t component_boundary_violation_count = 0;
  std::size_t component_overlap_count = 0;
  std::size_t component_y_violation_count = 0;
  std::size_t component_orientation_violation_count = 0;
  std::size_t physical_completion_violation_count = 0;
  std::size_t missing_well_tap_count = 0;
  std::size_t well_tap_geometry_violation_count = 0;
  std::size_t well_tap_spacing_violation_count = 0;
  std::size_t missing_end_cap_count = 0;
  std::size_t end_cap_geometry_violation_count = 0;
  std::size_t end_cap_tap_overlap_count = 0;
  std::size_t physical_component_count_violation_count = 0;
  std::size_t well_tap_coverage_violation_count = 0;
  double max_well_tap_coverage_gap = 0.0;

  /** Return structural violations plus the aggregate physical count. */
  std::size_t TotalViolationCount() const;
  /** Return true only when aggregate and diagnostic counters are zero. */
  bool IsLegal() const;
};

/** Convert the typed gridded legality report into a value-only summary. */
TimingDrivenPlacementLegalitySummary ToTimingDrivenPlacementLegalitySummary(
    const GriddedPlacementLegalityReport &report);

/**
 * Host-reported timing and placement results for one committed or trial state.
 * The WNS/TNS fields are summaries supplied by the host; controller feasibility
 * and infeasibility merit are recomputed from every constraint slack in the
 * complete vector, so those summaries cannot authorize acceptance.
 */
struct TimingDrivenPlacementMeasurement {
  std::map<std::string, int> delay_parameters;
  std::map<std::string, std::string> replacement_processes;
  double period_ps = 0.0;
  double wns_ps = 0.0;
  double tns_ps = 0.0;
  int constraint_count = 0;
  std::vector<TimingDrivenConstraintMeasurement> constraints;
  double placement_hpwl_um = 0.0;
  bool placement_legal = false;
  int overlap_count = -1;
  std::string artifact_id;
  TimingDrivenPlacementLegalitySummary legality;
  TimingDrivenPlacementDieGrid die_grid;
  bool timing_use_rc = false;
  int rc_min_routing_layer = 0;
  std::string topology_identity;
  std::string static_inventory_digest;
  std::string io_inventory_digest;
  std::string constraint_id_digest;
  std::string generation_id;
  /** Artifact id of the committed anchor from which this measurement began. */
  std::string artifact_parent_id;
  /** Generation id of the committed anchor from which this measurement began.
   */
  std::string generation_parent_id;
};

/**
 * Detailed immutable evidence stored separately from compact controller
 * measurements and referenced by artifact_id.
 */
struct TimingDrivenPlacementArtifactManifest {
  std::string artifact_id;
  std::string generation_id;
  std::string artifact_parent_id;
  std::string generation_parent_id;
  TimingDrivenPlacementCandidate candidate;
  std::vector<std::string> component_names;
  std::vector<std::string> static_component_names;
  std::vector<std::string> io_pin_names;
  std::string component_inventory_digest;
  std::string static_inventory_digest;
  std::string io_inventory_digest;
  std::string placement_status_digest;
  std::string constraint_id_digest;
  bool timing_use_rc = false;
  int rc_min_routing_layer = 0;
  TimingDrivenPlacementDieGrid die_grid;
  double period_ps = 0.0;
  double wns_ps = 0.0;
  double tns_ps = 0.0;
  std::vector<TimingDrivenConstraintMeasurement> constraints;
  double placement_hpwl_um = 0.0;
  TimingDrivenPlacementLegalitySummary legality;
  std::string anchor_path;
  std::string anchor_digest;
  std::string export_digest;
};

enum class TimingDrivenTrialDecision {
  kAcceptedBaseline,
  kBestInfeasible,
  kAccepted,
  kRejected,
  kFailed,
};

struct TimingDrivenTrialRecord {
  int trial_id = -1;
  TimingDrivenPlacementCandidate candidate;
  std::optional<TimingDrivenPlacementMeasurement> measurement;
  TimingDrivenTrialDecision decision = TimingDrivenTrialDecision::kFailed;
  std::string reason;
};

enum class TimingDrivenTerminalStatus {
  kConverged,
  kFailed,
};

enum class TimingDrivenTerminationReason {
  kPolicyExhausted,
  kNoImprovement,
  kMaxTrials,
  kNoFeasibleSolution,
  kInfeasibleBaseline,
  kValidationFailure,
  kHostFailure,
  kCommitFailure,
  kRollbackFailure,
};

enum class TimingDrivenConvergenceReason {
  kNone,
  kPolicyExhausted,
  kNoImprovement,
  kMaxTrials,
};

enum class TimingDrivenBaselineMode {
  /** The seed must be feasible before the policy may run. */
  kRequireFeasible,
  /** A valid infeasible seed may be committed as a provisional anchor. */
  kTrackBestInfeasible,
};

struct TimingDrivenPlacementResult {
  TimingDrivenTerminalStatus terminal_status =
      TimingDrivenTerminalStatus::kFailed;
  TimingDrivenTerminationReason termination_reason =
      TimingDrivenTerminationReason::kValidationFailure;
  TimingDrivenConvergenceReason convergence_reason =
      TimingDrivenConvergenceReason::kNone;
  std::optional<TimingDrivenPlacementCandidate> accepted_candidate;
  std::optional<TimingDrivenPlacementMeasurement> accepted_measurement;
  std::string accepted_artifact_id;
  std::optional<TimingDrivenPlacementCandidate> best_infeasible_candidate;
  std::optional<TimingDrivenPlacementMeasurement> best_infeasible_measurement;
  std::string best_infeasible_artifact_id;
  std::optional<TimingDrivenPlacementCandidate> provisional_candidate;
  std::optional<TimingDrivenPlacementMeasurement> provisional_measurement;
  std::string provisional_artifact_id;
  int trials_attempted = 0;
  std::vector<TimingDrivenTrialRecord> history;
};

struct TimingDrivenPlacementControllerConfig {
  TimingDrivenPlacementCandidate initial_candidate;
  std::string initial_anchor = "seed";
  int expected_constraint_count = 0;
  double required_slack_margin_ps = 0.0;
  int max_trials = 1;
  int no_improvement_limit = 1;
  double minimum_period_improvement_ps = 0.0;
  double feasibility_merit_tolerance_ps = 1e-9;
  std::string initial_generation_id = "seed-generation";
  bool require_lifecycle_invariants = false;
  TimingDrivenPlacementDieGrid expected_die_grid;
  bool expected_timing_use_rc = false;
  int expected_rc_min_routing_layer = 0;
  bool require_replacement_map = false;
  std::vector<std::string> expected_delay_site_ids;
  bool require_measurement_metadata = false;
  TimingDrivenBaselineMode baseline_mode =
      TimingDrivenBaselineMode::kRequireFeasible;
};

enum class TimingDrivenPlacementEventType {
  kTrialBegin,
  kMeasured,
  kCommit,
  kRollback,
  kTerminal,
};

/** Value-only event emitted synchronously at controller transaction boundaries.
 */
struct TimingDrivenPlacementEvent {
  TimingDrivenPlacementEventType type =
      TimingDrivenPlacementEventType::kTrialBegin;
  int trial_id = -1;
  TimingDrivenPlacementCandidate candidate;
  std::optional<TimingDrivenPlacementMeasurement> measurement;
  std::string anchor;
  std::string reason;
  bool operation_succeeded = false;
  TimingDrivenTrialDecision decision = TimingDrivenTrialDecision::kFailed;
  TimingDrivenTerminalStatus terminal_status =
      TimingDrivenTerminalStatus::kFailed;
  TimingDrivenTerminationReason termination_reason =
      TimingDrivenTerminationReason::kValidationFailure;
  TimingDrivenConvergenceReason convergence_reason =
      TimingDrivenConvergenceReason::kNone;
};

/**
 * Observes controller events without owning policy, host, or borrowed state.
 * Implementations must not throw. The controller catches and suppresses an
 * observer exception, then disables that observer for the rest of the run so
 * notifications cannot interrupt transaction ownership.
 */
class TimingDrivenPlacementEventObserver {
public:
  virtual ~TimingDrivenPlacementEventObserver() = default;
  virtual void
  OnTimingDrivenPlacementEvent(const TimingDrivenPlacementEvent &event) = 0;
};

/**
 * Host operations for one controller-owned trial.
 *
 * BeginTrial must establish a rollback-safe transaction before returning. A
 * false return may therefore follow partial host mutation; the controller
 * always calls RollbackTrial exactly once after every BeginTrial call that does
 * not reach CommitTrial. RollbackTrial must be safe and idempotent, including
 * after a failed BeginTrial. The host must not retain Dali or PhyDB pointers
 * across ACT re-elaboration; each measurement is an immutable value copy.
 */
class TimingDrivenFlowHost {
public:
  virtual ~TimingDrivenFlowHost() = default;

  virtual bool BeginTrial(const TimingDrivenPlacementCandidate &candidate,
                          const std::string &committed_anchor) = 0;
  virtual std::optional<TimingDrivenPlacementMeasurement>
  RunPlacementAndTiming() = 0;
  virtual bool CommitTrial() = 0;
  virtual bool RollbackTrial() = 0;
};

/** Controller-owned candidate generation policy. */
class TimingDrivenCandidatePolicy {
public:
  virtual ~TimingDrivenCandidatePolicy() = default;

  /**
   * Propose one candidate from value copies of the current committed state and
   * history. Before feasibility, the current state is provisional rather than
   * accepted. Returning nullopt is normal convergence only after a feasible
   * state exists; before feasibility it yields kFailed/kNoFeasibleSolution
   * while preserving the underlying convergence reason and diagnostics.
   */
  virtual std::optional<TimingDrivenPlacementCandidate> NextCandidate(
      TimingDrivenPlacementCandidate current_candidate,
      std::optional<TimingDrivenPlacementMeasurement> current_measurement,
      std::vector<TimingDrivenTrialRecord> history) = 0;
};

/** Owns the complete timing-driven transaction loop and its event history. */
class TimingDrivenPlacementController {
public:
  TimingDrivenPlacementController(
      TimingDrivenPlacementControllerConfig config,
      std::unique_ptr<TimingDrivenCandidatePolicy> policy);

  TimingDrivenPlacementResult
  Run(TimingDrivenFlowHost &host,
      TimingDrivenPlacementEventObserver *observer = nullptr);

  const std::vector<TimingDrivenTrialRecord> &History() const {
    return history_;
  }

private:
  TimingDrivenPlacementControllerConfig config_;
  std::unique_ptr<TimingDrivenCandidatePolicy> policy_;
  std::vector<TimingDrivenTrialRecord> history_;
};

} // namespace dali

#endif // DALI_TIMING_TIMING_DRIVEN_PLACEMENT_CONTROLLER_H_
