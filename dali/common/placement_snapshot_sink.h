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
#ifndef DALI_COMMON_PLACEMENT_SNAPSHOT_SINK_H_
#define DALI_COMMON_PLACEMENT_SNAPSHOT_SINK_H_

#include <string>
#include <utility>
#include <vector>

namespace dali {

class Circuit;

/** N/P-well layer carried by a placement snapshot. */
enum class PlacementWellLayer {
  kPwell,
  kNwell,
  kPplus,
  kNplus,
};

/** One well or implant rectangle in micron coordinates for visualization. */
struct PlacementWellRect {
  float lx = 0;
  float ly = 0;
  float ux = 0;
  float uy = 0;
  PlacementWellLayer layer = PlacementWellLayer::kPwell;
};

/** One placement stage expected to produce snapshots this run. */
struct PlacementSnapshotStage {
  std::string group;  // snapshot group id, e.g. "global_placement"
  std::string title;  // human-readable chart title, e.g. "Global placement"
};

/** Ordered delay-line membership and connectivity carried by one snapshot. */
struct PlacementDelayLineVisualization {
  std::string name;
  std::vector<int> component_ids;
  std::vector<std::pair<int, int>> component_edges;
};

/** One Dali-decided delay-site growth carried by topology snapshots. */
struct PlacementTopologySiteChange {
  std::string site;
  int current_pairs = 0;
  int requested_pairs = 0;
  double boundary_slack_ps = 0.0;
  bool has_boundary_slack = false;
};

/** One witness edge, resolved to component ids while the circuit was alive. */
struct PlacementPathEdge {
  int from_component_id = -1;
  int to_component_id = -1;
  std::string net_name;
};

/**
 * One constraint's witnesses, resolved for drawing.
 *
 * Copied values only. The numeric id is the timer's diagnostic handle and is
 * not stable across an ACT re-elaboration; `semantic_identity` is the key that
 * is, and it is carried so a consumer can tell the same constraint from a
 * renumbered one without ever matching on the number.
 */
struct PlacementTimingPathVisualization {
  int constraint_id = -1;
  std::string semantic_identity;
  /** Empty when no registered site owns this constraint. */
  std::string attributed_delay_line;
  /** True when more than one registered site matched; it owns no line. */
  bool ambiguous_attribution = false;
  std::vector<std::string> candidate_delay_lines;
  double slack_ps = 0.0;
  double fast_delay_ps = 0.0;
  double slow_delay_ps = 0.0;
  /** False when the witnesses were empty or no pin resolved to a component. */
  bool has_geometry = false;
  std::vector<int> fast_only_component_ids;
  std::vector<int> slow_only_component_ids;
  std::vector<int> common_component_ids;
  std::vector<PlacementPathEdge> fast_only_edges;
  std::vector<PlacementPathEdge> slow_only_edges;
  std::vector<PlacementPathEdge> common_edges;
  int root_component_id = -1;
  int fast_terminal_component_id = -1;
  int slow_terminal_component_id = -1;
};

/** Every constraint one registered delay line owns, worst first. */
struct PlacementDelayLineTimingVisualization {
  std::string delay_line_name;
  /** Sorted by slack ascending, so the worst constraint is first. */
  std::vector<PlacementTimingPathVisualization> constraints;
  int worst_constraint_id = -1;
  double worst_slack_ps = 0.0;
};

/**
 * What Dali decided for one site, carried forward so a later frame can show it.
 *
 * Copied at the moment each part becomes known: the request carries the
 * boundary evidence, the applied delta carries what ACT actually returned, and
 * the final frame carries the closing slack. A field that is genuinely not
 * known yet stays unset rather than being inferred.
 */
struct PlacementSizingDecisionEvidence {
  std::string site;
  std::string decision_point;
  double boundary_slack_ps = 0.0;
  bool has_boundary_slack = false;
  int current_pairs = 0;
  int requested_pairs = 0;
  double measured_response_ps = 0.0;
  bool has_measured_response = false;
  int expected_added_components = 0;
  int expected_added_nets = 0;
  /**
   * Applied growth for this site alone, from the added cells that carry its
   * name. -1 until the mutation has happened.
   */
  int actual_added_components = -1;
  int actual_added_nets = -1;
  /**
   * The batch as a whole. Kept separate from the per-site numbers because the
   * scopes are different and mixing them misleads: every rewired net joins two
   * neighbouring sites -- `c2` is both dl1's and dl2's -- so there is no
   * structured evidence assigning a rewire to one site, and a line reading
   * "+6 cells / +6 nets, 8 rewired" invites reading 8 as this site's.
   */
  int batch_added_components = -1;
  int batch_added_nets = -1;
  int batch_retired_nets = -1;
  int batch_rewired_nets = -1;
  int final_pairs = -1;
  double final_slack_ps = 0.0;
  bool has_final_slack = false;
  bool closed = false;
};

/** Run-level metadata shared by placement snapshot consumers. */
struct PlacementSnapshotRunMetadata {
  std::string design_name;
  int database_microns = 0;
  std::string git_commit;
  bool pause_at_every_snapshot = true;
  /** Stages that will run this configuration, in execution order. Consumers
   *  (e.g. the live GUI) can reserve a chart slot per stage up front. */
  std::vector<PlacementSnapshotStage> stages;
};

/** Checkpoint metadata for one placement state. */
struct PlacementSnapshotMetadata {
  std::string id;
  std::string label;
  std::string group;
  std::string subgroup;
  int iteration = -1;
  bool is_delay_line_feedback = false;
  std::string delay_line_name;
  std::vector<int> delay_line_constraint_ids;
  double delay_line_binding_slack_ps = 0.0;
  int delay_line_old_separation = -1;
  int delay_line_new_separation = -1;
  double delay_line_measured_gain_ps_per_row = 0.0;
  std::vector<PlacementDelayLineVisualization> delay_lines;
  int topology_generation = 0;
  std::vector<PlacementTopologySiteChange> topology_changes;
  std::vector<int> topology_added_component_ids;
  std::vector<PlacementWellRect> well_rects;
  /**
   * Which timing sample these paths were measured at, empty when none exists.
   *
   * Named rather than implied, because a placement frame can be newer than the
   * timing behind it and calling a stale number "current" is how a viewer
   * misleads.
   */
  std::string timing_sample_stage;
  std::vector<PlacementDelayLineTimingVisualization> delay_line_timing;
  std::vector<PlacementTimingPathVisualization> unattributed_constraints;
  std::vector<PlacementTimingPathVisualization> ambiguous_constraints;
  std::vector<PlacementSizingDecisionEvidence> sizing_decisions;
};

/**
 * Consumer interface for placement checkpoints.
 *
 * Backends may write binary files, stream to a live GUI, pause execution, or do
 * nothing. Placement code should publish checkpoints through this interface
 * instead of depending on a particular visualization implementation.
 */
class PlacementSnapshotSink {
 public:
  virtual ~PlacementSnapshotSink() = default;

  virtual void StartRun(const PlacementSnapshotRunMetadata& metadata) = 0;
  virtual bool IsEnabled() const = 0;
  virtual void PublishSnapshot(Circuit* circuit,
                               const PlacementSnapshotMetadata& metadata) = 0;
  /** Process pending UI/backend events without publishing a new snapshot. */
  virtual void FlushEvents() {}
  virtual void FinishRun() = 0;
};

}  // namespace dali

#endif  // DALI_COMMON_PLACEMENT_SNAPSHOT_SINK_H_
