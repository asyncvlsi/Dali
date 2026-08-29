/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/gridded_placement_validator.h"

#include <gtest/gtest.h>

#include "dali/placer/well_legalizer/well_row_completer.h"

namespace dali {

class GriddedPlacementValidatorTest : public testing::Test {
 protected:
  void SetUp() override {
    circuit.SetManufacturingGrid(1.0);
    circuit.SetUnitsDistanceMicrons(1);
    circuit.SetGridValue(1.0, 1.0);
    circuit.SetDieArea(0, 0, 100, 100);
    circuit.ReserveSpaceForDesignImp(2, 0, 0);
    circuit.AddMacro("cell", 10, 10);
    Macro* macro = circuit.GetMacroPtr("cell");
    macro->AddWellRect(false, 0, 0, 10, 4);
    macro->AddWellRect(true, 0, 4, 10, 10);
    circuit.AddWellTapMacro("tap", 3, 10);
    Macro* tap = circuit.GetMacroPtr("tap");
    tap->AddWellRect(false, 0, 0, 3, 4);
    tap->AddWellRect(true, 0, 4, 3, 10);
    circuit.AddComponent("left", "cell", 10, 20, PLACED);
    circuit.AddComponent("right", "cell", 25, 20, PLACED);

    columns.resize(1);
    columns.front().lx_ = 0;
    columns.front().width_ = 100;
    columns.front().stripe_list_.resize(1);
    Stripe& stripe = columns.front().stripe_list_.front();
    stripe.lx_ = 0;
    stripe.ly_ = 0;
    stripe.width_ = 100;
    stripe.height_ = 100;
    stripe.gridded_rows_.resize(1);
    GriddedRow& row = stripe.gridded_rows_.front();
    row.SetLLX(0);
    row.SetWidth(100);
    row.SetLLY(20);
    row.UpdateWellHeightUpward(4, 6);
    row.AddComponent(circuit.GetComponentPtr("left"));
    row.AddComponent(circuit.GetComponentPtr("right"));
    row.SetOrient(true);
  }

  GriddedRow& Row() {
    return columns.front().stripe_list_.front().gridded_rows_.front();
  }

