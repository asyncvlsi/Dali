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

TEST(GriddedDetailedPlacerTest, GlobalSwapImprovesHpwlWithinRowBounds) {
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
  const double hpwl_before = circuit.WeightedHPWL();
  ASSERT_TRUE(placer.StartPlacement());

  EXPECT_LT(circuit.WeightedHPWL(), hpwl_before);
  for (const GriddedRow& row : rows) {
    for (const Component* component : row.Components()) {
      EXPECT_GE(component->LLX(), row.LLX() + row.LeftBoundaryMargin());
      EXPECT_LE(component->URX(), row.URX() - row.RightBoundaryMargin());
    }
  }
}

TEST(GriddedDetailedPlacerTest, RelocationUsesLegalWhitespaceToImproveHpwl) {
  Circuit circuit;
  circuit.SetDatabaseMicrons(1000);
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 30, 20);
  circuit.ReserveSpaceForDesignImp(6, 0, 3);

  Macro* cell = circuit.AddMacro("cell", 10, 10);
  ASSERT_NE(cell, nullptr);
  cell->AddWellRect(false, 0, 0, 10, 4);
  cell->AddWellRect(true, 0, 4, 10, 10);
  circuit.AddMacroPin(cell, "p", true)->SetOffset(5, 5);

  circuit.AddComponent("move", "cell", 0, 0, PLACED);
  circuit.AddComponent("stay", "cell", 10, 0, PLACED);
  circuit.AddComponent("target", "cell", 20, 10, PLACED);
  circuit.AddComponent("move_anchor", "cell", 0, 10, FIXED);
  circuit.AddComponent("stay_anchor", "cell", 10, 0, FIXED);
  circuit.AddComponent("target_anchor", "cell", 20, 10, FIXED);

  const std::vector<std::string> movable_names = {"move", "stay", "target"};
  for (const std::string& name : movable_names) {
    const std::string net_name = name + "_net";
    circuit.AddNet(net_name, 2);
    circuit.AddComponentPinToNet(name, "p", net_name);
    circuit.AddComponentPinToNet(name + "_anchor", "p", net_name);
  }

  std::vector<GriddedRow> rows(2);
  for (size_t i = 0; i < rows.size(); ++i) {
    rows[i].SetLLX(0);
    rows[i].SetWidth(30);
    rows[i].SetLLY(static_cast<int>(i) * 10);
    rows[i].UpdateWellHeightUpward(4, 6);
    rows[i].SetOrient(true);
  }
  rows[0].AddComponent(circuit.GetComponentPtr("move"));
  rows[0].AddComponent(circuit.GetComponentPtr("stay"));
  rows[1].AddComponent(circuit.GetComponentPtr("target"));

  GriddedDetailedPlacer placer;
  placer.SetCircuit(&circuit);
  placer.SetRows({&rows[0], &rows[1]});
  placer.SetEnableRelocation(true);
  placer.SetEnableVerticalSwap(false);
  placer.SetMaxRounds(1);
  const double hpwl_before = circuit.WeightedHPWL();
  ASSERT_TRUE(placer.StartPlacement());

  EXPECT_DOUBLE_EQ(circuit.WeightedHPWL(), 0);
  EXPECT_LT(circuit.WeightedHPWL(), hpwl_before);
  EXPECT_EQ(rows[0].Components().size(), 1);
  EXPECT_EQ(rows[1].Components().size(), 2);
  EXPECT_EQ(rows[0].UsedSize(), 10);
  EXPECT_EQ(rows[1].UsedSize(), 20);
  EXPECT_EQ(rows[0].InitLocations().count(circuit.GetComponentPtr("move")), 0);
  EXPECT_EQ(rows[1].InitLocations().count(circuit.GetComponentPtr("move")), 1);
  for (const GriddedRow& row : rows) {
    for (const Component* component : row.Components()) {
      EXPECT_GE(component->LLX(), row.LLX() + row.LeftBoundaryMargin());
      EXPECT_LE(component->URX(), row.URX() - row.RightBoundaryMargin());
    }
  }
}

