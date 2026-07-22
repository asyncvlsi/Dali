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

#include <algorithm>

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

// A sparse (every-other-row) pattern leaves the tap columns of untapped rows
// empty. The P+/N+ select layer must stay continuous over them, so the builder
// fills those spans, split at the P/N well boundary.
TEST(WellGeometryBuilderTest, FillsTapColumnsOfUntappedRowsContinuously) {
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
  stripe.gridded_rows_.resize(2);
  for (int i = 0; i < 2; ++i) {
    GriddedRow& row = stripe.gridded_rows_[i];
    row.SetLLX(10);
    row.SetWidth(100);
    row.SetLLY(20 + 10 * i);
    row.UpdateWellHeightUpward(4, 6);
    row.SetBoundaryMargins(7, 7);
  }

  WellRowCompletionConfig config;
  config.well_tap_macro = tap_macro;
  config.space_to_well_tap = 3;
  config.pre_end_cap_width = 2;
  config.post_end_cap_width = 2;
  RowTapPlacer sparse(TapPosition::kRowEnd, TapCadence::kEveryOtherRow);
  config.tap_placer = &sparse;
  WellRowCompleter(&circuit, &columns, config).InsertWellTaps();

  // Row 0 is tapped, row 1 (Y 30..40) is not.
  ASSERT_NE(stripe.gridded_rows_[0].WellTapCell(), nullptr);
  ASSERT_EQ(stripe.gridded_rows_[1].WellTapCell(), nullptr);

  WellGeometryBuilder builder(columns, 20, 40);
  std::vector<WellGeometryRect> geometry = builder.Build(true);

  // Collect implant rects sitting in the left tap column (X 12..14).
  std::vector<WellGeometryRect> left_column;
  for (const WellGeometryRect& rect : geometry) {
    const bool is_implant = rect.layer == WellGeometryLayer::kPplus ||
                            rect.layer == WellGeometryLayer::kNplus;
    if (is_implant && rect.bounds.LLX() == 12 && rect.bounds.URX() == 14) {
      left_column.push_back(rect);
    }
  }
  std::sort(left_column.begin(), left_column.end(),
            [](const WellGeometryRect& a, const WellGeometryRect& b) {
              return a.bounds.LLY() < b.bounds.LLY();
            });

  // Only the untapped row needs filling, split at its P/N edge (Y=34).
  ASSERT_EQ(left_column.size(), 2U);
  EXPECT_EQ(left_column[0].bounds.LLY(), 30);
  EXPECT_EQ(left_column[0].bounds.URY(), 34);
  EXPECT_EQ(left_column[0].layer, WellGeometryLayer::kNplus);
  EXPECT_EQ(left_column[1].bounds.LLY(), 34);
  EXPECT_EQ(left_column[1].bounds.URY(), 40);
  EXPECT_EQ(left_column[1].layer, WellGeometryLayer::kPplus);

  // The fill is gap-free across the untapped row.
  EXPECT_EQ(left_column[0].bounds.URY(), left_column[1].bounds.LLY());

  // The right tap column (X 106..108) is filled identically.
  int right_column_count = 0;
  for (const WellGeometryRect& rect : geometry) {
    const bool is_implant = rect.layer == WellGeometryLayer::kPplus ||
                            rect.layer == WellGeometryLayer::kNplus;
    if (is_implant && rect.bounds.LLX() == 106 && rect.bounds.URX() == 108) {
      ++right_column_count;
    }
  }
  EXPECT_EQ(right_column_count, 2);
}

}  // namespace dali
