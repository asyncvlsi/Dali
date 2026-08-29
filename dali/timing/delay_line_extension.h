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
 * Growing a registered delay line by appending inverter pairs to its tail.
 *
 * A delay line closes timing by being long. When a measured line is short by
 * more delay than moving its existing cells can supply, the only remaining
 * lever is more cells. This splices them onto the end of the chain, in pairs so
 * the line's logical polarity is unchanged, and leaves everything else in the
 * circuit exactly as it was.
 *
 * The work is split into a preflight that resolves and validates every name,
 * master, and capacity, and an apply that performs the mutation. Nothing may
 * fail during apply: components are addressed by index and nets hold pointers
 * into the component vector, so a mutation abandoned halfway would leave a
 * circuit no caller could repair. Preflighting every requested line before
 * applying any of them is what makes a multi-line extension all-or-none.
 */
#ifndef DALI_TIMING_DELAY_LINE_EXTENSION_H_
#define DALI_TIMING_DELAY_LINE_EXTENSION_H_

#include <string>
#include <utility>
#include <vector>

#include "dali/circuit/circuit.h"

namespace dali {

/** How much one registered line should grow. */
struct DelayLineExtensionRequest {
  std::string name_prefix;
  /** Elements are added two at a time so the line's polarity is preserved. */
  int inverter_pairs = 0;
};

/**
 * A validated extension, holding everything the mutation will need.
 *
 * Built entirely from reads of the circuit, so a plan can be discarded without
 * consequence. The names are fixed here rather than during the mutation so a
 * collision is a preflight failure instead of a half-applied edit.
 */
struct DelayLineExtensionPlan {
  std::string name_prefix;
  int tail_component_id = -1;
  /** The net the tail drives today; it is interrupted by the insertion. */
  int interrupted_net_id = -1;
  std::string macro_name;
  std::string input_pin_name;
  std::string output_pin_name;
  /** Where the new cells start out, before the placer moves them. */
  double seed_x = 0.0;
  double seed_y = 0.0;
  /** New cell names, in chain order from the current tail onward. */
  std::vector<std::string> component_names;
  /**
   * New net names, one per new cell. The last one carries the extended chain's
   * output to whatever the interrupted net used to drive.
   */
  std::vector<std::string> net_names;
  /** Component/pin pairs the interrupted net drove, reconnected at the end. */
  std::vector<std::pair<std::string, std::string>> reconnect_pins;
};

/**
 * Resolves every requested extension against the circuit without changing it.
 *
 * Fails, leaving `plans` unusable, if any line is unknown or not a simple
 * chain, if a generated name is taken, if the tail's master cannot be
 * identified, if the interrupted net carries an I/O pin, or if the circuit
 * lacks the reserved component or net capacity for all requests together.
 */
bool PreflightDelayLineExtensions(
    Circuit &circuit, const std::vector<DelayLineExtensionRequest> &requests,
    std::vector<DelayLineExtensionPlan> *plans, std::string *error_message);

/**
 * Applies preflighted extensions. Expects plans produced by
 * PreflightDelayLineExtensions against this same circuit, unmodified since.
 */
bool ApplyDelayLineExtensions(Circuit &circuit,
                              const std::vector<DelayLineExtensionPlan> &plans,
                              std::string *error_message);

} // namespace dali

#endif // DALI_TIMING_DELAY_LINE_EXTENSION_H_