TEST(GriddedDetailedPlacerTest, EjectionChainCreatesRowWhitespace) {
  Circuit circuit;
  circuit.SetDatabaseMicrons(1000);
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 20, 30);
  circuit.ReserveSpaceForDesignImp(10, 0, 5);

  Macro* cell = circuit.AddMacro("cell", 10, 10);
  ASSERT_NE(cell, nullptr);
  cell->AddWellRect(false, 0, 0, 10, 4);
  cell->AddWellRect(true, 0, 4, 10, 10);
  circuit.AddMacroPin(cell, "p", true)->SetOffset(5, 5);

  const std::vector<std::string> movable_names = {
      "move", "source_stay", "eject", "target_stay", "receiver_stay"};
  const std::vector<int> movable_x = {0, 10, 0, 10, 10};
  const std::vector<int> movable_y = {0, 0, 10, 10, 20};
  const std::vector<int> anchor_y = {10, 0, 20, 10, 20};
  for (size_t i = 0; i < movable_names.size(); ++i) {
    circuit.AddComponent(movable_names[i], "cell", movable_x[i], movable_y[i],
                         PLACED);
    circuit.AddComponent(movable_names[i] + "_anchor", "cell", movable_x[i],
                         anchor_y[i], FIXED);
  }
  for (const std::string& movable_name : movable_names) {
    const std::string net_name = movable_name + "_net";
    circuit.AddNet(net_name, 2);
    circuit.AddComponentPinToNet(movable_name, "p", net_name);
    circuit.AddComponentPinToNet(movable_name + "_anchor", "p", net_name);
  }

  std::vector<GriddedRow> rows(3);
  for (size_t i = 0; i < rows.size(); ++i) {
    rows[i].SetLLX(0);
    rows[i].SetWidth(20);
    rows[i].SetLLY(static_cast<int>(i) * 10);
    rows[i].UpdateWellHeightUpward(4, 6);
    rows[i].SetOrient(true);
  }
  rows[0].AddComponent(circuit.GetComponentPtr("move"));
  rows[0].AddComponent(circuit.GetComponentPtr("source_stay"));
  rows[1].AddComponent(circuit.GetComponentPtr("eject"));
  rows[1].AddComponent(circuit.GetComponentPtr("target_stay"));
  rows[2].AddComponent(circuit.GetComponentPtr("receiver_stay"));

  GriddedDetailedPlacer placer;
  placer.SetCircuit(&circuit);
  placer.SetRows({&rows[0], &rows[1], &rows[2]});
  placer.SetEnableRelocation(true);
  placer.SetEnableVerticalSwap(false);
  placer.SetMaxRounds(1);
  const double hpwl_before = circuit.WeightedHPWL();
  ASSERT_TRUE(placer.StartPlacement());

  EXPECT_LT(circuit.WeightedHPWL(), hpwl_before);
  EXPECT_EQ(rows[0].Components().size(), 1);
  EXPECT_EQ(rows[1].Components().size(), 2);
  EXPECT_EQ(rows[2].Components().size(), 2);
  EXPECT_NE(std::find(rows[1].Components().begin(), rows[1].Components().end(),
                      circuit.GetComponentPtr("move")),
            rows[1].Components().end());
  EXPECT_NE(std::find(rows[2].Components().begin(), rows[2].Components().end(),
                      circuit.GetComponentPtr("eject")),
            rows[2].Components().end());
}

