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
#include "dali/circuit/topology_delta.h"

#include <cstddef>
#include <unordered_set>

#include "dali/circuit/circuit.h"
#include "dali/common/logging.h"

namespace dali {

namespace {

/** Whether a macro carries a pin of this name. */
bool MacroHasPin(Macro *macro, const std::string &pin_name) {
  if (macro == nullptr)
    return false;
  for (Pin &pin : macro->PinList()) {
    if (pin.Name() == pin_name)
      return true;
  }
  return false;
}

} // namespace

bool ValidateTopologyDelta(Circuit &circuit, const TopologyDelta &delta,
                           std::string *error_message) {
  // Names the delta itself introduces count as existing for everything checked
  // after them, so a net may reference a component the same delta adds.
  std::unordered_set<std::string> added_component_names;
  std::unordered_set<std::string> added_net_names;

  for (const TopologyDeltaComponent &component : delta.added_components) {
    if (component.name.empty()) {
      *error_message = "delta contains a component with no name";
      return false;
    }
    if (circuit.IsComponentExisting(component.name) ||
        !added_component_names.insert(component.name).second) {
      *error_message = "component name already taken: " + component.name;
      return false;
    }
    // IsMacroExisting rather than GetMacroPtr: the getter aborts on an unknown
    // name, which would turn a rejectable delta into a crash.
    if (!circuit.IsMacroExisting(component.macro_name)) {
      *error_message = "unknown master '" + component.macro_name +
                       "' for component '" + component.name + "'";
      return false;
    }
  }

  for (const TopologyDeltaNet &net : delta.added_nets) {
    if (net.name.empty()) {
      *error_message = "delta contains a net with no name";
      return false;
    }
    if (circuit.IsNetExisting(net.name) ||
        !added_net_names.insert(net.name).second) {
      *error_message = "net name already taken: " + net.name;
      return false;
    }
    for (const TopologyDeltaPin &pin : net.pins) {
      const bool is_new = added_component_names.count(pin.component_name) > 0;
      if (!is_new && !circuit.IsComponentExisting(pin.component_name)) {
        *error_message = "net '" + net.name + "' names unknown component '" +
                         pin.component_name + "'";
        return false;
      }
      // The master is whichever one the component has or will have.
      Macro *macro = nullptr;
      if (is_new) {
        for (const TopologyDeltaComponent &component : delta.added_components) {
          if (component.name == pin.component_name) {
            if (circuit.IsMacroExisting(component.macro_name)) {
              macro = circuit.GetMacroPtr(component.macro_name);
            }
            break;
          }
        }
      } else {
        macro = circuit.GetComponentPtr(pin.component_name)->MacroPtr();
      }
      if (!MacroHasPin(macro, pin.pin_name)) {
        *error_message = "component '" + pin.component_name + "' has no pin '" +
                         pin.pin_name + "' for net '" + net.name + "'";
        return false;
      }
    }
  }

  for (const TopologyDeltaNet &net : delta.rewired_nets) {
    if (!circuit.IsNetExisting(net.name)) {
      *error_message = "cannot rewire unknown net: " + net.name;
      return false;
    }
    Net *existing = circuit.GetNetPtr(net.name);
    if (!existing->IoPinPtrs().empty()) {
      *error_message =
          "cannot rewire net '" + net.name + "' because it carries an I/O pin";
      return false;
    }
    // A net's pin capacity is fixed when it is created, so a rewire that needs
    // more endpoints than the net was built for cannot be applied at all.
    if (net.pins.size() > existing->ComponentPins().capacity()) {
      *error_message = "rewiring net '" + net.name + "' needs " +
                       std::to_string(net.pins.size()) +
                       " pins but it was created with room for " +
                       std::to_string(existing->ComponentPins().capacity());
      return false;
    }
    for (const TopologyDeltaPin &pin : net.pins) {
      const bool is_new = added_component_names.count(pin.component_name) > 0;
      if (!is_new && !circuit.IsComponentExisting(pin.component_name)) {
        *error_message = "rewired net '" + net.name +
                         "' names unknown component '" + pin.component_name +
                         "'";
        return false;
      }
      Macro *macro = nullptr;
      if (is_new) {
        for (const TopologyDeltaComponent &component : delta.added_components) {
          if (component.name == pin.component_name) {
            if (circuit.IsMacroExisting(component.macro_name)) {
              macro = circuit.GetMacroPtr(component.macro_name);
            }
            break;
          }
        }
      } else {
        macro = circuit.GetComponentPtr(pin.component_name)->MacroPtr();
      }
      if (!MacroHasPin(macro, pin.pin_name)) {
        *error_message = "component '" + pin.component_name + "' has no pin '" +
                         pin.pin_name + "' for rewired net '" + net.name + "'";
        return false;
      }
    }
  }

  for (const std::string &net_name : delta.retired_nets) {
    if (!circuit.IsNetExisting(net_name)) {
      *error_message = "cannot retire unknown net: " + net_name;
      return false;
    }
    // Retiring a net with I/O pins is refused by Net::Retire itself, so it is
    // caught here instead, where refusing is still free.
    if (!circuit.GetNetPtr(net_name)->IoPinPtrs().empty()) {
      *error_message =
          "cannot retire net '" + net_name + "' because it carries an I/O pin";
      return false;
    }
  }

  // Capacity is a fixed budget taken when the design was read. Growing either
  // vector would reallocate it, dangling every pointer a net holds into the
  // component vector and invalidating every id the placer is carrying.
  const std::vector<Component> &components = circuit.Components();
  if (components.size() + delta.added_components.size() >
      components.capacity()) {
    *error_message = "delta needs " +
                     std::to_string(delta.added_components.size()) +
                     " components but only " +
                     std::to_string(components.capacity() - components.size()) +
                     " are reserved";
    return false;
  }
  const std::vector<Net> &nets = circuit.Nets();
  if (nets.size() + delta.added_nets.size() > nets.capacity()) {
    *error_message = "delta needs " + std::to_string(delta.added_nets.size()) +
                     " nets but only " +
                     std::to_string(nets.capacity() - nets.size()) +
                     " are reserved";
    return false;
  }
  return true;
}

/**
 * Reopens the component registry for the length of a delta, and closes it again
 * however the scope is left.
 *
 * The registry is frozen once the design is loaded, so that a late accidental
 * insertion is caught rather than silently accepted. A topology delta is the
 * one deliberate late insertion, and it is a narrow, validated one: reopening
 * for exactly its duration keeps the guard meaningful for everything else.
 */
class ScopedRegistryReopen {
public:
  explicit ScopedRegistryReopen(Circuit &circuit)
      : circuit_(circuit),
        was_frozen_(circuit.design().ComponentCollection().IsFrozen()) {
    if (was_frozen_)
      circuit_.design().ComponentCollection().Unfreeze();
  }
  ~ScopedRegistryReopen() {
    if (was_frozen_)
      circuit_.design().ComponentCollection().Freeze();
  }
  ScopedRegistryReopen(const ScopedRegistryReopen &) = delete;
  ScopedRegistryReopen &operator=(const ScopedRegistryReopen &) = delete;

private:
  Circuit &circuit_;
  bool was_frozen_ = false;
};

void ApplyTopologyDelta(Circuit &circuit, const TopologyDelta &delta) {
  ScopedRegistryReopen reopened(circuit);
  // Retire first: an insertion usually interrupts an existing connection, and
  // the new nets are what put the circuit back together.
  for (const std::string &net_name : delta.retired_nets) {
    circuit.GetNetPtr(net_name)->Retire();
  }
  // UNPLACED, because at this instant that is true: the cell has a seed, which
  // is its line's centroid, and no legal site yet. Dali promotes it once
  // legalization has actually given it one. Marking it placed here would be
  // faster and would be a lie -- a run whose legalization failed would export
  // cells claiming positions nothing had assigned them.
  for (const TopologyDeltaComponent &component : delta.added_components) {
    circuit.AddComponent(component.name, component.macro_name, component.seed_x,
                         component.seed_y, UNPLACED, N);
  }
  // Rewiring before the additions, for the same reason retirement comes first:
  // the net being rewired is the one the grown site now drives, and its old
  // driver must let go before the new one takes over.
  for (const TopologyDeltaNet &net : delta.rewired_nets) {
    circuit.GetNetPtr(net.name)->Retire();
  }
  for (const TopologyDeltaNet &net : delta.added_nets) {
    circuit.AddNet(net.name, net.pins.size());
    for (const TopologyDeltaPin &pin : net.pins) {
      circuit.AddComponentPinToNet(pin.component_name, pin.pin_name, net.name);
    }
  }
  for (const TopologyDeltaNet &net : delta.rewired_nets) {
    for (const TopologyDeltaPin &pin : net.pins) {
      circuit.AddComponentPinToNet(pin.component_name, pin.pin_name, net.name);
    }
  }
  LOG(info) << "  Applied topology delta: +" << delta.added_components.size()
            << " components, +" << delta.added_nets.size() << " nets, "
            << delta.retired_nets.size() << " retired, "
            << delta.rewired_nets.size() << " rewired\n";
}

bool ValidateTopologyAddedPlacement(
    Circuit &circuit, const std::vector<std::string> &component_names,
    std::string *error_message) {
  std::unordered_set<int> added_ids;
  for (const std::string &name : component_names) {
    Component *component = circuit.GetComponentPtr(name);
    if (component == nullptr) {
      *error_message = "added component no longer exists: " + name;
      return false;
    }
    if (!added_ids.insert(component->Id()).second) {
      *error_message = "added component is listed twice: " + name;
      return false;
    }
    if (component->LLX() < circuit.RegionLLX() ||
        component->LLY() < circuit.RegionLLY() ||
        component->URX() > circuit.RegionURX() ||
        component->URY() > circuit.RegionURY()) {
      *error_message =
          "added component is outside the placement region: " + name;
      return false;
    }
  }

  const std::vector<Component> &components = circuit.Components();
  for (int added_id : added_ids) {
    const Component &added = components[added_id];
    for (const Component &other : components) {
      if (other.Id() == added_id)
        continue;
      if (added.OverlapArea(other) <= 0.0)
        continue;
      *error_message = "added component '" + added.Name() +
                       "' overlaps component '" + other.Name() + "'";
      return false;
    }
  }
  return true;
}

} // namespace dali
