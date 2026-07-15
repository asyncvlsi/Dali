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
#include "dali/placer/well_legalizer/exact_gridded_whole_design_model_builder.h"

#include <algorithm>
#include <unordered_map>

#include "dali/common/helper.h"
#include "dali/placer/well_legalizer/exact_gridded_legalization_model_builder.h"

namespace dali {

ExactGriddedWholeDesignModelBuilder::ExactGriddedWholeDesignModelBuilder(
    Circuit* circuit, const ExactGriddedWholeDesignBuilderConfig& config)
    : circuit_(circuit), config_(config) {
  DaliExpects(circuit_ != nullptr,
              "Whole-design exact model builder requires a circuit");
  DaliExpects(config_.net_ignore_threshold >= 2,
              "Exact gridded net ignore threshold must be at least two");
  DaliExpects(
      config_.minimum_p_well_height >= 0 && config_.minimum_n_well_height >= 0,
      "Exact gridded minimum well heights must be non-negative");
  DaliExpects(config_.minimum_p_well_height + config_.minimum_n_well_height > 0,
              "Exact gridded minimum row height must be positive");
}

ExactGriddedWholeDesignBuildResult ExactGriddedWholeDesignModelBuilder::Build(
    std::vector<StripeColumn>* columns) const {
  DaliExpects(columns != nullptr,
              "Whole-design exact model builder requires stripe columns");

  struct ComponentAssignment {
    Component* component = nullptr;
    int stripe_id = -1;
    int start_row = -1;
  };

  ExactGriddedWholeDesignBuildResult result;
  std::unordered_map<int, ComponentAssignment> assignments;
  int next_stripe_id = 0;
  const int minimum_row_height =
      config_.minimum_p_well_height + config_.minimum_n_well_height;

  for (StripeColumn& column : *columns) {
    for (Stripe& stripe : column.stripe_list_) {
      std::vector<GriddedRow*> rows;
      rows.reserve(stripe.gridded_rows_.size());
      for (GriddedRow& row : stripe.gridded_rows_) rows.push_back(&row);
      std::sort(rows.begin(), rows.end(),
                [](const GriddedRow* first, const GriddedRow* second) {
                  return first->LLY() < second->LLY();
                });

      ExactGriddedStripe model_stripe;
      model_stripe.stripe_id = next_stripe_id++;
      model_stripe.lx = stripe.LLX();
      model_stripe.ly = stripe.LLY();
      model_stripe.ux = stripe.URX();
      model_stripe.uy = stripe.URY();
      model_stripe.minimum_p_well_height = config_.minimum_p_well_height;
      model_stripe.minimum_n_well_height = config_.minimum_n_well_height;
      model_stripe.maximum_rows =
          config_.use_full_row_slot_capacity
              ? stripe.Height() / minimum_row_height
              : std::max(1, static_cast<int>(rows.size()));
      DaliExpects(model_stripe.maximum_rows >= static_cast<int>(rows.size()),
                  "Existing gridded rows exceed physical stripe capacity");

      int next_row_y = model_stripe.ly;
      for (int row_id = 0; row_id < static_cast<int>(rows.size()); ++row_id) {
        GriddedRow* row = rows[row_id];
        model_stripe.left_boundary_margin = std::max(
            model_stripe.left_boundary_margin, row->LeftBoundaryMargin());
        model_stripe.right_boundary_margin = std::max(
            model_stripe.right_boundary_margin, row->RightBoundaryMargin());
        model_stripe.initial_rows.push_back(
            {true, row->LLY(), row->PHeight(), row->NHeight()});
        next_row_y = row->LLY() + row->PHeight() + row->NHeight();

        for (Component* component : row->Components()) {
          DaliExpects(component != nullptr,
                      "Gridded row contains a null component");
          auto [assignment, inserted] = assignments.emplace(
              component->Id(),
              ComponentAssignment{component, model_stripe.stripe_id, row_id});
          if (!inserted) {
            DaliExpects(
                assignment->second.stripe_id == model_stripe.stripe_id,
                "A movable component is assigned to more than one stripe");
            assignment->second.start_row =
                std::min(assignment->second.start_row, row_id);
          }
        }
      }

      while (model_stripe.initial_rows.size() <
             static_cast<size_t>(model_stripe.maximum_rows)) {
        model_stripe.initial_rows.push_back({false, next_row_y, 0, 0});
      }
      result.stats.active_row_count += static_cast<int>(rows.size());
      result.stats.row_slot_count += model_stripe.maximum_rows;
      result.model.stripes.push_back(std::move(model_stripe));
    }
  }

  DaliExpects(static_cast<int>(assignments.size()) ==
                  circuit_->TotalMovableComponentCnt(),
              "Finalized gridded rows must contain every movable component");

  std::vector<int> component_ids;
  component_ids.reserve(assignments.size());
  for (const auto& [component_id, assignment] : assignments) {
    component_ids.push_back(component_id);
  }
  std::sort(component_ids.begin(), component_ids.end());

  std::vector<ExactGriddedComponentDomain> domains;
  domains.reserve(component_ids.size());
  for (int component_id : component_ids) {
    const ComponentAssignment& assignment = assignments.at(component_id);
    ExactGriddedComponentDomain domain;
    domain.component = assignment.component;
    domain.initial_stripe_id = assignment.stripe_id;
    domain.initial_start_row = assignment.start_row;
    const int component_region_count =
        assignment.component->MacroPtr()->RegionCount();
    for (const ExactGriddedStripe& stripe : result.model.stripes) {
      if (!config_.allow_cross_stripe_moves &&
          stripe.stripe_id != assignment.stripe_id) {
        continue;
      }
      const int usable_width = stripe.ux - stripe.lx -
                               stripe.left_boundary_margin -
                               stripe.right_boundary_margin;
      if (assignment.component->Width() <= usable_width &&
          component_region_count <= stripe.maximum_rows) {
        domain.candidate_stripe_ids.push_back(stripe.stripe_id);
        result.stats.enumerated_placement_choice_upper_bound +=
            2LL * (stripe.maximum_rows - component_region_count + 1);
      }
    }
    DaliExpects(!domain.candidate_stripe_ids.empty(),
                "Movable component does not fit any exact-model stripe");
    domains.push_back(std::move(domain));
  }

  ExactGriddedModelBuilderConfig model_config;
  model_config.net_ignore_threshold = config_.net_ignore_threshold;
  ExactGriddedLegalizationModelBuilder model_builder(circuit_, model_config);
  result.model = model_builder.Build(domains, result.model.stripes);
  DaliExpects(result.model.Validate().empty(),
              "Whole-design exact legalization model is invalid");

  result.stats.stripe_count = static_cast<int>(result.model.stripes.size());
  result.stats.component_count = static_cast<int>(result.model.cells.size());
  result.stats.net_count = static_cast<int>(result.model.nets.size());
  return result;
}

}  // namespace dali
