/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/gridded_row.h"

#include <gtest/gtest.h>

namespace dali {

TEST(GriddedRowTest, SynchronizesComponentOrientationIdempotently) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(1, 0, 0);
  Macro* macro = circuit.AddMacro("cell", 2, 2);
  macro->AddWellRect(false, 0, 0, 2, 1);
  macro->AddWellRect(true, 0, 1, 2, 2);
  circuit.AddComponent("component", "cell", 0, 10, PLACED);
  Component* component = circuit.GetComponentPtr("component");
  component->SetOrient(FS);

  GriddedRow row;
  row.SetLLY(10);
  row.SetHeight(4);
  row.AddComponent(component);

  row.SetOrient(true);
  EXPECT_TRUE(row.IsOrientN());
  EXPECT_EQ(component->Orient(), N);
  EXPECT_DOUBLE_EQ(component->LLY(), 12.0);

  row.SetOrient(true);
  EXPECT_EQ(component->Orient(), N);
  EXPECT_DOUBLE_EQ(component->LLY(), 12.0);

  row.SetOrient(false);
  EXPECT_FALSE(row.IsOrientN());
  EXPECT_EQ(component->Orient(), FS);
  EXPECT_DOUBLE_EQ(component->LLY(), 10.0);
}

TEST(GriddedRowTest, SynchronizesComponentYToFinalWellEdge) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(1, 0, 0);
  Macro* macro = circuit.AddMacro("cell", 2, 2);
  macro->AddWellRect(false, 0, 0, 2, 1);
  macro->AddWellRect(true, 0, 1, 2, 2);
  circuit.AddComponent("component", "cell", 0, 10, PLACED);
  Component* component = circuit.GetComponentPtr("component");

  GriddedRow row;
  row.SetLLY(10);
  row.UpdateWellHeightUpward(3, 2);
  row.AddComponent(component);

  row.SetOrient(true);
  component->SetLLY(10);
  row.UpdateComponentLocY();
  EXPECT_EQ(component->Orient(), N);
  EXPECT_DOUBLE_EQ(component->LLY(), 12.0);

  row.SetOrient(false);
  component->SetLLY(10);
  row.UpdateComponentLocY();
  EXPECT_EQ(component->Orient(), FS);
  EXPECT_DOUBLE_EQ(component->LLY(), 11.0);
}

TEST(GriddedRowTest, MinimizesFixedOrderXDisplacementWithinMargins) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(3, 0, 0);
  Macro* macro = circuit.AddMacro("cell", 6, 2);
  macro->AddWellRect(false, 0, 0, 6, 1);
  macro->AddWellRect(true, 0, 1, 6, 2);
  circuit.AddComponent("left", "cell", 6, 0, PLACED);
  circuit.AddComponent("middle", "cell", 8, 0, PLACED);
  circuit.AddComponent("right", "cell", 10, 0, PLACED);

  GriddedRow row;
  row.SetLLX(0);
  row.SetWidth(30);
  row.SetBoundaryMargins(2, 2);
  row.AddComponent(circuit.GetComponentPtr("left"));
  row.AddComponent(circuit.GetComponentPtr("middle"));
  row.AddComponent(circuit.GetComponentPtr("right"));

  row.MinDisplacementLegalization();

  // The fixed-order quadratic optimum starts at x=2: the transformed target
  // locations are 6, 2, and -2, whose mean is 2 after the left bound clamps
  // the overlapping cluster.
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("left")->LLX(), 2.0);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("middle")->LLX(), 8.0);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("right")->LLX(), 14.0);
  EXPECT_TRUE(row.HasLegalComponentPlacement());
}

}  // namespace dali
