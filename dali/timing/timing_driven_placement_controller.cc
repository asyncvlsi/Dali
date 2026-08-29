#include "dali/timing/timing_driven_placement_controller.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <utility>

#include "dali/placer/well_legalizer/gridded_placement_validator.h"

namespace dali {

std::size_t TimingDrivenPlacementLegalitySummary::TotalViolationCount() const {
  return unassigned_component_count + duplicate_assignment_count +
         invalid_component_reference_count + row_boundary_violation_count +
         row_overlap_count + component_boundary_violation_count +
         component_overlap_count + component_y_violation_count +
         component_orientation_violation_count +
         physical_completion_violation_count;
}

bool TimingDrivenPlacementLegalitySummary::IsLegal() const {
  return TotalViolationCount() == 0 && missing_well_tap_count == 0 &&
         well_tap_geometry_violation_count == 0 &&
         well_tap_spacing_violation_count == 0 && missing_end_cap_count == 0 &&
         end_cap_geometry_violation_count == 0 &&
         end_cap_tap_overlap_count == 0 &&
         physical_component_count_violation_count == 0 &&
         well_tap_coverage_violation_count == 0;
}

TimingDrivenPlacementLegalitySummary ToTimingDrivenPlacementLegalitySummary(
    const GriddedPlacementLegalityReport &report) {
  TimingDrivenPlacementLegalitySummary summary;
  summary.movable_component_count = report.movable_component_count;
  summary.assigned_component_count = report.assigned_component_count;
  summary.unassigned_component_count = report.unassigned_component_count;
  summary.duplicate_assignment_count = report.duplicate_assignment_count;
  summary.invalid_component_reference_count =
      report.invalid_component_reference_count;
  summary.row_boundary_violation_count = report.row_boundary_violation_count;
  summary.row_overlap_count = report.row_overlap_count;
  summary.component_boundary_violation_count =
      report.component_boundary_violation_count;
  summary.component_overlap_count = report.component_overlap_count;
  summary.component_y_violation_count = report.component_y_violation_count;
  summary.component_orientation_violation_count =
      report.component_orientation_violation_count;
  summary.physical_completion_violation_count =
      report.physical_completion_violation_count;
  summary.missing_well_tap_count = report.missing_well_tap_count;
  summary.well_tap_geometry_violation_count =
      report.well_tap_geometry_violation_count;
  summary.well_tap_spacing_violation_count =
      report.well_tap_spacing_violation_count;
  summary.missing_end_cap_count = report.missing_end_cap_count;
  summary.end_cap_geometry_violation_count =
      report.end_cap_geometry_violation_count;
  summary.end_cap_tap_overlap_count = report.end_cap_tap_overlap_count;
  summary.physical_component_count_violation_count =
      report.physical_component_count_violation_count;
  summary.well_tap_coverage_violation_count =
      report.well_tap_coverage_violation_count;
  summary.max_well_tap_coverage_gap = report.max_well_tap_coverage_gap;
  return summary;
}

namespace timing_driven_placement_controller_detail {

struct FeasibilityMerit {
  double worst_deficit_ps = 0.0;
  double total_deficit_ps = 0.0;
  double period_ps = 0.0;
};

struct MeasurementValidation {
  bool valid = false;
  bool feasible = false;
  FeasibilityMerit merit;
  std::vector<int> constraint_ids;
  std::string constraint_id_digest;
  std::string reason;
};

bool IsFinite(double value) { return std::isfinite(value); }

bool WithinTolerance(double lhs, double rhs, double tolerance) {
  return std::abs(lhs - rhs) <= tolerance;
}

std::string ValueError(const std::string &field) {
  return "measurement has invalid " + field;
}

std::string ConstraintIdDigest(
    const std::vector<TimingDrivenConstraintMeasurement> &constraints,
    std::vector<int> *sorted_ids) {
  sorted_ids->clear();
  sorted_ids->reserve(constraints.size());
  for (const auto &constraint : constraints)
    sorted_ids->push_back(constraint.constraint_id);
  std::sort(sorted_ids->begin(), sorted_ids->end());
  std::ostringstream digest;
  digest << "ids:";
  for (const int id : *sorted_ids)
    digest << id << ',';
  return digest.str();
}

bool IsValidDieGrid(const TimingDrivenPlacementDieGrid &die_grid) {
  return die_grid.valid && IsFinite(die_grid.die_llx) &&
         IsFinite(die_grid.die_lly) && IsFinite(die_grid.die_urx) &&
         IsFinite(die_grid.die_ury) && IsFinite(die_grid.grid_x) &&
         IsFinite(die_grid.grid_y) && die_grid.die_urx > die_grid.die_llx &&
         die_grid.die_ury > die_grid.die_lly && die_grid.grid_x > 0.0 &&
         die_grid.grid_y > 0.0;
}

bool SameDieGrid(const TimingDrivenPlacementDieGrid &lhs,
                 const TimingDrivenPlacementDieGrid &rhs) {
  constexpr double kDieGridTolerance = 1e-9;
  return lhs.valid == rhs.valid &&
         WithinTolerance(lhs.die_llx, rhs.die_llx, kDieGridTolerance) &&
         WithinTolerance(lhs.die_lly, rhs.die_lly, kDieGridTolerance) &&
         WithinTolerance(lhs.die_urx, rhs.die_urx, kDieGridTolerance) &&
         WithinTolerance(lhs.die_ury, rhs.die_ury, kDieGridTolerance) &&
         WithinTolerance(lhs.grid_x, rhs.grid_x, kDieGridTolerance) &&
         WithinTolerance(lhs.grid_y, rhs.grid_y, kDieGridTolerance);
}

bool IsExpectedDelaySite(const std::string &site,
                         const TimingDrivenPlacementControllerConfig &config) {
  return std::find(config.expected_delay_site_ids.begin(),
                   config.expected_delay_site_ids.end(),
                   site) != config.expected_delay_site_ids.end();
}

std::string
ValidateCandidate(const TimingDrivenPlacementCandidate &candidate,
                  const TimingDrivenPlacementControllerConfig &config) {
  if (candidate.delay_parameters.empty()) {
    return "candidate has an empty delay-parameter map";
  }
  for (const auto &parameter : candidate.delay_parameters) {
    if (parameter.first.empty()) {
      return "candidate has an empty delay-parameter name";
    }
  }
  if (!config.require_replacement_map) {
    return {};
  }
  if (config.expected_delay_site_ids.empty()) {
    return "replacement-map validation has no declared delay sites";
  }
  if (candidate.delay_parameters.size() !=
      config.expected_delay_site_ids.size()) {
    return "candidate delay-parameter map is not a bijection with declared "
           "sites";
  }
  if (candidate.replacement_processes.size() !=
      config.expected_delay_site_ids.size()) {
    return "candidate replacement map is not a bijection with declared sites";
  }
  for (const std::string &site : config.expected_delay_site_ids) {
    const auto parameter = candidate.delay_parameters.find(site);
    if (parameter == candidate.delay_parameters.end()) {
      return "candidate delay-parameter map is missing declared site " + site;
    }
    if (parameter->second <= 0) {
      return "candidate delay parameter is not positive for " + site;
    }
    if (candidate.replacement_processes.find(site) ==
        candidate.replacement_processes.end()) {
      return "candidate replacement map is missing declared site " + site;
    }
    if (candidate.replacement_processes.at(site).empty()) {
      return "candidate replacement map has an empty process for " + site;
    }
  }
  for (const auto &parameter : candidate.delay_parameters) {
    if (!IsExpectedDelaySite(parameter.first, config)) {
      return "candidate delay-parameter map has undeclared site " +
             parameter.first;
    }
  }
  for (const auto &replacement : candidate.replacement_processes) {
    if (!IsExpectedDelaySite(replacement.first, config)) {
      return "candidate replacement map has undeclared site " +
             replacement.first;
    }
  }
  return {};
}

MeasurementValidation
ValidateMeasurement(const TimingDrivenPlacementMeasurement &measurement,
                    const TimingDrivenPlacementCandidate &candidate,
                    const TimingDrivenPlacementControllerConfig &config) {
  MeasurementValidation validation;
  if (!IsFinite(measurement.period_ps) || measurement.period_ps <= 0.0) {
    validation.reason = ValueError("period");
    return validation;
  }
  if (!IsFinite(measurement.wns_ps)) {
    validation.reason = ValueError("WNS");
    return validation;
  }
  if (!IsFinite(measurement.tns_ps)) {
    validation.reason = ValueError("TNS");
    return validation;
  }
  if (!IsFinite(measurement.placement_hpwl_um) ||
      measurement.placement_hpwl_um < 0.0) {
    validation.reason = ValueError("placement HPWL");
    return validation;
  }
  if (measurement.delay_parameters != candidate.delay_parameters ||
      measurement.replacement_processes != candidate.replacement_processes) {
    validation.reason =
        "measurement does not echo the requested candidate parameters";
    return validation;
  }
  if (measurement.artifact_id.empty()) {
    validation.reason = "measurement has an empty artifact id";
    return validation;
  }
  if (measurement.constraint_count != config.expected_constraint_count ||
      measurement.constraint_count < 0 ||
      measurement.constraints.size() !=
          static_cast<std::size_t>(measurement.constraint_count)) {
    validation.reason =
        "measurement constraint count does not match the configured count";
    return validation;
  }
  validation.constraint_id_digest =
      ConstraintIdDigest(measurement.constraints, &validation.constraint_ids);
  if (!measurement.constraint_id_digest.empty() &&
      measurement.constraint_id_digest != validation.constraint_id_digest) {
    validation.reason = "measurement constraint id digest is inconsistent";
    return validation;
  }
  for (std::size_t i = 0; i < measurement.constraints.size(); ++i) {
    const auto &constraint = measurement.constraints[i];
    if (!IsFinite(constraint.slack_ps)) {
      validation.reason = "measurement has non-finite constraint slack";
      return validation;
    }
    for (std::size_t j = 0; j < i; ++j) {
      if (measurement.constraints[j].constraint_id ==
          constraint.constraint_id) {
        validation.reason = "measurement contains duplicate constraint ids";
        return validation;
      }
    }
    const double deficit =
        std::max(config.required_slack_margin_ps - constraint.slack_ps, 0.0);
    validation.merit.worst_deficit_ps =
        std::max(validation.merit.worst_deficit_ps, deficit);
    validation.merit.total_deficit_ps += deficit;
  }
  if (!measurement.placement_legal) {
    validation.reason = "measurement reports illegal placement";
    return validation;
  }
  if (measurement.overlap_count != 0) {
    validation.reason = "measurement reports placement overlaps";
    return validation;
  }
  if (!measurement.legality.IsLegal()) {
    validation.reason = "measurement reports categorized placement violations";
    return validation;
  }
  const bool require_metadata = config.require_measurement_metadata ||
                                config.require_lifecycle_invariants;
  if (require_metadata && (measurement.topology_identity.empty() ||
                           measurement.static_inventory_digest.empty() ||
                           measurement.io_inventory_digest.empty() ||
                           measurement.constraint_id_digest.empty() ||
                           measurement.generation_id.empty() ||
                           measurement.artifact_parent_id.empty() ||
                           measurement.generation_parent_id.empty() ||
                           !IsValidDieGrid(measurement.die_grid))) {
    validation.reason = "measurement has incomplete artifact metadata";
    return validation;
  }
  if (config.require_lifecycle_invariants &&
      (!IsValidDieGrid(config.expected_die_grid) ||
       !SameDieGrid(measurement.die_grid, config.expected_die_grid) ||
       measurement.timing_use_rc != config.expected_timing_use_rc ||
       measurement.rc_min_routing_layer !=
           config.expected_rc_min_routing_layer)) {
    validation.reason = "measurement lifecycle metadata does not match config";
    return validation;
  }

  validation.merit.period_ps = measurement.period_ps;
  validation.feasible = validation.merit.worst_deficit_ps == 0.0;
  validation.valid = true;
  return validation;
}

bool ImprovesInfeasibility(
    const FeasibilityMerit &candidate, const FeasibilityMerit &current,
    const TimingDrivenPlacementControllerConfig &config) {
  const double tolerance = config.feasibility_merit_tolerance_ps;
  if (!WithinTolerance(candidate.worst_deficit_ps, current.worst_deficit_ps,
                       tolerance)) {
    return candidate.worst_deficit_ps < current.worst_deficit_ps;
  }
  if (!WithinTolerance(candidate.total_deficit_ps, current.total_deficit_ps,
                       tolerance)) {
    return candidate.total_deficit_ps < current.total_deficit_ps;
  }
  const double period_tolerance =
      std::max(config.minimum_period_improvement_ps, tolerance);
  return candidate.period_ps + period_tolerance < current.period_ps;
}

bool ImprovesPeriod(double candidate_period_ps, double current_period_ps,
                    const TimingDrivenPlacementControllerConfig &config) {
  return current_period_ps - candidate_period_ps >
         config.minimum_period_improvement_ps;
}

} // namespace timing_driven_placement_controller_detail

TimingDrivenPlacementController::TimingDrivenPlacementController(
    TimingDrivenPlacementControllerConfig config,
    std::unique_ptr<TimingDrivenCandidatePolicy> policy)
    : config_(std::move(config)), policy_(std::move(policy)) {}

TimingDrivenPlacementResult TimingDrivenPlacementController::Run(
    TimingDrivenFlowHost &host, TimingDrivenPlacementEventObserver *observer) {
  using namespace timing_driven_placement_controller_detail;

  history_.clear();
  TimingDrivenPlacementResult result;
  int last_recorded_trial_id = -1;
  bool observer_disabled = false;
  auto emit = [&](TimingDrivenPlacementEvent event) {
    if (observer == nullptr || observer_disabled)
      return;
    try {
      observer->OnTimingDrivenPlacementEvent(event);
    } catch (...) {
      observer_disabled = true;
    }
  };
  bool terminal_event_emitted = false;
  auto finish = [&]() {
    result.history = history_;
    if (!terminal_event_emitted) {
      TimingDrivenPlacementEvent event;
      event.type = TimingDrivenPlacementEventType::kTerminal;
      event.trial_id = last_recorded_trial_id;
      event.terminal_status = result.terminal_status;
      event.termination_reason = result.termination_reason;
      event.convergence_reason = result.convergence_reason;
      event.reason = "controller run finished";
      event.operation_succeeded =
          result.terminal_status == TimingDrivenTerminalStatus::kConverged;
      emit(event);
      terminal_event_emitted = true;
    }
    return result;
  };
  auto set_failure = [&](TimingDrivenTerminationReason reason) {
    result.terminal_status = TimingDrivenTerminalStatus::kFailed;
    result.termination_reason = reason;
    result.convergence_reason = TimingDrivenConvergenceReason::kNone;
  };
  bool has_feasible_state = false;
  auto set_convergence = [&](TimingDrivenTerminationReason reason,
                             TimingDrivenConvergenceReason convergence) {
    result.terminal_status = has_feasible_state
                                 ? TimingDrivenTerminalStatus::kConverged
                                 : TimingDrivenTerminalStatus::kFailed;
    result.termination_reason =
        has_feasible_state ? reason
                           : TimingDrivenTerminationReason::kNoFeasibleSolution;
    result.convergence_reason = convergence;
  };
  auto append = [&](TimingDrivenTrialRecord record) {
    last_recorded_trial_id = record.trial_id;
    history_.push_back(std::move(record));
  };
  auto rollback_and_append =
      [&](TimingDrivenTrialRecord record, TimingDrivenTrialDecision decision,
          const std::string &reason, bool terminate,
          TimingDrivenTerminationReason terminal_reason) {
        const bool rollback_ok = host.RollbackTrial();
        TimingDrivenPlacementEvent rollback_event;
        rollback_event.type = TimingDrivenPlacementEventType::kRollback;
        rollback_event.trial_id = record.trial_id;
        rollback_event.candidate = record.candidate;
        rollback_event.measurement = record.measurement;
        rollback_event.reason = reason;
        rollback_event.operation_succeeded = rollback_ok;
        rollback_event.decision = decision;
        emit(rollback_event);
        if (!rollback_ok) {
          record.decision = TimingDrivenTrialDecision::kFailed;
          record.reason = reason + "; rollback failed";
          append(std::move(record));
          set_failure(TimingDrivenTerminationReason::kRollbackFailure);
          return false;
        }
        record.decision = decision;
        record.reason = reason;
        append(std::move(record));
        if (terminate) {
          set_failure(terminal_reason);
          return false;
        }
        return true;
      };

  const bool require_metadata = config_.require_measurement_metadata ||
                                config_.require_lifecycle_invariants;

  bool duplicate_delay_site = false;
  for (std::size_t i = 0; i < config_.expected_delay_site_ids.size(); ++i) {
    for (std::size_t j = 0; j < i; ++j) {
      if (config_.expected_delay_site_ids[i] ==
          config_.expected_delay_site_ids[j]) {
        duplicate_delay_site = true;
      }
    }
  }
  if (!policy_ ||
      !ValidateCandidate(config_.initial_candidate, config_).empty() ||
      config_.initial_anchor.empty() ||
      config_.expected_constraint_count <= 0 || config_.max_trials <= 0 ||
      config_.no_improvement_limit <= 0 ||
      !IsFinite(config_.required_slack_margin_ps) ||
      config_.required_slack_margin_ps < 0.0 ||
      !IsFinite(config_.minimum_period_improvement_ps) ||
      config_.minimum_period_improvement_ps < 0.0 ||
      !IsFinite(config_.feasibility_merit_tolerance_ps) ||
      config_.feasibility_merit_tolerance_ps < 0.0 || duplicate_delay_site ||
      (require_metadata && config_.initial_generation_id.empty()) ||
      (config_.require_lifecycle_invariants &&
       (!IsValidDieGrid(config_.expected_die_grid) ||
        config_.expected_rc_min_routing_layer < 0)) ||
      (config_.require_replacement_map &&
       config_.expected_delay_site_ids.empty())) {
    set_failure(TimingDrivenTerminationReason::kValidationFailure);
    return finish();
  }

  TimingDrivenPlacementCandidate current_candidate = config_.initial_candidate;
  std::optional<TimingDrivenPlacementMeasurement> current_measurement;
  std::string current_anchor = config_.initial_anchor;
  std::optional<FeasibilityMerit> current_infeasibility_merit;
  std::optional<FeasibilityMerit> best_infeasibility_merit;
  std::vector<int> committed_constraint_ids;
  std::string committed_constraint_id_digest;
  std::string committed_static_inventory_digest;
  std::string committed_io_inventory_digest;
  std::string current_generation_id;
  std::set<std::string> seen_artifact_ids;
  std::set<std::string> seen_generation_ids;
  int no_improvement_count = 0;
  if (require_metadata) {
    seen_artifact_ids.insert(config_.initial_anchor);
    seen_generation_ids.insert(config_.initial_generation_id);
  }

  auto validateSeedLineage =
      [&](const TimingDrivenPlacementMeasurement &m) -> std::string {
    if (!require_metadata)
      return std::string();
    if (m.artifact_parent_id != current_anchor)
      return "baseline artifact parent does not match initial anchor";
    if (seen_artifact_ids.find(m.artifact_id) != seen_artifact_ids.end())
      return "baseline artifact id reuses an existing lineage root";
    if (seen_generation_ids.find(m.generation_id) != seen_generation_ids.end())
      return "baseline generation id reuses an existing lineage root";
    if (m.generation_parent_id != config_.initial_generation_id)
      return "baseline generation parent does not match initial generation";
    return std::string();
  };

  auto validateTrialLineage =
      [&](const TimingDrivenPlacementMeasurement &m) -> std::string {
    if (seen_artifact_ids.find(m.artifact_id) != seen_artifact_ids.end())
      return "measurement artifact id is not unique";
    if (require_metadata &&
        seen_generation_ids.find(m.generation_id) != seen_generation_ids.end())
      return "measurement generation id is not unique";
    if (require_metadata && m.artifact_parent_id != current_anchor)
      return "measurement artifact parent does not match committed anchor";
    if (require_metadata && m.generation_parent_id != current_generation_id)
      return "measurement generation parent does not match committed "
             "generation";
    if (require_metadata &&
        m.static_inventory_digest != committed_static_inventory_digest)
      return "measurement static inventory changed across candidates";
    if (require_metadata &&
        m.io_inventory_digest != committed_io_inventory_digest)
      return "measurement I/O inventory changed across candidates";
    return std::string();
  };

  auto validateConstraintIdentity =
      [&](const MeasurementValidation &validation) -> std::string {
    if (committed_constraint_ids.empty())
      return std::string();
    if (validation.constraint_ids != committed_constraint_ids)
      return "measurement constraint id set changed across candidates";
    if (validation.constraint_id_digest != committed_constraint_id_digest)
      return "measurement constraint id digest changed across candidates";
    return std::string();
  };

  auto update_best_infeasible =
      [&](const TimingDrivenPlacementCandidate &candidate,
          const TimingDrivenPlacementMeasurement &measurement,
          const FeasibilityMerit &merit) {
        if (!best_infeasibility_merit ||
            ImprovesInfeasibility(merit, *best_infeasibility_merit, config_)) {
          best_infeasibility_merit = merit;
          result.best_infeasible_candidate = candidate;
          result.best_infeasible_measurement = measurement;
          result.best_infeasible_artifact_id = measurement.artifact_id;
        }
      };
  auto update_current_state =
      [&](const TimingDrivenPlacementCandidate &candidate,
          const TimingDrivenPlacementMeasurement &measurement, bool feasible,
          const FeasibilityMerit &merit) {
        current_candidate = candidate;
        current_measurement = measurement;
        current_anchor = measurement.artifact_id;
        current_generation_id = measurement.generation_id;
        has_feasible_state = feasible;
        if (feasible) {
          current_infeasibility_merit.reset();
          result.accepted_candidate = candidate;
          result.accepted_measurement = measurement;
          result.accepted_artifact_id = measurement.artifact_id;
          result.provisional_candidate.reset();
          result.provisional_measurement.reset();
          result.provisional_artifact_id.clear();
        } else {
          current_infeasibility_merit = merit;
        }
      };

  int trial_id = 0;
  const TimingDrivenPlacementCandidate seed_candidate =
      config_.initial_candidate;
  const bool begin_ok = host.BeginTrial(seed_candidate, current_anchor);
  ++result.trials_attempted;
  TimingDrivenTrialRecord seed_record;
  seed_record.trial_id = trial_id;
  seed_record.candidate = seed_candidate;
  TimingDrivenPlacementEvent seed_begin_event;
  seed_begin_event.type = TimingDrivenPlacementEventType::kTrialBegin;
  seed_begin_event.trial_id = trial_id;
  seed_begin_event.candidate = seed_candidate;
  seed_begin_event.anchor = current_anchor;
  seed_begin_event.operation_succeeded = begin_ok;
  emit(seed_begin_event);
  if (!begin_ok) {
    rollback_and_append(std::move(seed_record),
                        TimingDrivenTrialDecision::kFailed,
                        "BeginTrial failed for baseline", true,
                        TimingDrivenTerminationReason::kHostFailure);
    return finish();
  }

  const std::optional<TimingDrivenPlacementMeasurement> seed_measurement =
      host.RunPlacementAndTiming();
  TimingDrivenPlacementEvent seed_measure_event;
  seed_measure_event.type = TimingDrivenPlacementEventType::kMeasured;
  seed_measure_event.trial_id = trial_id;
  seed_measure_event.candidate = seed_candidate;
  seed_measure_event.measurement = seed_measurement;
  seed_measure_event.operation_succeeded = seed_measurement.has_value();
  seed_measure_event.reason = seed_measurement.has_value()
                                  ? "baseline measured"
                                  : "baseline measurement failed";
  emit(seed_measure_event);
  if (!seed_measurement) {
    rollback_and_append(std::move(seed_record),
                        TimingDrivenTrialDecision::kFailed,
                        "RunPlacementAndTiming failed for baseline", true,
                        TimingDrivenTerminationReason::kHostFailure);
    return finish();
  }
  seed_record.measurement = seed_measurement;
  const MeasurementValidation seed_validation =
      ValidateMeasurement(*seed_measurement, seed_candidate, config_);
  if (!seed_validation.valid) {
    rollback_and_append(std::move(seed_record),
                        TimingDrivenTrialDecision::kFailed,
                        seed_validation.reason, true,
                        TimingDrivenTerminationReason::kValidationFailure);
    return finish();
  }
  const std::string seed_lineage_error = validateSeedLineage(*seed_measurement);
  if (!seed_lineage_error.empty()) {
    rollback_and_append(std::move(seed_record),
                        TimingDrivenTrialDecision::kFailed, seed_lineage_error,
                        true,
                        TimingDrivenTerminationReason::kValidationFailure);
    return finish();
  }
  committed_constraint_ids = seed_validation.constraint_ids;
  committed_constraint_id_digest = seed_validation.constraint_id_digest;
  committed_static_inventory_digest = seed_measurement->static_inventory_digest;
  committed_io_inventory_digest = seed_measurement->io_inventory_digest;
  seen_artifact_ids.insert(seed_measurement->artifact_id);
  if (require_metadata)
    seen_generation_ids.insert(seed_measurement->generation_id);
  if (!seed_validation.feasible) {
    update_best_infeasible(seed_candidate, *seed_measurement,
                           seed_validation.merit);
    if (config_.baseline_mode == TimingDrivenBaselineMode::kRequireFeasible) {
      rollback_and_append(
          std::move(seed_record), TimingDrivenTrialDecision::kFailed,
          "baseline does not meet the required slack margin", true,
          TimingDrivenTerminationReason::kInfeasibleBaseline);
      return finish();
    }
  }

  const bool seed_commit_ok = host.CommitTrial();
  TimingDrivenPlacementEvent seed_commit_event;
  seed_commit_event.type = TimingDrivenPlacementEventType::kCommit;
  seed_commit_event.trial_id = trial_id;
  seed_commit_event.candidate = seed_candidate;
  seed_commit_event.measurement = seed_measurement;
  seed_commit_event.operation_succeeded = seed_commit_ok;
  seed_commit_event.reason =
      seed_commit_ok ? "baseline committed" : "baseline commit failed";
  seed_commit_event.decision =
      seed_validation.feasible ? TimingDrivenTrialDecision::kAcceptedBaseline
                               : TimingDrivenTrialDecision::kBestInfeasible;
  emit(seed_commit_event);
  if (!seed_commit_ok) {
    if (!host.RollbackTrial()) {
      TimingDrivenPlacementEvent rollback_event;
      rollback_event.type = TimingDrivenPlacementEventType::kRollback;
      rollback_event.trial_id = trial_id;
      rollback_event.candidate = seed_candidate;
      rollback_event.measurement = seed_measurement;
      rollback_event.reason = "baseline commit failed";
      rollback_event.operation_succeeded = false;
      rollback_event.decision = TimingDrivenTrialDecision::kFailed;
      emit(rollback_event);
      seed_record.decision = TimingDrivenTrialDecision::kFailed;
      seed_record.reason = "baseline commit failed; rollback failed";
      append(std::move(seed_record));
      set_failure(TimingDrivenTerminationReason::kRollbackFailure);
      return finish();
    }
    TimingDrivenPlacementEvent rollback_event;
    rollback_event.type = TimingDrivenPlacementEventType::kRollback;
    rollback_event.trial_id = trial_id;
    rollback_event.candidate = seed_candidate;
    rollback_event.measurement = seed_measurement;
    rollback_event.reason = "baseline commit failed";
    rollback_event.operation_succeeded = true;
    rollback_event.decision = TimingDrivenTrialDecision::kFailed;
    emit(rollback_event);
    seed_record.decision = TimingDrivenTrialDecision::kFailed;
    seed_record.reason = "baseline commit failed";
    append(std::move(seed_record));
    set_failure(TimingDrivenTerminationReason::kCommitFailure);
    return finish();
  }

  if (seed_validation.feasible) {
    seed_record.decision = TimingDrivenTrialDecision::kAcceptedBaseline;
    seed_record.reason = "valid feasible baseline committed";
  } else {
    seed_record.decision = TimingDrivenTrialDecision::kBestInfeasible;
    seed_record.reason =
        "best infeasible baseline committed as provisional state";
  }
  append(std::move(seed_record));
  if (seed_validation.feasible) {
    update_current_state(seed_candidate, *seed_measurement, true,
                         seed_validation.merit);
  } else {
    current_candidate = seed_candidate;
    current_measurement = *seed_measurement;
    current_anchor = seed_measurement->artifact_id;
    current_generation_id = seed_measurement->generation_id;
    has_feasible_state = false;
    current_infeasibility_merit = seed_validation.merit;
    result.provisional_candidate = seed_candidate;
    result.provisional_measurement = *seed_measurement;
    result.provisional_artifact_id = seed_measurement->artifact_id;
  }

  while (true) {
    if (result.trials_attempted >= config_.max_trials) {
      set_convergence(TimingDrivenTerminationReason::kMaxTrials,
                      TimingDrivenConvergenceReason::kMaxTrials);
      return finish();
    }
    const std::optional<TimingDrivenPlacementCandidate> next_candidate =
        policy_->NextCandidate(current_candidate, current_measurement,
                               history_);
    if (!next_candidate) {
      set_convergence(TimingDrivenTerminationReason::kPolicyExhausted,
                      TimingDrivenConvergenceReason::kPolicyExhausted);
      return finish();
    }

    ++trial_id;
    TimingDrivenTrialRecord record;
    record.trial_id = trial_id;
    record.candidate = *next_candidate;
    const std::string candidate_error =
        ValidateCandidate(*next_candidate, config_);
    if (!candidate_error.empty()) {
      record.decision = TimingDrivenTrialDecision::kFailed;
      record.reason = candidate_error;
      append(std::move(record));
      set_failure(TimingDrivenTerminationReason::kValidationFailure);
      return finish();
    }
    const bool candidate_begin_ok =
        host.BeginTrial(*next_candidate, current_anchor);
    ++result.trials_attempted;
    TimingDrivenPlacementEvent begin_event;
    begin_event.type = TimingDrivenPlacementEventType::kTrialBegin;
    begin_event.trial_id = trial_id;
    begin_event.candidate = *next_candidate;
    begin_event.anchor = current_anchor;
    begin_event.operation_succeeded = candidate_begin_ok;
    emit(begin_event);
    if (!candidate_begin_ok) {
      rollback_and_append(std::move(record), TimingDrivenTrialDecision::kFailed,
                          "BeginTrial failed", true,
                          TimingDrivenTerminationReason::kHostFailure);
      return finish();
    }
    const std::optional<TimingDrivenPlacementMeasurement> measurement =
        host.RunPlacementAndTiming();
    TimingDrivenPlacementEvent measure_event;
    measure_event.type = TimingDrivenPlacementEventType::kMeasured;
    measure_event.trial_id = trial_id;
    measure_event.candidate = *next_candidate;
    measure_event.measurement = measurement;
    measure_event.operation_succeeded = measurement.has_value();
    measure_event.reason =
        measurement.has_value() ? "trial measured" : "trial measurement failed";
    emit(measure_event);
    if (!measurement) {
      rollback_and_append(std::move(record), TimingDrivenTrialDecision::kFailed,
                          "RunPlacementAndTiming failed", true,
                          TimingDrivenTerminationReason::kHostFailure);
      return finish();
    }
    record.measurement = measurement;
    const MeasurementValidation validation =
        ValidateMeasurement(*measurement, *next_candidate, config_);
    if (!validation.valid) {
      rollback_and_append(std::move(record), TimingDrivenTrialDecision::kFailed,
                          validation.reason, true,
                          TimingDrivenTerminationReason::kValidationFailure);
      return finish();
    }
    const std::string constraint_error = validateConstraintIdentity(validation);
    const std::string lineage_error = constraint_error.empty()
                                          ? validateTrialLineage(*measurement)
                                          : constraint_error;
    if (!lineage_error.empty()) {
      rollback_and_append(std::move(record), TimingDrivenTrialDecision::kFailed,
                          lineage_error, true,
                          TimingDrivenTerminationReason::kValidationFailure);
      return finish();
    }
    seen_artifact_ids.insert(measurement->artifact_id);
    if (require_metadata)
      seen_generation_ids.insert(measurement->generation_id);

    if (!validation.feasible) {
      update_best_infeasible(*next_candidate, *measurement, validation.merit);
    }

    bool accept = false;
    bool provisional_accept = false;
    std::string decision_reason;
    if (validation.feasible) {
      accept = !has_feasible_state ||
               ImprovesPeriod(measurement->period_ps,
                              current_measurement->period_ps, config_);
      decision_reason = has_feasible_state
                            ? "feasible candidate improves period"
                            : "feasible candidate supersedes provisional state";
    } else if (!has_feasible_state && current_infeasibility_merit &&
               ImprovesInfeasibility(validation.merit,
                                     *current_infeasibility_merit, config_)) {
      accept = true;
      provisional_accept = true;
      decision_reason = "candidate improves infeasibility merit";
    } else if (has_feasible_state) {
      decision_reason = "infeasible candidate cannot replace feasible state";
    } else {
      decision_reason = "candidate does not improve infeasibility merit";
    }

    if (!accept) {
      if (!rollback_and_append(std::move(record),
                               TimingDrivenTrialDecision::kRejected,
                               decision_reason, false,
                               TimingDrivenTerminationReason::kNoImprovement)) {
        return finish();
      }
      ++no_improvement_count;
      if (no_improvement_count >= config_.no_improvement_limit) {
        set_convergence(TimingDrivenTerminationReason::kNoImprovement,
                        TimingDrivenConvergenceReason::kNoImprovement);
        return finish();
      }
      continue;
    }

    const bool commit_ok = host.CommitTrial();
    TimingDrivenPlacementEvent commit_event;
    commit_event.type = TimingDrivenPlacementEventType::kCommit;
    commit_event.trial_id = trial_id;
    commit_event.candidate = *next_candidate;
    commit_event.measurement = measurement;
    commit_event.operation_succeeded = commit_ok;
    commit_event.reason = commit_ok ? decision_reason : "commit failed";
    commit_event.decision = provisional_accept
                                ? TimingDrivenTrialDecision::kBestInfeasible
                                : TimingDrivenTrialDecision::kAccepted;
    emit(commit_event);
    if (!commit_ok) {
      if (!host.RollbackTrial()) {
        TimingDrivenPlacementEvent rollback_event;
        rollback_event.type = TimingDrivenPlacementEventType::kRollback;
        rollback_event.trial_id = trial_id;
        rollback_event.candidate = *next_candidate;
        rollback_event.measurement = measurement;
        rollback_event.reason = "commit failed";
        rollback_event.operation_succeeded = false;
        rollback_event.decision = TimingDrivenTrialDecision::kFailed;
        emit(rollback_event);
        record.decision = TimingDrivenTrialDecision::kFailed;
        record.reason = "commit failed; rollback failed";
        append(std::move(record));
        set_failure(TimingDrivenTerminationReason::kRollbackFailure);
        return finish();
      }
      TimingDrivenPlacementEvent rollback_event;
      rollback_event.type = TimingDrivenPlacementEventType::kRollback;
      rollback_event.trial_id = trial_id;
      rollback_event.candidate = *next_candidate;
      rollback_event.measurement = measurement;
      rollback_event.reason = "commit failed";
      rollback_event.operation_succeeded = true;
      rollback_event.decision = TimingDrivenTrialDecision::kFailed;
      emit(rollback_event);
      record.decision = TimingDrivenTrialDecision::kFailed;
      record.reason = "commit failed";
      append(std::move(record));
      set_failure(TimingDrivenTerminationReason::kCommitFailure);
      return finish();
    }

    record.decision = provisional_accept
                          ? TimingDrivenTrialDecision::kBestInfeasible
                          : TimingDrivenTrialDecision::kAccepted;
    record.reason = decision_reason;
    append(std::move(record));
    update_current_state(*next_candidate, *measurement, validation.feasible,
                         validation.merit);
    if (!validation.feasible) {
      result.provisional_candidate = *next_candidate;
      result.provisional_measurement = *measurement;
      result.provisional_artifact_id = measurement->artifact_id;
      current_infeasibility_merit = validation.merit;
    }
    no_improvement_count = 0;
  }
}

} // namespace dali
