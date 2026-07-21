/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *******************************************************************************/
#include "dali/placer/well_legalizer/well_row_completer.h"

#include <string>
#include <utility>

namespace dali {

WellRowCompleter::WellRowCompleter(Circuit* circuit,
                                   std::vector<StripeColumn>* columns,
                                   WellRowCompletionConfig config)
    : circuit_(circuit), columns_(columns), config_(config) {
  DaliExpects(circuit_ != nullptr, "Well row completion requires a circuit");
  DaliExpects(columns_ != nullptr,
              "Well row completion requires legalized columns");
}

void WellRowCompleter::InsertWellTaps() {
  DaliExpects(config_.well_tap_macro != nullptr,
              "Cannot insert well taps without a well-tap macro");

  RowEndTapPlacer default_placer;
  const TapPlacer& placer =
      config_.tap_placer != nullptr ? *config_.tap_placer : default_placer;
  const TapPlacementContext ctx{config_.well_tap_macro, config_.pre_end_cap_width,
                                config_.post_end_cap_width,
                                config_.space_to_well_tap};

  auto& tap_components = circuit_->design().WellTapComponentCollection();
  tap_components.Clear();

  size_t row_count = 0;
  for (const auto& column : *columns_) {
    for (const auto& stripe : column.stripe_list_) {
      row_count += stripe.gridded_rows_.size();
    }
  }
  // Hint only; the collection grows if a pattern places more taps per row.
  tap_components.Reserve(row_count * config_.well_tap_count_per_row);

  int component_id = 0;
  size_t row_index = 0;
  for (auto& column : *columns_) {
    for (auto& stripe : column.stripe_list_) {
      for (auto& row : stripe.gridded_rows_) {
        placer.ValidateRow(row, ctx);
        for (double tap_center : placer.RowTapCenters(row, row_index, ctx)) {
          std::string name = "__well_tap__" + std::to_string(component_id++);
          auto [tap, tap_id] = tap_components.CreateWithId(name);
          tap.SetPlacementStatus(PLACED);
          tap.SetMacro(config_.well_tap_macro);
          tap.SetId(static_cast<int>(tap_id));
          row.InsertWellTapCell(tap, tap_center);
        }
        ++row_index;
      }
    }
  }

  tap_components.Freeze();
  LOG(info) << "Insertion complete: " << component_id << " well tap cell created ("
            << placer.Name() << " pattern)\n";
}

void WellRowCompleter::CreateEndCapMacros() {
  for (const auto& column : *columns_) {
    for (const auto& stripe : column.stripe_list_) {
      for (const auto& row : stripe.gridded_rows_) {
        RowHeight row_height = {row.NHeight(), row.PHeight()};
        if (pre_end_cap_macro_ids_.find(row_height) !=
            pre_end_cap_macro_ids_.end()) {
          continue;
        }

        std::string height_suffix =
            "_n_height_" + std::to_string(row.NHeight()) + "_p_height_" +
            std::to_string(row.PHeight());
        int pre_macro_id = circuit_->CreateEndCapMacro(
            "pre_end_cap" + height_suffix, config_.pre_end_cap_width,
            row.NHeight(), row.PHeight());
        int post_macro_id = circuit_->CreateEndCapMacro(
            "post_end_cap" + height_suffix, config_.post_end_cap_width,
            row.NHeight(), row.PHeight());
        pre_end_cap_macro_ids_[row_height] = pre_macro_id;
        post_end_cap_macro_ids_[row_height] = post_macro_id;
      }
    }
  }
  circuit_->tech().EndCapCellMacroCollection().Freeze();
}

void WellRowCompleter::InsertEndCaps() {
  DaliExpects(config_.pre_end_cap_width > 0,
              "Pre-end-cap width must be positive");
  DaliExpects(config_.post_end_cap_width > 0,
              "Post-end-cap width must be positive");
  CreateEndCapMacros();

  auto& end_cap_components = circuit_->design().EndCapComponentCollection();
  end_cap_components.Clear();

  size_t row_count = 0;
  for (const auto& column : *columns_) {
    for (const auto& stripe : column.stripe_list_) {
      row_count += stripe.gridded_rows_.size();
    }
  }
  end_cap_components.Reserve(row_count * 2);

  int row_id = 0;
  for (auto& column : *columns_) {
    for (auto& stripe : column.stripe_list_) {
      for (auto& row : stripe.gridded_rows_) {
        RowHeight row_height = {row.NHeight(), row.PHeight()};
        Macro* pre_macro =
            circuit_->tech().EndCapCellMacroCollection().GetInstanceById(
                pre_end_cap_macro_ids_.at(row_height));
        Macro* post_macro =
            circuit_->tech().EndCapCellMacroCollection().GetInstanceById(
                post_end_cap_macro_ids_.at(row_height));

        auto [pre_end_cap, pre_id] = end_cap_components.CreateWithId(
            "__pre_end_cap_cell__" + std::to_string(row_id));
        pre_end_cap.SetPlacementStatus(PLACED);
        pre_end_cap.SetMacro(pre_macro);
        pre_end_cap.SetId(static_cast<int>(pre_id));
        row.PlacePhysicalCell(pre_end_cap,
                              row.LLX() + pre_macro->Width() / 2.0);

        auto [post_end_cap, post_id] = end_cap_components.CreateWithId(
            "__post_end_cap_cell__" + std::to_string(row_id));
        post_end_cap.SetPlacementStatus(PLACED);
        post_end_cap.SetMacro(post_macro);
        post_end_cap.SetId(static_cast<int>(post_id));
        row.PlacePhysicalCell(post_end_cap,
                              row.URX() - post_macro->Width() / 2.0);
        ++row_id;
      }
    }
  }

  end_cap_components.Freeze();
  LOG(info) << "Insertion complete: " << row_id * 2
            << " pre- and post- end cap cells created\n";
}

}  // namespace dali
