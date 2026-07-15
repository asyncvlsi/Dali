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
#include "dali/placer/well_legalizer/exact_gridded_legalization_model_builder.h"

#include <algorithm>
#include <cmath>
#include <unordered_set>

#include "dali/common/helper.h"

namespace dali {

ExactGriddedLegalizationModelBuilder::ExactGriddedLegalizationModelBuilder(
    Circuit* circuit, const ExactGriddedModelBuilderConfig& config)
    : circuit_(circuit), config_(config) {
  DaliExpects(circuit_ != nullptr,
              "Exact gridded model builder requires a circuit");
  DaliExpects(config_.net_ignore_threshold >= 2,
              "Exact gridded net ignore threshold must be at least two");
}

ExactGriddedLegalizationModel ExactGriddedLegalizationModelBuilder::Build(
    const std::vector<ExactGriddedComponentDomain>& domains,
    const std::vector<ExactGriddedStripe>& stripes) const {
  ExactGriddedLegalizationModel model;
  model.stripes = stripes;
  model.distance_scale_x = circuit_->GridValueX();
  model.distance_scale_y = circuit_->GridValueY();

  std::unordered_set<int> variable_component_ids;
  std::vector<int> affected_net_ids;
  model.cells.reserve(domains.size());
  for (const ExactGriddedComponentDomain& domain : domains) {
    Component* component = domain.component;
    DaliExpects(component != nullptr,
                "Exact gridded component domain contains a null component");
    DaliExpects(component->IsMovable(),
                "Exact gridded component domains must be movable");
    DaliExpects(variable_component_ids.insert(component->Id()).second,
                "Exact gridded component domains must be unique");

    Macro* macro = component->MacroPtr();
    DaliExpects(macro != nullptr, "Exact gridded component has no cell master");
    DaliExpects(macro->HasWellInfo(),
                "Exact gridded component has no well information");

    ExactGriddedCell cell;
    cell.component_id = component->Id();
    cell.width = component->Width();
    cell.initial_x = static_cast<int>(std::llround(component->LLX()));
    cell.initial_y = static_cast<int>(std::llround(component->LLY()));
    cell.candidate_stripe_ids = domain.candidate_stripe_ids;
    if (macro->HasCompleteWellRegions()) {
      cell.regions.reserve(macro->RegionCount());
      for (int region_id = 0; region_id < macro->RegionCount(); ++region_id) {
        cell.regions.push_back({macro->PwellHeight(region_id),
                                macro->NwellHeight(region_id),
                                macro->IsNwellAbovePwell(region_id)});
      }
    } else {
      // Legacy .cell inputs describe a single N-well rectangle and imply the
      // complementary P-well below it. Match final legalization and capacity
      // estimation by treating that shape as one complete logical region.
      cell.regions.push_back(
          {macro->FirstPwellHeight(), macro->FirstNwellHeight(), true});
    }
    model.cells.push_back(std::move(cell));
    affected_net_ids.insert(affected_net_ids.end(),
                            component->NetList().begin(),
                            component->NetList().end());
  }

  std::sort(affected_net_ids.begin(), affected_net_ids.end());
  affected_net_ids.erase(
      std::unique(affected_net_ids.begin(), affected_net_ids.end()),
      affected_net_ids.end());
  for (int net_id : affected_net_ids) {
    DaliExpects(
        net_id >= 0 && net_id < static_cast<int>(circuit_->Nets().size()),
        "Component refers to a net outside the circuit net list");
    Net& net = circuit_->Nets()[net_id];
    if (net.PinCnt() < 2 ||
        net.PinCnt() >= static_cast<size_t>(config_.net_ignore_threshold) ||
        net.Weight() <= 0.0) {
      continue;
    }

    ExactGriddedNet model_net;
    model_net.weight = net.Weight();
    model_net.pins.reserve(net.ComponentPins().size());
    bool has_variable_pin = false;
    for (const NetPin& pin : net.ComponentPins()) {
      if (variable_component_ids.count(pin.ComponentId()) != 0) {
        Pin* macro_pin = pin.PinPtr();
        model_net.pins.push_back({pin.ComponentId(), macro_pin->OffsetX(N),
                                  macro_pin->OffsetY(N), macro_pin->OffsetX(FS),
                                  macro_pin->OffsetY(FS), 0.0, 0.0});
        has_variable_pin = true;
      } else {
        model_net.pins.push_back(
            {-1, 0.0, 0.0, 0.0, 0.0, pin.AbsX(), pin.AbsY()});
      }
    }
    if (has_variable_pin) model.nets.push_back(std::move(model_net));
  }
  return model;
}

}  // namespace dali
