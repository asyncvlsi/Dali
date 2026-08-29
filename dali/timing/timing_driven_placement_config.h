/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 *******************************************************************************/

/** @file Strict line-oriented configuration for the P2B controller boundary. */
#ifndef DALI_TIMING_TIMING_DRIVEN_PLACEMENT_CONFIG_H_
#define DALI_TIMING_TIMING_DRIVEN_PLACEMENT_CONFIG_H_

#include <string>
#include <vector>

#include "dali/timing/timing_driven_placement_controller.h"

namespace dali {

/** Controller-only data consumed by a Dali-owned candidate policy. */
struct TimingDrivenPlacementPolicyConfig {
  TimingDrivenPlacementControllerConfig controller;
  std::vector<std::string> delay_site_ids;
  std::vector<TimingDrivenPlacementCandidate> candidate_sequence;
};

/** Value-only settings consumed by the disposable interact lifecycle host. */
struct TimingDrivenPlacementLifecycleConfig {
  std::string liberty_path;
  std::string tech_config_path;
  std::string placement_recipe_path;
  bool timing_use_rc = false;
  int rc_min_routing_layer = 0;
  double target_density = 0.0;
  int num_threads = 0;
  bool is_standard_cell = false;
  int well_emit_mode = 0;
  bool enable_well_taps = false;
  TimingDrivenPlacementDieGrid fixed_die_grid;
};

/** Parsed policy plus lifecycle data; neither side selects the other. */
struct TimingDrivenPlacementConfig {
  TimingDrivenPlacementPolicyConfig policy;
  TimingDrivenPlacementLifecycleConfig lifecycle;
};

/**
 * Load the strict P2B configuration grammar.
 *
 * Each non-comment line is whitespace-separated as follows:
 *
 *   format_version 1
 *   policy.expected_constraint_count 512
 *   policy.required_slack_margin_ps 25
 *   policy.max_trials 4
 *   policy.no_improvement_limit 4
 *   policy.minimum_period_improvement_ps 0
 *   policy.feasibility_merit_tolerance_ps 0.000001
 *   policy.baseline_mode track_best_infeasible
 *   policy.initial_anchor seed
 *   policy.initial_generation_id seed-generation
 *   policy.require_measurement_metadata true
 *   policy.delay_site dl0
 *   policy.initial.parameter dl0 7
 *   policy.initial.replacement dl0 plain_delay<7>
 *   policy.candidate 1 parameter dl0 9
 *   policy.candidate 1 replacement dl0 plain_delay<9>
 *   lifecycle.liberty_path tech/lib/reference.lib
 *   lifecycle.tech_config_path tech/sky130_rcx.rules
 *   lifecycle.placement_recipe_path recipes/placement.dali
 *   lifecycle.timing_use_rc true
 *   lifecycle.rc_min_routing_layer 1
 *   lifecycle.target_density 0.70
 *   lifecycle.num_threads 1
 *   lifecycle.is_standard_cell false
 *   lifecycle.well_emit_mode 0
 *   lifecycle.enable_well_taps true
 *   lifecycle.fixed_die 0 0 100000 100000 600 300
 *
 * Blank lines and text after '#' are ignored. Values are single tokens; paths
 * may be absolute or relative to the configuration file's directory. Unknown,
 * duplicate, missing, or malformed keys are errors. Candidate indices start at
 * one and must be contiguous. The initial and every sequence candidate must
 * provide exactly one replacement for every declared delay site.
 */
bool LoadTimingDrivenPlacementConfig(const std::string &file_name,
                                     TimingDrivenPlacementConfig *config,
                                     std::string *error);

/** Write the same grammar, using absolute lifecycle paths, for round trips. */
bool WriteTimingDrivenPlacementConfig(const std::string &file_name,
                                      const TimingDrivenPlacementConfig &config,
                                      std::string *error);

} // namespace dali

#endif // DALI_TIMING_TIMING_DRIVEN_PLACEMENT_CONFIG_H_
