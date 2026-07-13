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
#include "dali/placer/well_legalizer/gridded_detailed_placer.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace dali {

TEST(GriddedDetailedPlacerTest, VerticalSwapKeepsComponentsInTheirStripe) {
  Circuit circuit;
  circuit.SetDatabaseMicrons(1000);
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 110, 20);
  circuit.ReserveSpaceForDesignImp(8, 0, 4);

  Macro* cell = circuit.AddMacro("cell", 10, 10);
  ASSERT_NE(cell, nullptr);
  cell->AddWellRect(false, 0, 0, 10, 4);
  cell->AddWellRect(true, 0, 4, 10, 10);
  circuit.AddMacroPin(cell, "p", true)->SetOffset(5, 5);

  const std::vector<std::string> component_names = {"a", "b", "c", "d"};
  const std::vector<std::string> anchor_names = {"anchor_a", "anchor_b",
                                                 "anchor_c", "anchor_d"};
  const std::vector<int> component_x = {0, 100, 0, 100};
  const std::vector<int> anchor_x = {100, 0, 0, 100};
  const std::vector<int> component_y = {0, 0, 10, 10};
  for (size_t i = 0; i < component_names.size(); ++i) {
    circuit.AddComponent(component_names[i], "cell", component_x[i],
                         component_y[i], PLACED);
    circuit.AddComponent(anchor_names[i], "cell", anchor_x[i], component_y[i],
                         FIXED);
  }
  for (size_t i = 0; i < component_names.size(); ++i) {
    const std::string net_name = "net_" + component_names[i];
    circuit.AddNet(net_name, 2);
    circuit.AddComponentPinToNet(component_names[i], "p", net_name);
    circuit.AddComponentPinToNet(anchor_names[i], "p", net_name);
  }

  std::vector<GriddedRow> rows(4);
  for (size_t i = 0; i < rows.size(); ++i) {
    rows[i].SetLLX(component_x[i]);
    rows[i].SetWidth(10);
    rows[i].SetLLY(component_y[i]);
    rows[i].UpdateWellHeightUpward(4, 6);
    rows[i].SetOrient(true);
    rows[i].AddComponent(circuit.GetComponentPtr(component_names[i]));
  }

  std::vector<GriddedRow*> row_pointers;
  for (GriddedRow& row : rows) {
    row_pointers.push_back(&row);
  }
  GriddedDetailedPlacer placer;
  placer.SetCircuit(&circuit);
  placer.SetRows(row_pointers);
  ASSERT_TRUE(placer.StartPlacement());

  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("a")->LLX(), 0);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("b")->LLX(), 100);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("c")->LLX(), 0);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("d")->LLX(), 100);
}

}  // namespace dali