  Circuit circuit;
  std::vector<StripeColumn> columns;
};

TEST_F(GriddedPlacementValidatorTest, AcceptsLegalSingleRegionPlacement) {
  const GriddedPlacementLegalityReport report =
      GriddedPlacementValidator(&circuit, &columns).Validate();

  EXPECT_TRUE(report.IsLegal());
  EXPECT_EQ(report.movable_component_count, 2U);
  EXPECT_EQ(report.assigned_component_count, 2U);
}

TEST_F(GriddedPlacementValidatorTest, RejectsOverlapAndMissingOwnership) {
  circuit.GetComponentPtr("right")->SetLLX(15);
  Row().Components().pop_back();

  GriddedPlacementLegalityReport missing =
      GriddedPlacementValidator(&circuit, &columns).Validate();
  EXPECT_FALSE(missing.IsLegal());
  EXPECT_EQ(missing.unassigned_component_count, 1U);

  Row().AddComponent(circuit.GetComponentPtr("right"));
  Row().SetOrient(true);
  GriddedPlacementLegalityReport overlap =
      GriddedPlacementValidator(&circuit, &columns).Validate();
  EXPECT_FALSE(overlap.IsLegal());
  EXPECT_EQ(overlap.component_overlap_count, 1U);
}

// Rows in different stripes of the same column sit side by side, so they share
// a y range as a matter of course. Only rows that also share x can collide.
TEST_F(GriddedPlacementValidatorTest, AcceptsSideBySideRowsSharingAYRange) {
  columns.front().stripe_list_.resize(2);
  Stripe& right_stripe = columns.front().stripe_list_.back();
  right_stripe.lx_ = 120;
  right_stripe.ly_ = 0;
  right_stripe.width_ = 100;
  right_stripe.height_ = 100;
  right_stripe.gridded_rows_.resize(1);
  GriddedRow& right_row = right_stripe.gridded_rows_.front();
  right_row.SetLLX(120);
  right_row.SetWidth(100);
  // deliberately offset by one, so the two rows overlap in y but not in x,
  // which is the arrangement the timing-driven flows produce
  right_row.SetLLY(21);
  right_row.UpdateWellHeightUpward(4, 6);
  right_row.SetOrient(true);

  const GriddedPlacementLegalityReport report =
      GriddedPlacementValidator(&circuit, &columns).Validate();
  EXPECT_EQ(report.row_overlap_count, 0u);
}

// The same y overlap between rows that do share x is a real collision.
TEST_F(GriddedPlacementValidatorTest, RejectsStackedRowsSharingAnXRange) {
  Stripe& stripe = columns.front().stripe_list_.front();
  stripe.gridded_rows_.resize(2);
  GriddedRow& upper = stripe.gridded_rows_.back();
  upper.SetLLX(0);
  upper.SetWidth(100);
  upper.SetLLY(21);
  upper.UpdateWellHeightUpward(4, 6);
  upper.SetOrient(true);

  const GriddedPlacementLegalityReport report =
      GriddedPlacementValidator(&circuit, &columns).Validate();
  EXPECT_EQ(report.row_overlap_count, 1u);
}

TEST_F(GriddedPlacementValidatorTest, RejectsWrongRowOrientation) {
  circuit.GetComponentPtr("left")->SetOrient(FS);

  const GriddedPlacementLegalityReport report =
      GriddedPlacementValidator(&circuit, &columns).Validate();

  EXPECT_FALSE(report.IsLegal());
  EXPECT_EQ(report.component_orientation_violation_count, 1U);
}

TEST_F(GriddedPlacementValidatorTest, ValidatesTapAndEndCapCompletion) {
  Macro* tap = circuit.GetMacroPtr("tap");
  ASSERT_NE(tap, nullptr);
  Row().SetBoundaryMargins(9, 11);
  Row().LegalizeLooseX();

  WellRowCompletionConfig completion;
  completion.well_tap_macro = tap;
  completion.space_to_well_tap = 3;
  completion.pre_end_cap_width = 3;
  completion.post_end_cap_width = 5;
  WellRowCompleter(&circuit, &columns, completion).InsertWellTaps();
  WellRowCompleter(&circuit, &columns, completion).InsertEndCaps();

  GriddedPlacementValidationConfig validation;
  validation.expect_well_taps = true;
  validation.expect_end_caps = true;
  validation.space_to_well_tap = 3;
  validation.pre_end_cap_width = 3;
  validation.post_end_cap_width = 5;
  const GriddedPlacementLegalityReport report =
      GriddedPlacementValidator(&circuit, &columns, validation).Validate();

  EXPECT_TRUE(report.IsLegal());
  EXPECT_EQ(report.physical_completion_violation_count, 0U);
  ASSERT_NE(Row().LeftWellTapCell(), nullptr);
  ASSERT_NE(Row().RightWellTapCell(), nullptr);
  EXPECT_DOUBLE_EQ(Row().LeftWellTapCell()->LLX(), 3.0);
  EXPECT_DOUBLE_EQ(Row().RightWellTapCell()->URX(), 95.0);
  const auto& end_caps =
      circuit.design().EndCapComponentCollection().Instances();
  ASSERT_EQ(end_caps.size(), 2U);
  EXPECT_DOUBLE_EQ(end_caps[0].LLX(), 0.0);
  EXPECT_DOUBLE_EQ(end_caps[1].URX(), 100.0);

  Row().LeftWellTapCell()->SetLLX(4.0);
  const GriddedPlacementLegalityReport drifted_report =
      GriddedPlacementValidator(&circuit, &columns, validation).Validate();
  EXPECT_FALSE(drifted_report.IsLegal());
  EXPECT_EQ(drifted_report.well_tap_geometry_violation_count, 1U);
}

TEST_F(GriddedPlacementValidatorTest, SparsePatternAllowsUntappedRows) {
  // The row carries no taps. The strict (row-end) expectation flags it as
  // missing taps; a sparse pattern that relies on coverage does not.
  GriddedPlacementValidationConfig strict;
  strict.expect_well_taps = true;
  const GriddedPlacementLegalityReport strict_report =
      GriddedPlacementValidator(&circuit, &columns, strict).Validate();
  EXPECT_GE(strict_report.missing_well_tap_count, 1U);

  GriddedPlacementValidationConfig sparse;
  sparse.expect_well_taps = true;
  sparse.require_taps_every_row = false;
  sparse.check_exact_well_tap_count = false;
  const GriddedPlacementLegalityReport sparse_report =
      GriddedPlacementValidator(&circuit, &columns, sparse).Validate();
  EXPECT_EQ(sparse_report.missing_well_tap_count, 0U);
  EXPECT_EQ(sparse_report.physical_component_count_violation_count, 0U);
}

TEST_F(GriddedPlacementValidatorTest, VerifiesWellTapCoverageAgainstMaxPlugDist) {
  Macro* tap = circuit.GetMacroPtr("tap");
  ASSERT_NE(tap, nullptr);
  Row().SetBoundaryMargins(9, 11);
  Row().LegalizeLooseX();

  WellRowCompletionConfig completion;
  completion.well_tap_macro = tap;
  completion.space_to_well_tap = 3;
  completion.pre_end_cap_width = 3;
  completion.post_end_cap_width = 5;
  WellRowCompleter(&circuit, &columns, completion).InsertWellTaps();
  ASSERT_FALSE(Row().TapCells().empty());

  // A generous latch-up budget covers every cell; the worst gap is still
  // reported as a positive diagnostic.
  GriddedPlacementValidationConfig covered;
  covered.check_well_tap_coverage = true;
  covered.max_plug_dist = 100.0;
  const GriddedPlacementLegalityReport covered_report =
      GriddedPlacementValidator(&circuit, &columns, covered).Validate();
  EXPECT_TRUE(covered_report.IsLegal());
  EXPECT_TRUE(covered_report.IsWellTapCoverageLegal());
  EXPECT_EQ(covered_report.well_tap_coverage_violation_count, 0U);
  EXPECT_GT(covered_report.max_well_tap_coverage_gap, 0.0);

  // A tiny budget leaves interior cells under-tapped, independent of the
  // row-end tap-spacing check.
  GriddedPlacementValidationConfig starved;
  starved.check_well_tap_coverage = true;
  starved.max_plug_dist = 1.0;
  const GriddedPlacementLegalityReport starved_report =
      GriddedPlacementValidator(&circuit, &columns, starved).Validate();
  EXPECT_FALSE(starved_report.IsLegal());
  EXPECT_FALSE(starved_report.IsWellTapCoverageLegal());
  EXPECT_GE(starved_report.well_tap_coverage_violation_count, 1U);
}

}  // namespace dali
