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
#include "dali/timing/timing_path_visualization.h"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "dali/timing/delay_line_feedback.h"

namespace dali {

std::string ComponentNameFromPin(const std::string &pin) {
  const std::size_t colon = pin.rfind(':');
  if (colon == std::string::npos) return pin;
  return pin.substr(0, colon);
}

namespace {

/** An edge keyed so the same connection from either path compares equal. */
using EdgeKey = std::tuple<int, int, std::string>;

struct PathGeometry {
  std::vector<int> component_ids;   // in first-seen order, unique
  std::vector<EdgeKey> edges;       // in first-seen order, unique
  int root_component_id = -1;
  int terminal_component_id = -1;
  bool resolved_anything = false;
};

/** Resolve one witness into unique components and edges. */
PathGeometry ResolvePath(const TimingPathSnapshot &path,
                         const ComponentIdResolver &resolve) {
  PathGeometry geometry;
  std::unordered_set<int> seen_components;
  std::set<EdgeKey> seen_edges;

  auto add_component = [&](int id) {
    if (id < 0) return;
    geometry.resolved_anything = true;
    if (seen_components.insert(id).second) {
      geometry.component_ids.push_back(id);
    }
  };

  geometry.root_component_id = resolve(ComponentNameFromPin(path.root_pin));
  geometry.terminal_component_id =
      resolve(ComponentNameFromPin(path.terminal_pin));
  add_component(geometry.root_component_id);
  add_component(geometry.terminal_component_id);

  for (const TimingPathStep &step : path.steps) {
    const int from = resolve(ComponentNameFromPin(step.source_pin));
    const int to = resolve(ComponentNameFromPin(step.target_pin));
    add_component(from);
    add_component(to);
    // A step whose ends are the same component is an internal arc; it colours
    // the cell but is not a line to draw, and drawing it would put a dot on a
    // cell that already has one.
    if (from < 0 || to < 0 || from == to) continue;
    const EdgeKey key{from, to, step.net_name};
    if (seen_edges.insert(key).second) {
      geometry.edges.push_back(key);
    }
  }
  return geometry;
}

PlacementPathEdge ToEdge(const EdgeKey &key) {
  PlacementPathEdge edge;
  edge.from_component_id = std::get<0>(key);
  edge.to_component_id = std::get<1>(key);
  edge.net_name = std::get<2>(key);
  return edge;
}

/** Which registered lines claim this constraint, by the shared predicate. */
std::vector<std::string> MatchingDelayLines(
    const RelativeTimingConstraintSnapshot &constraint,
    const std::vector<std::string> &registered_delay_lines) {
  std::vector<std::string> owners;
  for (const std::string &line : registered_delay_lines) {
    const bool matches = std::any_of(
        constraint.delay_repair_candidate_nets.begin(),
        constraint.delay_repair_candidate_nets.end(),
        [&line](const std::string &candidate) {
          return delay_line_feedback_internal::HasTokenBoundaryPrefix(line,
                                                                     candidate);
        });
    if (matches) owners.push_back(line);
  }
  return owners;
}

PlacementTimingPathVisualization Describe(
    const RelativeTimingConstraintSnapshot &constraint,
    const ComponentIdResolver &resolve) {
  PlacementTimingPathVisualization view;
  view.constraint_id = constraint.constraint_id;
  view.semantic_identity = constraint.SemanticIdentity();
  view.slack_ps = constraint.slack;
  view.fast_delay_ps = constraint.fast_path.TotalDelay();
  view.slow_delay_ps = constraint.slow_path.TotalDelay();

  const PathGeometry fast = ResolvePath(constraint.fast_path, resolve);
  const PathGeometry slow = ResolvePath(constraint.slow_path, resolve);
  view.has_geometry = fast.resolved_anything || slow.resolved_anything;
  view.root_component_id = slow.root_component_id >= 0 ? slow.root_component_id
                                                       : fast.root_component_id;
  view.fast_terminal_component_id = fast.terminal_component_id;
  view.slow_terminal_component_id = slow.terminal_component_id;

  // Shared before exclusive: a cell on both paths is common, and colouring it
  // as belonging to one of them would claim the two paths diverge where they do
  // not.
  const std::unordered_set<int> fast_components(fast.component_ids.begin(),
                                                fast.component_ids.end());
  const std::unordered_set<int> slow_components(slow.component_ids.begin(),
                                                slow.component_ids.end());
  for (int id : fast.component_ids) {
    if (slow_components.count(id) != 0) {
      view.common_component_ids.push_back(id);
    } else {
      view.fast_only_component_ids.push_back(id);
    }
  }
  for (int id : slow.component_ids) {
    if (fast_components.count(id) == 0) {
      view.slow_only_component_ids.push_back(id);
    }
  }

  const std::set<EdgeKey> fast_edges(fast.edges.begin(), fast.edges.end());
  const std::set<EdgeKey> slow_edges(slow.edges.begin(), slow.edges.end());
  for (const EdgeKey &key : fast.edges) {
    if (slow_edges.count(key) != 0) {
      view.common_edges.push_back(ToEdge(key));
    } else {
      view.fast_only_edges.push_back(ToEdge(key));
    }
  }
  for (const EdgeKey &key : slow.edges) {
    if (fast_edges.count(key) == 0) {
      view.slow_only_edges.push_back(ToEdge(key));
    }
  }
  return view;
}

} // namespace

TimingVisualizationResult BuildTimingPathVisualization(
    const TimingSnapshot &snapshot,
    const std::vector<std::string> &registered_delay_lines,
    const ComponentIdResolver &resolve) {
  TimingVisualizationResult result;
  std::map<std::string, std::vector<PlacementTimingPathVisualization>> by_line;
  for (const std::string &line : registered_delay_lines) {
    by_line[line];
  }

  for (const RelativeTimingConstraintSnapshot &constraint :
       snapshot.relative_constraints) {
    PlacementTimingPathVisualization view = Describe(constraint, resolve);
    const std::vector<std::string> owners =
        MatchingDelayLines(constraint, registered_delay_lines);
    view.candidate_delay_lines = owners;
    if (owners.size() == 1) {
      view.attributed_delay_line = owners.front();
      by_line[owners.front()].push_back(std::move(view));
    } else if (owners.empty()) {
      result.unattributed.push_back(std::move(view));
    } else {
      view.ambiguous_attribution = true;
      result.ambiguous.push_back(std::move(view));
    }
  }

  // Worst first, and deterministic: equal slacks order by constraint id so the
  // same evidence always names the same worst constraint.
  auto worst_first = [](const PlacementTimingPathVisualization &left,
                        const PlacementTimingPathVisualization &right) {
    if (left.slack_ps != right.slack_ps) return left.slack_ps < right.slack_ps;
    return left.constraint_id < right.constraint_id;
  };
  for (const std::string &line : registered_delay_lines) {
    PlacementDelayLineTimingVisualization entry;
    entry.delay_line_name = line;
    entry.constraints = std::move(by_line[line]);
    std::sort(entry.constraints.begin(), entry.constraints.end(), worst_first);
    if (!entry.constraints.empty()) {
      entry.worst_constraint_id = entry.constraints.front().constraint_id;
      entry.worst_slack_ps = entry.constraints.front().slack_ps;
    }
    result.delay_lines.push_back(std::move(entry));
  }
  std::sort(result.unattributed.begin(), result.unattributed.end(), worst_first);
  std::sort(result.ambiguous.begin(), result.ambiguous.end(), worst_first);
  return result;
}

} // namespace dali
