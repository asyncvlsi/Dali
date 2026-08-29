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
 * A netlist change described entirely by value.
 *
 * The authoritative netlist lives in ACT. When a delay line grows, ACT
 * re-elaborates and the names of everything it produces are ACT's to choose --
 * Dali must not invent them. A delta is how those names, and only those names,
 * cross into Dali: plain strings and numbers, no pointers, nothing the producer
 * can still be holding a reference to and nothing Dali can mutate on the
 * producer's behalf.
 *
 * The split between validation and application is what makes applying it
 * transactional. Components are addressed by index and nets hold pointers into
 * the component vector, so a change abandoned halfway leaves a circuit no
 * caller can repair. Everything that can be rejected is therefore rejected
 * before the first component is added, and application afterwards cannot fail.
 */
#ifndef DALI_CIRCUIT_TOPOLOGY_DELTA_H_
#define DALI_CIRCUIT_TOPOLOGY_DELTA_H_

#include <string>
#include <vector>

namespace dali {

class Circuit;

/** A component the producer says now exists, with the name it gave it. */
struct TopologyDeltaComponent {
  std::string name;
  std::string macro_name;
  /**
   * Where the new cell starts out. Only new cells are placed by a delta; the
   * seed is a starting point for the next solve, not a placement decision.
   */
  double seed_x = 0.0;
  double seed_y = 0.0;
};

/** One endpoint of a net in the delta, named rather than pointed to. */
struct TopologyDeltaPin {
  std::string component_name;
  std::string pin_name;
};

/** A net the producer says now exists, with its complete connectivity. */
struct TopologyDeltaNet {
  std::string name;
  /** Complete pin list. The driver is expected first, as the producer emits it.
   */
  std::vector<TopologyDeltaPin> pins;
};

/**
 * A complete netlist change.
 *
 * Complete is the operative word: a delta describes the whole change, so it can
 * be checked as a whole. A producer that can only describe part of what it did
 * has to report failure instead.
 */
struct TopologyDelta {
  std::vector<TopologyDeltaComponent> added_components;
  std::vector<TopologyDeltaNet> added_nets;
  /** Nets that stop connecting. Retired, never erased: net ids are indices. */
  std::vector<std::string> retired_nets;
  /**
   * Nets that keep their name but connect something different.
   *
   * Raising a delay site rewires the net the site drives: its name is part of
   * the enclosing design and does not change, but the cell driving it is now a
   * different one. Measured on bd_pipeline, `dl0` at 7 pairs drives net `c1`
   * from `dl0_ainv_513_6` and at 12 pairs from `dl0_ainv_523_6`.
   *
   * Neither an addition nor a retirement describes that, and treating it as
   * either would leave Dali's netlist disagreeing with ACT's about what drives
   * a real signal. The pin list here replaces the net's existing one entirely.
   */
  std::vector<TopologyDeltaNet> rewired_nets;

  bool IsEmpty() const {
    return added_components.empty() && added_nets.empty() &&
           retired_nets.empty() && rewired_nets.empty();
  }
};

/**
 * Checks a delta against a circuit without changing it.
 *
 * Rejects unknown masters, names already taken, pins naming a component or pin
 * that will not exist, retirements of nets that are absent or carry I/O pins,
 * and any delta that would grow either vector past its reserved capacity --
 * reallocation would dangle every pointer a net holds and invalidate every id
 * the placer is carrying.
 *
 * Returns true when ApplyTopologyDelta is guaranteed to succeed.
 */
bool ValidateTopologyDelta(Circuit &circuit, const TopologyDelta &delta,
                           std::string *error_message);

/**
 * Applies a delta that ValidateTopologyDelta accepted for this same circuit.
 *
 * Existing component and net ids, coordinates, orientations, and placement
 * statuses are untouched; only the named additions and retirements happen.
 */
void ApplyTopologyDelta(Circuit &circuit, const TopologyDelta &delta);

/**
 * Validate the legalized positions of components introduced by a delta.
 *
 * Added cells remain UNPLACED until legalization succeeds. Before promotion,
 * every named cell must exist, lie inside the placement region, and have zero
 * positive-area overlap with every other design component. Touching edges are
 * legal. This is deliberately independent of placement status so it can grade
 * the state immediately before UNPLACED becomes PLACED.
 */
bool ValidateTopologyAddedPlacement(
    Circuit &circuit, const std::vector<std::string> &component_names,
    std::string *error_message);

} // namespace dali

#endif // DALI_CIRCUIT_TOPOLOGY_DELTA_H_
