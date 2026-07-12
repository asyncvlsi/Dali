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
#include "dali/placer/well_legalizer/well_geometry.h"

#include <gtest/gtest.h>

#include "dali/placer/well_legalizer/well_row_completer.h"

namespace dali {

TEST(WellGeometryBuilderTest, UsesFinalTapColumnsForImplantGeometry) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1.0);
  circuit.SetGridValue(1.0, 1.0);
  circuit.AddWellTapMacro("well_tap", 2.0, 10.0);
  Macro* tap_macro = circuit.GetMacroPtr("well_tap");
  ASSERT_NE(tap_macro, nullptr);
  tap_macro->AddWellRect(false, 0, 0, 2, 4);
  tap_macro->AddWellRect(true, 0, 4, 2, 10);

  std::vector<StripeColumn> columns(1);
  columns.front().stripe_list_.resize(1);
  Stripe& stripe = columns.front().stripe_list_.front();
  stripe.lx_ = 10;
  stripe.width_ = 100;
  stripe.is_bottom_up_ = true;
  stripe.is_first_row_orient_N_ = true;
  stripe.gridded_rows_.resize(1);
  GriddedRow& row = stripe.gridded_rows_.front();
  row.SetLLX(10);
  row.SetWidth(100);
  row.SetLLY(20);
  row.UpdateWellHeightUpward(4, 6);
  row.SetBoundaryMargins(7, 7);

  WellRowCompletionConfig config;
  config.well_tap_macro = tap_macro;
  config.space_to_well_tap = 3;
  config.pre_end_cap_width = 2;
  config.post_end_cap_width = 2;
  WellRowCompleter(&circuit, &columns, config).InsertWellTaps();

  WellGeometryBuilder builder(columns, 20, 30);
  std::vector<WellGeometryRect> geometry = builder.Build(true);

  int pwell_count = 0;
  int nwell_count = 0;
  int pplus_count = 0;
  int nplus_count = 0;
  for (const WellGeometryRect& rect : geometry) {
    switch (rect.layer) {
      case WellGeometryLayer::kPwell:
        ++pwell_count;
        break;
      case WellGeometryLayer::kNwell:
        ++nwell_count;
        break;
      case WellGeometryLayer::kPplus:
        ++pplus_count;
        EXPECT_EQ(rect.bounds.LLX(), 14);
        EXPECT_EQ(rect.bounds.URX(), 106);
        break;
      case WellGeometryLayer::kNplus:
        ++nplus_count;
        EXPECT_EQ(rect.bounds.LLX(), 14);
        EXPECT_EQ(rect.bounds.URX(), 106);
        break;
    }
  }

  EXPECT_EQ(pwell_count, 1);
  EXPECT_EQ(nwell_count, 1);
  EXPECT_EQ(pplus_count, 1);
  EXPECT_EQ(nplus_count, 1);
}

}  // namespace dali
