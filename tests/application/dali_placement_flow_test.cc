/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/
// clang-format off
#include <stdio.h>
#include <common/config.h>
// clang-format on

#include <gtest/gtest.h>

#include "dali/dali.h"

using testing::Test;

class DaliPlacementFlowTest : public Test {
 protected:
  void SetUp() override { config_clear(); }
  void TearDown() override { config_clear(); }

  static void ConfigureCircuitGrid(dali::Circuit* circuit) {
    circuit->SetManufacturingGrid(1);
    circuit->SetUnitsDistanceMicrons(1);
    circuit->SetGridValue(1, 1);
    circuit->SetDieArea(0, 0, 100, 100);
    circuit->ReserveSpaceForDesignImp(4, 0, 0);
  }

  static void AddMovableComponent(dali::Circuit* circuit,
                                  const std::string& component_name) {
    if (!circuit->IsMacroExisting("cell")) {
      dali::Macro* cell = circuit->AddMacro("cell", 10, 10);
      circuit->AddMacroPin(cell, "p", true);
    }
    circuit->AddComponent(component_name, "cell");
  }

  static void AddFixedComponent(dali::Circuit* circuit,
                                const std::string& component_name) {
    if (!circuit->IsMacroExisting("fixed_cell")) {
      circuit->AddMacro("fixed_cell", 10, 10);
    }
    circuit->AddComponent(component_name, "fixed_cell", 0, 0, dali::FIXED);
  }
};

TEST_F(DaliPlacementFlowTest, SkipsGlobalPlacementWhenDisabled) {
  config_set_int("dali.disable_global_place", 1);

  dali::Dali placer(nullptr, dali::severity::info);

  EXPECT_FALSE(placer.ShouldRunGlobalPlacement());
  placer.Close();
}

TEST_F(DaliPlacementFlowTest, SkipsGlobalPlacementWithoutMovableComponents) {
  dali::Dali placer(nullptr, dali::severity::info);
  ConfigureCircuitGrid(&placer.GetCircuit());

  EXPECT_FALSE(placer.ShouldRunGlobalPlacement());
  placer.Close();
}

TEST_F(DaliPlacementFlowTest, SkipsGlobalPlacementWithoutNets) {
  dali::Dali placer(nullptr, dali::severity::info);
  dali::Circuit& circuit = placer.GetCircuit();
  ConfigureCircuitGrid(&circuit);
  AddMovableComponent(&circuit, "u0");

  EXPECT_FALSE(placer.ShouldRunGlobalPlacement());
  placer.Close();
}

TEST_F(DaliPlacementFlowTest, RunsGlobalPlacementForMovableNetlist) {
  dali::Dali placer(nullptr, dali::severity::info);
  dali::Circuit& circuit = placer.GetCircuit();
  ConfigureCircuitGrid(&circuit);
  AddMovableComponent(&circuit, "u0");
  AddMovableComponent(&circuit, "u1");
  circuit.AddNet("n0", 2);
  circuit.AddComponentPinToNet("u0", "p", "n0");
  circuit.AddComponentPinToNet("u1", "p", "n0");

  EXPECT_TRUE(placer.ShouldRunGlobalPlacement());
  placer.Close();
}

TEST_F(DaliPlacementFlowTest, SkipsMovableCellLegalizationForFixedOnlyDesign) {
  dali::Dali placer(nullptr, dali::severity::info);
  dali::Circuit& circuit = placer.GetCircuit();
  ConfigureCircuitGrid(&circuit);
  AddFixedComponent(&circuit, "fixed0");

  EXPECT_FALSE(placer.ShouldRunMovableCellLegalization());
  placer.Close();
}

TEST_F(DaliPlacementFlowTest, RunsMovableCellLegalizationForMovableDesign) {
  dali::Dali placer(nullptr, dali::severity::info);
  dali::Circuit& circuit = placer.GetCircuit();
  ConfigureCircuitGrid(&circuit);
  AddMovableComponent(&circuit, "u0");

  EXPECT_TRUE(placer.ShouldRunMovableCellLegalization());
  placer.Close();
}
