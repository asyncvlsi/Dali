#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_placement_model.h"

#include <gtest/gtest.h>

#include "dali/placer/detailed_placer/detailed_placer.h"
#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_legalizer.h"

namespace dali {

TEST(StandardCellPlacementModelTest, KeepsWholeRowFreeWithoutBlockages) {
  StandardCellPlacementModel model;

  model.AddRow(10, 20, 12, 2, 5);
  model.BuildFreeSegments();

  ASSERT_EQ(model.RowCount(), 1);
  ASSERT_EQ(model.Rows()[0].free_segments.size(), 1);
  EXPECT_EQ(model.Rows()[0].free_segments[0].lx, 10);
  EXPECT_EQ(model.Rows()[0].free_segments[0].ux, 20);
  EXPECT_TRUE(model.IsIntervalFree(0, 10, 20));
}

TEST(StandardCellPlacementModelTest, SplitsRowsAroundBlockages) {
  StandardCellPlacementModel model;

  model.AddRow(0, 0, 10, 2, 10);
  model.AddBlockage(5, -1, 11, 11);
  model.BuildFreeSegments();

  ASSERT_EQ(model.Rows()[0].free_segments.size(), 2);
  EXPECT_EQ(model.Rows()[0].free_segments[0].lx, 0);
  EXPECT_EQ(model.Rows()[0].free_segments[0].ux, 4);
  EXPECT_EQ(model.Rows()[0].free_segments[1].lx, 12);
  EXPECT_EQ(model.Rows()[0].free_segments[1].ux, 20);
  EXPECT_TRUE(model.IsIntervalFree(0, 12, 20));
  EXPECT_FALSE(model.IsIntervalFree(0, 4, 12));
}

TEST(StandardCellPlacementModelTest, ClipsBlockagesToTouchedRows) {
  StandardCellPlacementModel model;

  model.AddRow(0, 0, 10, 1, 10);
  model.AddRow(0, 10, 10, 1, 10);
  model.AddBlockage(3, 1, 6, 9);
  model.BuildFreeSegments();

  ASSERT_EQ(model.Rows()[0].free_segments.size(), 2);
  EXPECT_EQ(model.Rows()[1].free_segments.size(), 1);
  EXPECT_TRUE(model.IsIntervalFree(1, 0, 10));
}

TEST(StandardCellPlacementModelTest, FindsRowContainingY) {
  StandardCellPlacementModel model;

  model.AddRow(0, 100, 10, 1, 10);
  model.AddRow(0, 110, 10, 1, 10);

  ASSERT_TRUE(model.RowIndexAtY(100).has_value());
  EXPECT_EQ(*model.RowIndexAtY(100), 0);
  ASSERT_TRUE(model.RowIndexAtY(119).has_value());
  EXPECT_EQ(*model.RowIndexAtY(119), 1);
  EXPECT_FALSE(model.RowIndexAtY(120).has_value());
}

TEST(StandardCellPlacementModelTest, MixedHeightReservesEveryDefRow) {
  phydb::PhyDB phy_db;
  phy_db.AddSite("site", "CORE", 2, 4);
  phy_db.AddRow("upper_right", "site", "N", 17, 4, 12, 1, 2, 0);
  phy_db.AddRow("bottom", "site", "FS", 1, 0, 20, 1, 2, 0);
  phy_db.AddRow("upper_left", "site", "N", 3, 4, 5, 1, 2, 0);
  phy_db.AddRow("different_phase", "site", "N", 0, 8, 20, 1, 2, 0);

  Circuit circuit;
  circuit.SetDatabaseMicrons(1);
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetRowHeight(4);
  circuit.SetDieArea(0, 0, 42, 12);
  circuit.ReserveSpaceForDesignImp(5, 0, 1);
  circuit.AddMacro("tall", 6, 8);
  Macro* short_cell = circuit.AddMacro("short", 4, 4);
  circuit.AddMacroPin(short_cell, "p", true)->SetOffset(1, 1);
  circuit.AddComponent("stack", "tall", 3, 0, PLACED);
  circuit.AddComponent("lower", "short", 7, 0, PLACED);
  circuit.AddComponent("upper", "short", 7, 4, PLACED);
  circuit.AddComponent("other", "short", 21, 4, PLACED);
  circuit.AddComponent("obstacle", "short", 3, 4, FIXED);
  circuit.AddNet("pull", 2);
  circuit.AddComponentPinToNet("upper", "p", "pull");
  circuit.AddComponentPinToNet("other", "p", "pull");

  StandardCellLegalizer legalizer;
  legalizer.SetCircuit(&circuit);
  legalizer.SetMixedHeightRows(&phy_db);
  legalizer.SetCostMode(StandardCellLegalizerCostMode::kDisplacement);
  ASSERT_TRUE(legalizer.StartPlacement());
  ASSERT_TRUE(legalizer.UsesMixedHeightRows());
  EXPECT_EQ(legalizer.MixedHeightSiteWidth(), 2);
  auto* stack = circuit.GetComponentPtr("stack");
  EXPECT_EQ(stack->LLX(), 7);
  EXPECT_EQ(stack->LLY(), 0);
  EXPECT_EQ(stack->Orient(), FS);
  for (auto& row : circuit.design().Rows()) {
    if (row.LY() >= stack->URY()) {
      continue;
    }
    for (auto& segment : row.RowSegments()) {
      EXPECT_TRUE(segment.UX() <= stack->LLX() ||
                  segment.LX() >= stack->URX());
    }
  }

  DetailedPlacer detail;
  detail.CopyPlacementContextFrom(&legalizer);
  detail.SetFixedRowSiteWidth(legalizer.MixedHeightSiteWidth());
  detail.SetMaxOptimizationRounds(1);
  ASSERT_TRUE(detail.StartPlacement());
  EXPECT_TRUE(legalizer.ValidateMixedHeightPlacement());
  EXPECT_EQ(stack->LLX(), 7);
  EXPECT_EQ(stack->LLY(), 0);

  stack->SetLLX(5);
  EXPECT_FALSE(legalizer.ValidateMixedHeightPlacement());
}

TEST(StandardCellPlacementModelTest, MixedHeightCannotBridgeMissingBand) {
  phydb::PhyDB phy_db;
  phy_db.AddSite("site", "CORE", 1, 2);
  phy_db.AddRow("bottom", "site", "N", 0, 0, 10, 1, 1, 0);
  phy_db.AddRow("top", "site", "N", 0, 4, 10, 1, 1, 0);

  Circuit circuit;
  circuit.SetDatabaseMicrons(1);
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetRowHeight(2);
  circuit.SetDieArea(0, 0, 10, 6);
  circuit.ReserveSpaceForDesignImp(1, 0, 0);
  circuit.AddMacro("tall", 3, 4);
  circuit.AddComponent("stack", "tall", 2, 0, PLACED);

  StandardCellLegalizer legalizer;
  legalizer.SetCircuit(&circuit);
  legalizer.SetMixedHeightRows(&phy_db);
  EXPECT_FALSE(legalizer.StartPlacement());
  EXPECT_TRUE(legalizer.UsesMixedHeightRows());
  EXPECT_EQ(circuit.GetComponentPtr("stack")->LLX(), 2);
  EXPECT_EQ(circuit.GetComponentPtr("stack")->LLY(), 0);
}

}  // namespace dali
