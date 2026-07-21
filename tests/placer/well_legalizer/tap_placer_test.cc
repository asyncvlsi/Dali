/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/tap_placer.h"

#include <gtest/gtest.h>

namespace dali {

class TapPlacerTest : public testing::Test {
 protected:
  void SetUp() override {
    circuit.SetManufacturingGrid(1.0);
    circuit.SetUnitsDistanceMicrons(1);
    circuit.SetGridValue(1.0, 1.0);
    circuit.SetDieArea(0, 0, 100, 100);
    circuit.ReserveSpaceForDesignImp(1, 0, 0);
    circuit.AddMacro("cell", 10, 10);
    Macro* macro = circuit.GetMacroPtr("cell");
    macro->AddWellRect(false, 0, 0, 10, 4);
    macro->AddWellRect(true, 0, 4, 10, 10);
    circuit.AddWellTapMacro("tap", 3, 10);
    tap_macro = circuit.GetMacroPtr("tap");
    circuit.AddComponent("mid", "cell", 45, 20, PLACED);

    row.SetLLX(0);
    row.SetWidth(100);
    row.SetLLY(20);
    row.UpdateWellHeightUpward(4, 6);
    row.SetBoundaryMargins(9, 11);
    row.AddComponent(circuit.GetComponentPtr("mid"));
    row.SetOrient(true);

    ctx.well_tap_macro = tap_macro;
    ctx.pre_end_cap_width = 3;
    ctx.post_end_cap_width = 5;
    ctx.space_to_well_tap = 3;
  }

