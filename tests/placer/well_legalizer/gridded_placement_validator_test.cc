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
