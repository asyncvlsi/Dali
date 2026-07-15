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
#include "dali/placer/well_legalizer/exact_gridded_stripe_model_builder.h"

#include <algorithm>
#include <unordered_map>

#include "dali/common/helper.h"
#include "dali/placer/well_legalizer/exact_gridded_legalization_model_builder.h"

namespace dali {

ExactGriddedStripeModelBuilder::ExactGriddedStripeModelBuilder(
    Circuit* circuit, const ExactGriddedStripeModelBuilderConfig& config)
    : circuit_(circuit), config_(config) {
  DaliExpects(circuit_ != nullptr,
              "Exact stripe model builder requires a circuit");
  DaliExpects(config_.net_ignore_threshold >= 2,
              "Exact stripe net ignore threshold must be at least two");
  DaliExpects(
      config_.minimum_p_well_height >= 0 && config_.minimum_n_well_height >= 0,
      "Exact stripe minimum well heights must be non-negative");
}

ExactGriddedStripeBuildResult ExactGriddedStripeModelBuilder::Build(
    Stripe* stripe, int stripe_id) const {
  DaliExpects(stripe != nullptr, "Cannot build a model from a null stripe");
  DaliExpects(stripe_id >= 0, "Exact stripe id must be non-negative");

  ExactGriddedStripeBuildResult result;
  result.rows.reserve(stripe->gridded_rows_.size());
  for (GriddedRow& row : stripe->gridded_rows_) result.rows.push_back(&row);
  std::sort(result.rows.begin(), result.rows.end(),
            [](const GriddedRow* lhs, const GriddedRow* rhs) {
              return lhs->LLY() < rhs->LLY();
            });
  if (result.rows.empty()) return result;

  ExactGriddedStripe model_stripe;
  model_stripe.stripe_id = stripe_id;
  model_stripe.lx = stripe->LLX();
  model_stripe.ly = stripe->LLY();
  model_stripe.ux = stripe->URX();
  model_stripe.uy = stripe->URY();
  model_stripe.maximum_rows = static_cast<int>(result.rows.size());
  model_stripe.minimum_p_well_height = config_.minimum_p_well_height;
  model_stripe.minimum_n_well_height = config_.minimum_n_well_height;

  struct ComponentAssignment {
    Component* component = nullptr;
    int first_row = -1;
  };
  std::unordered_map<int, ComponentAssignment> assignments;
  for (int row_index = 0; row_index < static_cast<int>(result.rows.size());
       ++row_index) {
    GriddedRow* row = result.rows[row_index];
    model_stripe.left_boundary_margin =
        std::max(model_stripe.left_boundary_margin, row->LeftBoundaryMargin());
    model_stripe.right_boundary_margin = std::max(
        model_stripe.right_boundary_margin, row->RightBoundaryMargin());
    model_stripe.initial_rows.push_back(
        {true, row->LLY(), row->PHeight(), row->NHeight()});
    for (Component* component : row->Components()) {
      DaliExpects(component != nullptr,
                  "Exact stripe row contains a null component");
      auto [assignment, inserted] = assignments.emplace(
          component->Id(), ComponentAssignment{component, row_index});
      if (!inserted) {
        assignment->second.first_row =
            std::min(assignment->second.first_row, row_index);
      }
    }
  }

  std::vector<int> component_ids;
  component_ids.reserve(assignments.size());
  for (const auto& [component_id, assignment] : assignments) {
    component_ids.push_back(component_id);
  }
  std::sort(component_ids.begin(), component_ids.end());

  std::vector<ExactGriddedComponentDomain> domains;
  domains.reserve(component_ids.size());
  result.components.reserve(component_ids.size());
  for (int component_id : component_ids) {
    const ComponentAssignment& assignment = assignments.at(component_id);
    domains.push_back(
        {assignment.component, {stripe_id}, stripe_id, assignment.first_row});
    result.components.push_back(assignment.component);
    result.affected_net_ids.insert(result.affected_net_ids.end(),
                                   assignment.component->NetList().begin(),
                                   assignment.component->NetList().end());
  }
  std::sort(result.affected_net_ids.begin(), result.affected_net_ids.end());
  result.affected_net_ids.erase(std::unique(result.affected_net_ids.begin(),
                                            result.affected_net_ids.end()),
                                result.affected_net_ids.end());

  ExactGriddedModelBuilderConfig model_config;
  model_config.net_ignore_threshold = config_.net_ignore_threshold;
  ExactGriddedLegalizationModelBuilder model_builder(circuit_, model_config);
  result.model = model_builder.Build(domains, {model_stripe});
  DaliExpects(result.model.Validate().empty(),
              "Exact stripe legalization model is invalid");
  return result;
}

}  // namespace dali
