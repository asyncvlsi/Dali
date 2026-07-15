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
#include "dali/placer/well_legalizer/exact_gridded_boundary_model_builder.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "dali/common/helper.h"
#include "dali/placer/well_legalizer/exact_gridded_legalization_model_builder.h"

namespace dali {

ExactGriddedBoundaryModelBuilder::ExactGriddedBoundaryModelBuilder(
    Circuit* circuit, const ExactGriddedBoundaryModelBuilderConfig& config)
    : circuit_(circuit), config_(config) {
  DaliExpects(circuit_ != nullptr,
              "Exact boundary model builder requires a circuit");
  DaliExpects(config_.net_ignore_threshold >= 2,
              "Exact boundary net ignore threshold must be at least two");
  DaliExpects(
      config_.minimum_p_well_height >= 0 && config_.minimum_n_well_height >= 0,
      "Exact boundary minimum well heights must be non-negative");
}

std::vector<GriddedRow*> ExactGriddedBoundaryModelBuilder::SelectClosedRows(
    Stripe* stripe, int first_row, int last_row) const {
  DaliExpects(stripe != nullptr,
              "Cannot build an exact boundary from a null stripe");

  std::vector<GriddedRow*> rows;
  rows.reserve(stripe->gridded_rows_.size());
  for (GriddedRow& row : stripe->gridded_rows_) rows.push_back(&row);
  std::sort(rows.begin(), rows.end(),
            [](const GriddedRow* lhs, const GriddedRow* rhs) {
              return lhs->LLY() < rhs->LLY();
            });
  DaliExpects(first_row >= 0 && first_row <= last_row &&
                  last_row < static_cast<int>(rows.size()),
              "Exact boundary row-band range is invalid");

  std::unordered_map<int, std::pair<int, int>> component_extents;
  for (int row_index = 0; row_index < static_cast<int>(rows.size());
       ++row_index) {
    for (const Component* component : rows[row_index]->Components()) {
      auto [extent, inserted] = component_extents.emplace(
          component->Id(), std::make_pair(row_index, row_index));
      if (!inserted) {
        extent->second.first = std::min(extent->second.first, row_index);
        extent->second.second = std::max(extent->second.second, row_index);
      }
    }
  }
  for (int row_index = first_row; row_index <= last_row; ++row_index) {
    for (const Component* component : rows[row_index]->Components()) {
      const auto extent = component_extents.at(component->Id());
      DaliExpects(extent.first >= first_row && extent.second <= last_row,
                  "Exact boundary row band splits a multi-region component");
    }
  }
  return {rows.begin() + first_row, rows.begin() + last_row + 1};
}

ExactGriddedBoundaryBuildResult ExactGriddedBoundaryModelBuilder::Build(
    Stripe* first_stripe, int first_stripe_id, int first_row,
    int first_last_row, Stripe* second_stripe, int second_stripe_id,
    int second_row, int second_last_row) const {
  DaliExpects(first_stripe != second_stripe,
              "Exact boundary model requires two distinct stripes");
  DaliExpects(first_stripe_id >= 0 && second_stripe_id >= 0 &&
                  first_stripe_id != second_stripe_id,
              "Exact boundary model requires distinct non-negative ids");

  ExactGriddedBoundaryBuildResult result;
  result.row_sets.push_back(
      {first_stripe_id, first_stripe,
       SelectClosedRows(first_stripe, first_row, first_last_row)});
  result.row_sets.push_back(
      {second_stripe_id, second_stripe,
       SelectClosedRows(second_stripe, second_row, second_last_row)});

  struct ComponentAssignment {
    Component* component = nullptr;
    int stripe_id = -1;
    int first_row = -1;
  };
  std::unordered_map<int, ComponentAssignment> assignments;
  std::vector<ExactGriddedStripe> model_stripes;
  model_stripes.reserve(result.row_sets.size());
  for (const ExactGriddedBoundaryRowSet& row_set : result.row_sets) {
    DaliExpects(!row_set.rows.empty(),
                "Exact boundary model requires non-empty row bands");
    ExactGriddedStripe model_stripe;
    model_stripe.stripe_id = row_set.stripe_id;
    model_stripe.lx = row_set.stripe->LLX();
    model_stripe.ly = row_set.rows.front()->LLY();
    model_stripe.ux = row_set.stripe->URX();
    model_stripe.uy = row_set.rows.back()->URY();
    model_stripe.maximum_rows = static_cast<int>(row_set.rows.size());
    model_stripe.minimum_p_well_height = config_.minimum_p_well_height;
    model_stripe.minimum_n_well_height = config_.minimum_n_well_height;

    for (int row_index = 0; row_index < static_cast<int>(row_set.rows.size());
         ++row_index) {
      GriddedRow* row = row_set.rows[row_index];
      model_stripe.left_boundary_margin = std::max(
          model_stripe.left_boundary_margin, row->LeftBoundaryMargin());
      model_stripe.right_boundary_margin = std::max(
          model_stripe.right_boundary_margin, row->RightBoundaryMargin());
      model_stripe.initial_rows.push_back(
          {true, row->LLY(), row->PHeight(), row->NHeight()});
      for (Component* component : row->Components()) {
        DaliExpects(component != nullptr,
                    "Exact boundary row contains a null component");
        auto [assignment, inserted] = assignments.emplace(
            component->Id(),
            ComponentAssignment{component, row_set.stripe_id, row_index});
        if (!inserted) {
          DaliExpects(assignment->second.stripe_id == row_set.stripe_id,
                      "A component occupies both boundary stripes");
          assignment->second.first_row =
              std::min(assignment->second.first_row, row_index);
        }
      }
    }
    model_stripes.push_back(std::move(model_stripe));
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
    ExactGriddedComponentDomain domain;
    domain.component = assignment.component;
    domain.initial_stripe_id = assignment.stripe_id;
    domain.initial_start_row = assignment.first_row;
    const int region_count = assignment.component->MacroPtr()->RegionCount();
    for (const ExactGriddedStripe& stripe : model_stripes) {
      const int usable_width = stripe.ux - stripe.lx -
                               stripe.left_boundary_margin -
                               stripe.right_boundary_margin;
      if (assignment.component->Width() <= usable_width &&
          region_count <= stripe.maximum_rows) {
        domain.candidate_stripe_ids.push_back(stripe.stripe_id);
      }
    }
    DaliExpects(!domain.candidate_stripe_ids.empty(),
                "Boundary component does not fit either modeled stripe");
    domains.push_back(std::move(domain));
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
  result.model = ExactGriddedLegalizationModelBuilder(circuit_, model_config)
                     .Build(domains, model_stripes);
  DaliExpects(result.model.Validate().empty(),
              "Exact boundary legalization model is invalid");
  return result;
}

}  // namespace dali
