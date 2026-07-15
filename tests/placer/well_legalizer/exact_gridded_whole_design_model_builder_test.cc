/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/exact_gridded_whole_design_model_builder.h"

#include <gtest/gtest.h>

namespace dali {

TEST(ExactGriddedWholeDesignModelBuilderTest,
     BuildsAllMovableCellsWithEveryFittingStripe) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(4, 0, 1);
  Macro* macro = circuit.AddMacro("cell", 2, 2);
  macro->AddWellRect(false, 0, 0, 2, 1);
  macro->AddWellRect(true, 0, 1, 2, 2);
  circuit.AddMacroPin(macro, "pin", true)->SetOffset(1, 1);
  circuit.AddComponent("first", "cell", 0, 0, PLACED);
  circuit.AddComponent("second", "cell", 2, 0, PLACED);
  circuit.AddComponent("third", "cell", 10, 0, PLACED);
  circuit.AddComponent("anchor", "cell", 18, 0, FIXED);
  circuit.AddNet("signal", 2);
  circuit.AddComponentPinToNet("first", "pin", "signal");
  circuit.AddComponentPinToNet("anchor", "pin", "signal");

  std::vector<StripeColumn> columns(2);
  for (int column_id = 0; column_id < 2; ++column_id) {
    StripeColumn& column = columns[column_id];
    column.stripe_list_.resize(1);
    Stripe& stripe = column.stripe_list_[0];
    stripe.lx_ = column_id * 10;
    stripe.ly_ = 0;
    stripe.width_ = 10;
    stripe.height_ = 6;
    stripe.gridded_rows_.resize(1);
    GriddedRow& row = stripe.gridded_rows_[0];
    row.SetLLX(stripe.LLX());
    row.SetLLY(0);
    row.SetWidth(stripe.Width());
    row.UpdateWellHeightUpward(1, 1);
  }
  columns[0].stripe_list_[0].gridded_rows_[0].AddComponent(
      circuit.GetComponentPtr("first"));
  columns[0].stripe_list_[0].gridded_rows_[0].AddComponent(
      circuit.GetComponentPtr("second"));
  columns[1].stripe_list_[0].gridded_rows_[0].AddComponent(
      circuit.GetComponentPtr("third"));

  ExactGriddedWholeDesignBuilderConfig config;
  config.minimum_p_well_height = 1;
  config.minimum_n_well_height = 1;
  ExactGriddedWholeDesignBuildResult result =
      ExactGriddedWholeDesignModelBuilder(&circuit, config).Build(&columns);

  EXPECT_TRUE(result.model.Validate().empty());
  EXPECT_EQ(result.stats.stripe_count, 2);
  EXPECT_EQ(result.stats.active_row_count, 2);
  EXPECT_EQ(result.stats.row_slot_count, 6);
  EXPECT_EQ(result.stats.component_count, 3);
  EXPECT_EQ(result.stats.net_count, 1);
  EXPECT_EQ(result.stats.enumerated_placement_choice_upper_bound, 36);
  for (const ExactGriddedStripe& stripe : result.model.stripes) {
    ASSERT_EQ(stripe.initial_rows.size(), 3U);
    EXPECT_EQ(stripe.initial_rows[1].y, 2);
    EXPECT_EQ(stripe.initial_rows[2].y, 2);
  }
  ASSERT_EQ(result.model.cells.size(), 3U);
  for (const ExactGriddedCell& cell : result.model.cells) {
    EXPECT_EQ(cell.candidate_stripe_ids, (std::vector<int>{0, 1}));
    EXPECT_EQ(cell.initial_start_row, 0);
  }
  EXPECT_EQ(result.model.cells[0].initial_stripe_id, 0);
  EXPECT_EQ(result.model.cells[1].initial_stripe_id, 0);
  EXPECT_EQ(result.model.cells[2].initial_stripe_id, 1);
}

TEST(ExactGriddedWholeDesignModelBuilderTest,
     CanRestrictTheModelToCurrentStripesAndRowCounts) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(2, 0, 0);
  Macro* macro = circuit.AddMacro("cell", 2, 2);
  macro->AddWellRect(false, 0, 0, 2, 1);
  macro->AddWellRect(true, 0, 1, 2, 2);
  circuit.AddComponent("left", "cell", 0, 0, PLACED);
  circuit.AddComponent("right", "cell", 10, 0, PLACED);

  std::vector<StripeColumn> columns(2);
  for (int column_id = 0; column_id < 2; ++column_id) {
    Stripe& stripe = columns[column_id].stripe_list_.emplace_back();
    stripe.lx_ = column_id * 10;
    stripe.ly_ = 0;
    stripe.width_ = 10;
    stripe.height_ = 6;
    GriddedRow& row = stripe.gridded_rows_.emplace_back();
    row.SetLLX(stripe.LLX());
    row.SetLLY(0);
    row.SetWidth(stripe.Width());
    row.UpdateWellHeightUpward(1, 1);
  }
  columns[0].stripe_list_[0].gridded_rows_[0].AddComponent(
      circuit.GetComponentPtr("left"));
  columns[1].stripe_list_[0].gridded_rows_[0].AddComponent(
      circuit.GetComponentPtr("right"));

  ExactGriddedWholeDesignBuilderConfig config;
  config.minimum_p_well_height = 1;
  config.minimum_n_well_height = 1;
  config.allow_cross_stripe_moves = false;
  config.use_full_row_slot_capacity = false;
  ExactGriddedWholeDesignBuildResult result =
      ExactGriddedWholeDesignModelBuilder(&circuit, config).Build(&columns);

  EXPECT_EQ(result.stats.row_slot_count, 2);
  EXPECT_EQ(result.stats.enumerated_placement_choice_upper_bound, 4);
  ASSERT_EQ(result.model.cells.size(), 2U);
  EXPECT_EQ(result.model.cells[0].candidate_stripe_ids, (std::vector<int>{0}));
  EXPECT_EQ(result.model.cells[1].candidate_stripe_ids, (std::vector<int>{1}));
}

}  // namespace dali
