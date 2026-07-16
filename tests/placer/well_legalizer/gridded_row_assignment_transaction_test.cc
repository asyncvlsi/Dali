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
#include "dali/placer/well_legalizer/gridded_row_assignment_transaction.h"

#include <gtest/gtest.h>

#include "dali/circuit/circuit.h"

namespace dali {

TEST(GriddedRowAssignmentTransactionTest, MeasuresAndRestoresTrialAssignment) {
  Circuit circuit;
  circuit.SetDatabaseMicrons(1000);
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(-100, 0, 110, 20);
  circuit.ReserveSpaceForDesignImp(2, 0, 1);

  Macro* cell = circuit.AddMacro("cell", 10, 10);
  ASSERT_NE(cell, nullptr);
  cell->AddWellRect(false, 0, 0, 10, 4);
  cell->AddWellRect(true, 0, 4, 10, 10);
  circuit.AddMacroPin(cell, "p", true)->SetOffset(5, 5);
  circuit.AddComponent("movable", "cell", 0, 0, PLACED);
  Component* movable = circuit.GetComponentPtr("movable");
  ASSERT_NE(movable, nullptr);
  circuit.AddComponent("anchor", "cell", 100, 0, FIXED);
  circuit.AddNet("net", 2);
  circuit.AddComponentPinToNet("movable", "p", "net");
  circuit.AddComponentPinToNet("anchor", "p", "net");

  GriddedRow source_row;
  source_row.SetLLX(-100);
  source_row.SetWidth(210);
  source_row.SetLLY(0);
  source_row.AddComponent(movable);
  source_row.SetUsedSize(13);
  GriddedRow target_row;
  target_row.SetLLX(-100);
  target_row.SetWidth(210);
  target_row.SetLLY(10);

  GriddedRowAssignmentTransaction improving_transaction(
      &circuit, {&source_row, &target_row});
  source_row.Components().clear();
  source_row.InitLocations().clear();
  source_row.SetUsedSize(0);
  target_row.Components().push_back(movable);
  target_row.InitLocations()[movable] = double2d(100, 10);
  target_row.SetUsedSize(10);
  movable->SetLLX(100);
  movable->SetLLY(10);
  movable->SetOrient(FS);
  EXPECT_GT(improving_transaction.HpwlImprovement(), 0);
  EXPECT_TRUE(improving_transaction.ImprovesHpwl(1e-9));
  improving_transaction.Restore();

  ASSERT_EQ(source_row.Components().size(), 1);
  EXPECT_EQ(source_row.Components().front(), movable);
  EXPECT_TRUE(target_row.Components().empty());
  EXPECT_EQ(source_row.InitLocations().size(), 1U);
  EXPECT_TRUE(target_row.InitLocations().empty());
  EXPECT_EQ(source_row.UsedSize(), 13);
  EXPECT_EQ(target_row.UsedSize(), 0);
  EXPECT_DOUBLE_EQ(movable->LLX(), 0);
  EXPECT_DOUBLE_EQ(movable->LLY(), 0);
  EXPECT_EQ(movable->Orient(), N);

  GriddedRowAssignmentTransaction worsening_transaction(
      &circuit, {&source_row, &target_row});
  source_row.Components().clear();
  target_row.Components().push_back(movable);
  movable->SetLLX(-100);
  movable->SetLLY(10);
  movable->SetOrient(FS);
  EXPECT_LT(worsening_transaction.HpwlImprovement(), 0);
  EXPECT_FALSE(worsening_transaction.ImprovesHpwl(1e-9));
  worsening_transaction.Restore();

  ASSERT_EQ(source_row.Components().size(), 1);
  EXPECT_EQ(source_row.Components().front(), movable);
  EXPECT_TRUE(target_row.Components().empty());
  EXPECT_DOUBLE_EQ(movable->LLX(), 0);
  EXPECT_DOUBLE_EQ(movable->LLY(), 0);
  EXPECT_EQ(movable->Orient(), N);
}

}  // namespace dali
