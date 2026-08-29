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
#include "dali/timing/delay_line_extension.h"

#include <cstddef>
#include <unordered_set>

#include "dali/common/logging.h"
#include "dali/timing/delay_line_detour.h"

namespace dali {

namespace {

/**
 * The pin names the chain already uses, read off the tail's own connections.
 *
 * Taken from connectivity rather than from the master's pin list because a
 * master carries power pins too, and which of its signal pins the chain uses is
 * a property of how the design was built, not of the cell.
 */
bool ResolveTailPinNames(Circuit &circuit, int tail_component_id,
                         std::string *input_pin_name,
                         std::string *output_pin_name, int *driver_net_id,
                         std::string *error_message) {
  bool found_input = false;
  bool found_output = false;
  for (Net &net : circuit.Nets()) {
    for (NetPin &net_pin : net.ComponentPins()) {
      if (net_pin.ComponentId() != tail_component_id) continue;
      if (net_pin.PinPtr()->IsInput()) {
        if (!found_input) {
          *input_pin_name = net_pin.PinPtr()->Name();
          found_input = true;
        }
      } else {
        if (found_output) {
          *error_message = "delay-line tail drives more than one net";
          return false;
        }
        *output_pin_name = net_pin.PinPtr()->Name();
        *driver_net_id = net.Id();
        found_output = true;
      }
    }
  }
  if (!found_input || !found_output) {
    *error_message = "delay-line tail is missing an input or an output net";
    return false;
  }
  return true;
}

} // namespace

bool PreflightDelayLineExtensions(
    Circuit &circuit, const std::vector<DelayLineExtensionRequest> &requests,
    std::vector<DelayLineExtensionPlan> *plans, std::string *error_message) {
  plans->clear();

  size_t added_components = 0;
  size_t added_nets = 0;
  // Names are checked against the circuit and against each other, because two
  // requests in one batch are applied without a circuit lookup in between.
  std::unordered_set<std::string> claimed_component_names;
  std::unordered_set<std::string> claimed_net_names;

  for (const DelayLineExtensionRequest &request : requests) {
    if (request.inverter_pairs <= 0) continue;

    DelayLineChain chain;
    if (!BuildDelayLineChain(circuit, request.name_prefix, &chain,
                             error_message)) {
      *error_message =
          "cannot extend '" + request.name_prefix + "': " + *error_message;
      return false;
    }
    if (chain.nodes.empty()) {
      *error_message = "cannot extend '" + request.name_prefix +
                       "': the chain has no elements";
      return false;
    }

    DelayLineExtensionPlan plan;
    plan.name_prefix = request.name_prefix;
    plan.tail_component_id = chain.nodes.back().component_id;

    Component &tail = circuit.Components()[plan.tail_component_id];
    if (tail.MacroPtr() == nullptr) {
      *error_message = "cannot extend '" + request.name_prefix + "': '" +
                       tail.Name() + "' has no master";
      return false;
    }
    plan.macro_name = tail.MacroPtr()->Name();
    plan.seed_x = tail.LLX();
    plan.seed_y = tail.LLY();

    if (!ResolveTailPinNames(circuit, plan.tail_component_id,
                             &plan.input_pin_name, &plan.output_pin_name,
                             &plan.interrupted_net_id, error_message)) {
      *error_message =
          "cannot extend '" + request.name_prefix + "': " + *error_message;
      return false;
    }

    Net &interrupted = circuit.Nets()[plan.interrupted_net_id];
    if (!interrupted.IoPinPtrs().empty()) {
      *error_message = "cannot extend '" + request.name_prefix + "': net '" +
                       interrupted.Name() +
                       "' carries an I/O pin, which cannot be reconnected "
                       "through an inserted chain";
      return false;
    }
    for (NetPin &net_pin : interrupted.ComponentPins()) {
      if (net_pin.ComponentId() == plan.tail_component_id) continue;
      plan.reconnect_pins.emplace_back(net_pin.ComponentPtr()->Name(),
                                       net_pin.PinPtr()->Name());
    }

    // Names carry the chain's current length so a line extended more than once
    // cannot collide with its own earlier growth, and stay under the registered
    // prefix so the chain builder finds the new cells as members of this line.
    const int elements = request.inverter_pairs * 2;
    const std::string round = std::to_string(chain.nodes.size());
    for (int index = 0; index < elements; ++index) {
      std::string component_name =
          request.name_prefix + "_ext" + round + "_" + std::to_string(index);
      if (circuit.IsComponentExisting(component_name) ||
          !claimed_component_names.insert(component_name).second) {
        *error_message = "cannot extend '" + request.name_prefix +
                         "': component name '" + component_name +
                         "' is already taken";
        return false;
      }
      plan.component_names.push_back(std::move(component_name));
    }
    // One net per new cell to feed it, plus one carrying the extended chain's
    // output onward to what the interrupted net used to drive.
    for (int index = 0; index <= elements; ++index) {
      std::string net_name =
          request.name_prefix + "_extnet" + round + "_" + std::to_string(index);
      if (circuit.IsNetExisting(net_name) ||
          !claimed_net_names.insert(net_name).second) {
        *error_message = "cannot extend '" + request.name_prefix +
                         "': net name '" + net_name + "' is already taken";
        return false;
      }
      plan.net_names.push_back(std::move(net_name));
    }
    added_components += static_cast<size_t>(elements);
    added_nets += static_cast<size_t>(elements) + 1;
    plans->push_back(std::move(plan));
  }

  // Nets hold pointers into the component vector and the placer holds ids into
  // both, so neither may reallocate. The reserve made when the design was read
  // is the whole budget; exceeding it is a preflight failure, never a resize.
  const std::vector<Component> &components = circuit.Components();
  if (components.size() + added_components > components.capacity()) {
    *error_message =
        "delay-line extension needs " + std::to_string(added_components) +
        " components but only " +
        std::to_string(components.capacity() - components.size()) +
        " are reserved";
    return false;
  }
  const std::vector<Net> &nets = circuit.Nets();
  if (nets.size() + added_nets > nets.capacity()) {
    *error_message = "delay-line extension needs " + std::to_string(added_nets) +
                     " nets but only " +
                     std::to_string(nets.capacity() - nets.size()) +
                     " are reserved";
    return false;
  }
  return true;
}

bool ApplyDelayLineExtensions(Circuit &circuit,
                              const std::vector<DelayLineExtensionPlan> &plans,
                              std::string *error_message) {
  for (const DelayLineExtensionPlan &plan : plans) {
    if (plan.component_names.empty()) continue;

    // Retiring first leaves the tail driving nothing, which is the state the
    // new cells are spliced into. The net keeps its slot: nets are addressed by
    // index everywhere, so removing one would renumber the rest.
    circuit.Nets()[plan.interrupted_net_id].Retire();

    // Seeded on top of the tail rather than spread out: the placer decides
    // where these belong, and starting them anywhere else would be a placement
    // decision made by the mutation.
    for (const std::string &component_name : plan.component_names) {
      circuit.AddComponent(component_name, plan.macro_name, plan.seed_x,
                           plan.seed_y, UNPLACED, N);
    }

    // Each new net joins one element's output to the next element's input, the
    // first fed by the old tail. Names are resolved through the circuit rather
    // than held as pointers because adding a component or net invalidates both.
    const size_t count = plan.component_names.size();
    const std::string tail_name =
        circuit.Components()[plan.tail_component_id].Name();
    for (size_t index = 0; index < count; ++index) {
      circuit.AddNet(plan.net_names[index], 2);
      const std::string &driver_name =
          index == 0 ? tail_name : plan.component_names[index - 1];
      circuit.AddComponentPinToNet(driver_name, plan.output_pin_name,
                                   plan.net_names[index]);
      circuit.AddComponentPinToNet(plan.component_names[index],
                                   plan.input_pin_name, plan.net_names[index]);
    }

    // The final net carries the new tail's output to everything the interrupted
    // net drove, which is what puts the extended chain back in circuit.
    const std::string &last_net = plan.net_names.back();
    circuit.AddNet(last_net, plan.reconnect_pins.size() + 1);
    circuit.AddComponentPinToNet(plan.component_names.back(),
                                 plan.output_pin_name, last_net);
    for (const auto &pin : plan.reconnect_pins) {
      circuit.AddComponentPinToNet(pin.first, pin.second, last_net);
    }

    LOG(info) << "  Extended delay line '" << plan.name_prefix << "' by "
              << count << " elements\n";
  }
  (void)error_message;
  return true;
}

} // namespace dali