  Circuit circuit;
  Macro* tap_macro = nullptr;
  GriddedRow row;
  TapPlacementContext ctx;
};

TEST_F(TapPlacerTest, RowEndPlacesTwoTapsInMargins) {
  RowTapPlacer placer(TapPosition::kRowEnd, TapCadence::kEveryRow);
  EXPECT_EQ(placer.Name(), "row-end");
  placer.ValidateRow(row, ctx);  // Must not throw for a well-reserved row.

  const std::vector<double> centers = placer.RowTapCenters(row, 0, ctx);
  ASSERT_EQ(centers.size(), 2U);
  // Left center: LLX + pre_end_cap + tap_width/2 = 0 + 3 + 1.5.
  EXPECT_DOUBLE_EQ(centers[0], 4.5);
  // Right center: URX - post_end_cap - tap_width/2 = 100 - 5 - 1.5.
  EXPECT_DOUBLE_EQ(centers[1], 93.5);
}

TEST_F(TapPlacerTest, RowEndCountIsRowIndexIndependent) {
  RowTapPlacer placer(TapPosition::kRowEnd, TapCadence::kEveryRow);
  EXPECT_EQ(placer.RowTapCenters(row, 0, ctx).size(), 2U);
  EXPECT_EQ(placer.RowTapCenters(row, 1, ctx).size(), 2U);
  EXPECT_EQ(placer.RowTapCenters(row, 7, ctx).size(), 2U);
}

TEST_F(TapPlacerTest, RowMidPlacesOneCenteredTap) {
  RowTapPlacer placer(TapPosition::kRowMid, TapCadence::kEveryRow);
  EXPECT_EQ(placer.Name(), "row-mid");
  const std::vector<double> centers = placer.RowTapCenters(row, 0, ctx);
  ASSERT_EQ(centers.size(), 1U);
  // Center of the row: (LLX + URX) / 2 = (0 + 100) / 2.
  EXPECT_DOUBLE_EQ(centers[0], 50.0);
}

TEST_F(TapPlacerTest, EveryOtherCadenceSkipsOddRows) {
  RowTapPlacer placer(TapPosition::kRowEnd, TapCadence::kEveryOtherRow);
  EXPECT_EQ(placer.Name(), "row-end-every-other");

  // Even rows keep the row-end pair; odd rows get none.
  EXPECT_EQ(placer.RowTapCenters(row, 0, ctx).size(), 2U);
  EXPECT_TRUE(placer.RowTapCenters(row, 1, ctx).empty());
  EXPECT_EQ(placer.RowTapCenters(row, 2, ctx).size(), 2U);
  EXPECT_TRUE(placer.RowTapCenters(row, 3, ctx).empty());

  // Even-row positions match the every-row pattern exactly.
  const std::vector<double> even = placer.RowTapCenters(row, 0, ctx);
  const std::vector<double> baseline =
      RowTapPlacer(TapPosition::kRowEnd, TapCadence::kEveryRow)
          .RowTapCenters(row, 0, ctx);
  EXPECT_EQ(even, baseline);
}

TEST(WellTapPatternTest, ParseRoundTripsCanonicalNames) {
  EXPECT_EQ(ParseWellTapPattern("row-end"), WellTapPattern::kRowEnd);
  EXPECT_EQ(ParseWellTapPattern("row_end"), WellTapPattern::kRowEnd);
  EXPECT_EQ(ParseWellTapPattern("row-end-every-other"),
            WellTapPattern::kRowEndEveryOther);
  EXPECT_EQ(ParseWellTapPattern("row-mid"), WellTapPattern::kRowMid);
  // Legacy name is still accepted as an alias.
  EXPECT_EQ(ParseWellTapPattern("every-other-row"),
            WellTapPattern::kRowEndEveryOther);
  EXPECT_EQ(ParseWellTapPattern("every_other_row"),
            WellTapPattern::kRowEndEveryOther);
  // Unknown names fall back to the safe default.
  EXPECT_EQ(ParseWellTapPattern("nonsense"), WellTapPattern::kRowEnd);

  EXPECT_EQ(WellTapPatternName(WellTapPattern::kRowEnd), "row-end");
  EXPECT_EQ(WellTapPatternName(WellTapPattern::kRowEndEveryOther),
            "row-end-every-other");
  EXPECT_EQ(WellTapPatternName(WellTapPattern::kRowMid), "row-mid");
}

TEST(WellTapPatternTest, PatternDecomposesIntoAxes) {
  EXPECT_EQ(PositionOf(WellTapPattern::kRowEnd), TapPosition::kRowEnd);
  EXPECT_EQ(CadenceOf(WellTapPattern::kRowEnd), TapCadence::kEveryRow);
  EXPECT_EQ(PositionOf(WellTapPattern::kRowEndEveryOther), TapPosition::kRowEnd);
  EXPECT_EQ(CadenceOf(WellTapPattern::kRowEndEveryOther),
            TapCadence::kEveryOtherRow);
  EXPECT_EQ(PositionOf(WellTapPattern::kRowMid), TapPosition::kRowMid);
  EXPECT_EQ(CadenceOf(WellTapPattern::kRowMid), TapCadence::kEveryRow);
}

TEST(WellTapPatternTest, FactoryBuildsMatchingStrategy) {
  EXPECT_EQ(CreateTapPlacer(WellTapPattern::kRowEnd)->Name(), "row-end");
  EXPECT_EQ(CreateTapPlacer(WellTapPattern::kRowEndEveryOther)->Name(),
            "row-end-every-other");
  EXPECT_EQ(CreateTapPlacer(WellTapPattern::kRowMid)->Name(), "row-mid");
}

TEST(WellTapPatternTest, FixedCountForEveryRowCadence) {
  EXPECT_TRUE(WellTapPatternHasFixedCount(WellTapPattern::kRowEnd));
  EXPECT_TRUE(WellTapPatternHasFixedCount(WellTapPattern::kRowMid));
  EXPECT_FALSE(WellTapPatternHasFixedCount(WellTapPattern::kRowEndEveryOther));
}

TEST(WellTapPatternTest, TryParseRejectsUnknownNames) {
  WellTapPattern pattern = WellTapPattern::kRowEndEveryOther;
  EXPECT_TRUE(TryParseWellTapPattern("row-end", &pattern));
  EXPECT_EQ(pattern, WellTapPattern::kRowEnd);
  // Unknown names leave the out-param untouched and report failure.
  EXPECT_FALSE(TryParseWellTapPattern("checkerboard", &pattern));
  EXPECT_EQ(pattern, WellTapPattern::kRowEnd);
  EXPECT_FALSE(TryParseWellTapPattern("nonsense", &pattern));
}

TEST(WellTapPatternTest, SupportReflectsEndToEndReadiness) {
  // "Supported" is independent of tap count: row-end runs today; the other
  // known patterns are planned but not yet end-to-end.
  EXPECT_TRUE(IsWellTapPatternSupported(WellTapPattern::kRowEnd));
  EXPECT_FALSE(IsWellTapPatternSupported(WellTapPattern::kRowEndEveryOther));
  EXPECT_FALSE(IsWellTapPatternSupported(WellTapPattern::kRowMid));
}

TEST(WellTapPatternTest, RegistryListsAreConsistent) {
  const std::vector<std::string>& known = KnownWellTapPatternNames();
  // Every known name round-trips through the parser.
  for (const std::string& name : known) {
    WellTapPattern pattern;
    EXPECT_TRUE(TryParseWellTapPattern(name, &pattern)) << name;
    EXPECT_EQ(WellTapPatternName(pattern), name);
  }
  // The supported list is the subset of known names that run end-to-end.
  EXPECT_EQ(SupportedWellTapPatternList(), "row-end");
}

}  // namespace dali