TEST(GriddedDetailedPlacerTest, ClosedAssignmentCycleUsesFreedSourceSpace) {
  Circuit circuit;
  circuit.SetDatabaseMicrons(1000);
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 20, 30);
  circuit.ReserveSpaceForDesignImp(12, 0, 6);

  Macro* cell = circuit.AddMacro("cell", 10, 10);
  ASSERT_NE(cell, nullptr);
  cell->AddWellRect(false, 0, 0, 10, 4);
  cell->AddWellRect(true, 0, 4, 10, 10);
  circuit.AddMacroPin(cell, "p", true)->SetOffset(5, 5);

  const std::vector<std::string> moving_names = {"a", "b", "c"};
  const std::vector<int> initial_y = {0, 10, 20};
  const std::vector<int> desired_y = {10, 20, 0};
  for (size_t i = 0; i < moving_names.size(); ++i) {
    circuit.AddComponent(moving_names[i], "cell", 0, initial_y[i], PLACED);
    circuit.AddComponent(moving_names[i] + "_anchor", "cell", 0, desired_y[i],
                         FIXED);
  }
  for (int row = 0; row < 3; ++row) {
    circuit.AddComponent("stay_" + std::to_string(row), "cell", 10, row * 10,
                         PLACED);
    circuit.AddComponent("stay_anchor_" + std::to_string(row), "cell", 10,
                         row * 10, FIXED);
  }
  for (const std::string& moving_name : moving_names) {
    const std::string net_name = moving_name + "_net";
    circuit.AddNet(net_name, 2);
    circuit.AddComponentPinToNet(moving_name, "p", net_name);
    circuit.AddComponentPinToNet(moving_name + "_anchor", "p", net_name);
  }
  for (int row = 0; row < 3; ++row) {
    const std::string net_name = "stay_net_" + std::to_string(row);
    circuit.AddNet(net_name, 2);
    circuit.AddComponentPinToNet("stay_" + std::to_string(row), "p", net_name);
    circuit.AddComponentPinToNet("stay_anchor_" + std::to_string(row), "p",
                                 net_name);
  }

  std::vector<GriddedRow> rows(3);
  for (size_t i = 0; i < rows.size(); ++i) {
    rows[i].SetLLX(0);
    rows[i].SetWidth(20);
    rows[i].SetLLY(static_cast<int>(i) * 10);
    rows[i].UpdateWellHeightUpward(4, 6);
    rows[i].SetOrient(true);
    rows[i].AddComponent(circuit.GetComponentPtr(moving_names[i]));
    rows[i].AddComponent(circuit.GetComponentPtr("stay_" + std::to_string(i)));
  }

  GriddedDetailedPlacer placer;
  placer.SetCircuit(&circuit);
  placer.SetRows({&rows[0], &rows[1], &rows[2]});
  placer.SetEnableRelocation(true);
  placer.SetEnableVerticalSwap(false);
  placer.SetMaxRounds(1);
  const double hpwl_before = circuit.WeightedHPWL();
  ASSERT_TRUE(placer.StartPlacement());

  EXPECT_LT(circuit.WeightedHPWL(), hpwl_before);
  EXPECT_NE(std::find(rows[1].Components().begin(), rows[1].Components().end(),
                      circuit.GetComponentPtr("a")),
            rows[1].Components().end());
  EXPECT_NE(std::find(rows[2].Components().begin(), rows[2].Components().end(),
                      circuit.GetComponentPtr("b")),
            rows[2].Components().end());
  EXPECT_NE(std::find(rows[0].Components().begin(), rows[0].Components().end(),
                      circuit.GetComponentPtr("c")),
            rows[0].Components().end());
}

TEST(GriddedDetailedPlacerTest, SingleSegmentClusteringPreservesOrder) {
  Circuit circuit;
  circuit.SetDatabaseMicrons(1000);
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 60, 10);
  circuit.ReserveSpaceForDesignImp(6, 0, 3);

  Macro* cell = circuit.AddMacro("cell", 10, 10);
  ASSERT_NE(cell, nullptr);
  cell->AddWellRect(false, 0, 0, 10, 4);
  cell->AddWellRect(true, 0, 4, 10, 10);
  circuit.AddMacroPin(cell, "p", true)->SetOffset(5, 5);

  const std::vector<std::string> names = {"a", "b", "c"};
  const std::vector<int> initial_x = {0, 20, 40};
  const std::vector<int> anchor_x = {40, 20, 0};
  for (size_t i = 0; i < names.size(); ++i) {
    circuit.AddComponent(names[i], "cell", initial_x[i], 0, PLACED);
    circuit.AddComponent(names[i] + "_anchor", "cell", anchor_x[i], 0, FIXED);
  }
  for (const std::string& name : names) {
    const std::string net_name = name + "_net";
    circuit.AddNet(net_name, 2);
    circuit.AddComponentPinToNet(name, "p", net_name);
    circuit.AddComponentPinToNet(name + "_anchor", "p", net_name);
  }

  GriddedRow row;
  row.SetLLX(0);
  row.SetWidth(60);
  row.SetLLY(0);
  row.UpdateWellHeightUpward(4, 6);
  row.SetOrient(true);
  for (const std::string& name : names) {
    row.AddComponent(circuit.GetComponentPtr(name));
  }

  GriddedDetailedPlacer placer;
  placer.SetCircuit(&circuit);
  placer.SetRows({&row});
  placer.SetMaxRounds(0);
  ASSERT_TRUE(placer.StartPlacement());

  ASSERT_EQ(row.Components().size(), 3);
  EXPECT_EQ(row.Components()[0], circuit.GetComponentPtr("a"));
  EXPECT_EQ(row.Components()[1], circuit.GetComponentPtr("b"));
  EXPECT_EQ(row.Components()[2], circuit.GetComponentPtr("c"));
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("a")->LLX(), 10);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("b")->LLX(), 20);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("c")->LLX(), 30);
  EXPECT_DOUBLE_EQ(circuit.WeightedHPWL(), 60);
}

}  // namespace dali
