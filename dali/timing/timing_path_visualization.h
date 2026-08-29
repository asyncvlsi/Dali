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
/**
 * @file
 * Turn captured timing witnesses into drawable, value-only records.
 *
 * The viewer needs to know which cells lie on a constraint's fast path, which
 * on its slow path, and which on both. That question is answered here, once,
 * while the circuit and the timing snapshot are both alive -- not in Qt, which
 * receives component ids and never sees a pin string, a net, or a pointer into
 * the timer.
 *
 * Attribution is not re-decided here. A constraint belongs to the registered
 * delay line whose prefix matches one of its repair-candidate nets, by exactly
 * the predicate `AttributeDelayLineConstraints` uses. What differs is the
 * response to the awkward cases: a constraint owned by no line, or matched by
 * several, is reported rather than dropped or treated as an error, because a
 * diagnostic view that silently omits constraints is worse than one that shows
 * them in a group of their own.
 */
#ifndef DALI_TIMING_TIMING_PATH_VISUALIZATION_H_
#define DALI_TIMING_TIMING_PATH_VISUALIZATION_H_

#include <functional>
#include <string>
#include <vector>

#include "dali/common/placement_snapshot_sink.h"
#include "dali/timing/timing_snapshot.h"

namespace dali {

/**
 * Resolve a component name to its circuit id, or -1 when it is not a component.
 *
 * I/O endpoints and pins inside a replaceable site legitimately resolve to
 * nothing; that is a fact about the design, not a failure, and the caller marks
 * such a path as carrying no geometry rather than inventing a coordinate.
 */
using ComponentIdResolver = std::function<int(const std::string &)>;

struct TimingVisualizationResult {
  std::vector<PlacementDelayLineTimingVisualization> delay_lines;
  std::vector<PlacementTimingPathVisualization> unattributed;
  std::vector<PlacementTimingPathVisualization> ambiguous;
};

/**
 * Group every constraint in `snapshot` under the registered line that owns it.
 *
 * `registered_delay_lines` are the names Dali registered, in the order the
 * caller wants them listed. Lines with no attributed constraint still appear,
 * so a line that stopped being measured is visible as empty rather than absent.
 *
 * Each line's constraints are sorted by slack ascending, so the worst is first
 * and `worst_constraint_id` names it. Ties are broken by constraint id, so the
 * same evidence always produces the same worst constraint.
 */
TimingVisualizationResult BuildTimingPathVisualization(
    const TimingSnapshot &snapshot,
    const std::vector<std::string> &registered_delay_lines,
    const ComponentIdResolver &resolve);

/** Split `pin` at its last colon, returning the component-name part. */
std::string ComponentNameFromPin(const std::string &pin);

} // namespace dali

#endif // DALI_TIMING_TIMING_PATH_VISUALIZATION_H_
