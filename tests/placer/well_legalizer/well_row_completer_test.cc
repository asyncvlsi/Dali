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
#include "dali/placer/well_legalizer/well_row_completer.h"

#include <gtest/gtest.h>

namespace dali {

TEST(WellRowCompleterTest, RowLegalizationPreservesBoundaryMargins) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1.0);
  circuit.SetGridValue(1.0, 1.0);
  circuit.AddMacro("ordinary", 5.0, 10.0);
  Macro* ordinary_macro = circuit.GetMacroPtr("ordinary");
  ASSERT_NE(ordinary_macro, nullptr);
  ordinary_macro->AddWellRect(false, 0, 0, 5, 4);
  ordinary_macro->AddWellRect(true, 0, 4, 5, 10);
  circuit.ReserveSpaceForDesignImp(2, 0, 0);
  circuit.AddComponent("left", "ordinary", 10, 20, PLACED);
  circuit.AddComponent("right", "ordinary", 105, 20, PLACED);

  GriddedRow row;
  row.SetLLX(10);
  row.SetWidth(100);
  row.SetBoundaryMargins(7, 7);
  row.AddComponent(circuit.GetComponentPtr("left"));
  row.AddComponent(circuit.GetComponentPtr("right"));

  row.LegalizeLooseX();

  EXPECT_GE(circuit.GetComponentPtr("left")->LLX(), 17);
  EXPECT_LE(circuit.GetComponentPtr("right")->URX(), 103);

  circuit.GetComponentPtr("left")->SetLLX(10);
  circuit.GetComponentPtr("right")->SetLLX(105);
  row.MinDisplacementLegalization();

  EXPECT_GE(circuit.GetComponentPtr("left")->LLX(), 17);
  EXPECT_LE(circuit.GetComponentPtr("right")->URX(), 103);
}

TEST(WellRowCompleterTest, PlacesBoundaryCellsOutsideOrdinaryCellSpace) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1.0);
  circuit.SetGridValue(1.0, 1.0);
  circuit.AddMacro("ordinary", 5.0, 10.0);
  circuit.AddWellTapMacro("well_tap", 2.0, 10.0);

  Macro* ordinary_macro = circuit.GetMacroPtr("ordinary");
  Macro* tap_macro = circuit.GetMacroPtr("well_tap");
  ASSERT_NE(ordinary_macro, nullptr);
  ASSERT_NE(tap_macro, nullptr);
  ordinary_macro->AddWellRect(false, 0, 0, 5, 4);
  ordinary_macro->AddWellRect(true, 0, 4, 5, 10);
  tap_macro->AddWellRect(false, 0, 0, 2, 4);
  tap_macro->AddWellRect(true, 0, 4, 2, 10);

  circuit.ReserveSpaceForDesignImp(1, 0, 0);
  circuit.AddComponent("cell", "ordinary", 10, 20, PLACED);

  std::vector<StripeColumn> columns(1);
  columns.front().stripe_list_.resize(1);
  Stripe& stripe = columns.front().stripe_list_.front();
  stripe.gridded_rows_.resize(1);
  GriddedRow& row = stripe.gridded_rows_.front();
  row.SetLLX(10);
  row.SetWidth(100);
  row.SetLLY(20);
  row.UpdateWellHeightUpward(4, 6);
  row.SetBoundaryMargins(7, 7);
  row.AddComponent(circuit.GetComponentPtr("cell"));

  WellRowCompletionConfig config;
  config.well_tap_macro = tap_macro;
  config.space_to_well_tap = 3;
  config.pre_end_cap_width = 2;
  config.post_end_cap_width = 2;

  WellRowCompleter completer(&circuit, &columns, config);
  row.LegalizeLooseX();
  const double ordinary_lx_before_completion =
      circuit.GetComponentPtr("cell")->LLX();
  const double ordinary_ly_before_completion =
      circuit.GetComponentPtr("cell")->LLY();
  const ComponentOrient ordinary_orient_before_completion =
      circuit.GetComponentPtr("cell")->Orient();
  completer.InsertWellTaps();
  completer.InsertEndCaps();

  ASSERT_EQ(circuit.design().WellTaps().size(), 2);
  ASSERT_EQ(circuit.design().EndCapComponentCollection().GetSize(), 2);

  const Component& left_tap = circuit.design().WellTaps()[0];
  const Component& right_tap = circuit.design().WellTaps()[1];
  EXPECT_DOUBLE_EQ(left_tap.LLX(), 12);
  EXPECT_DOUBLE_EQ(right_tap.URX(), 108);

  const auto& end_caps =
      circuit.design().EndCapComponentCollection().Instances();
  EXPECT_DOUBLE_EQ(end_caps[0].LLX(), 10);
  EXPECT_DOUBLE_EQ(end_caps[0].URX(), left_tap.LLX());
  EXPECT_DOUBLE_EQ(end_caps[1].LLX(), right_tap.URX());
  EXPECT_DOUBLE_EQ(end_caps[1].URX(), 110);

  const Component* ordinary = circuit.GetComponentPtr("cell");
  ASSERT_NE(ordinary, nullptr);
  EXPECT_DOUBLE_EQ(ordinary->LLX(), ordinary_lx_before_completion);
  EXPECT_DOUBLE_EQ(ordinary->LLY(), ordinary_ly_before_completion);
  EXPECT_EQ(ordinary->Orient(), ordinary_orient_before_completion);
  EXPECT_GE(ordinary->LLX(), left_tap.URX() + config.space_to_well_tap);
  EXPECT_LE(ordinary->URX(), right_tap.LLX() - config.space_to_well_tap);
}

}  // namespace dali
