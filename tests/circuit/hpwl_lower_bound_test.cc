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
#include "dali/circuit/hpwl_lower_bound.h"

#include <gtest/gtest.h>

#include "dali/circuit/circuit.h"

namespace dali {

TEST(HpwlLowerBoundTest, AccountsForMovablePinReachInsidePlacementBox) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.SetDieArea(0, 0, 100, 100);
  circuit.ReserveSpaceForDesignImp(2, 0, 1);
  circuit.AddMacro("cell", 10, 10);
  Macro* macro = circuit.GetMacroPtr("cell");
  circuit.AddMacroPin(macro, "p", true)->SetOffset(5, 5);
  circuit.AddComponent("fixed", "cell", -20, 20, FIXED);
  circuit.AddComponent("movable", "cell", 0, 0, UNPLACED);
  circuit.AddNet("net", 2);
  circuit.AddComponentPinToNet("fixed", "p", "net");
  circuit.AddComponentPinToNet("movable", "p", "net");

  HpwlLowerBound fixed_terminal = ComputeFixedTerminalHpwlLowerBound(circuit);
  HpwlLowerBound placement_box = ComputePlacementBoxHpwlLowerBound(circuit);

  EXPECT_DOUBLE_EQ(fixed_terminal.Total(), 0.0);
  EXPECT_DOUBLE_EQ(placement_box.x, 20.0);
  EXPECT_DOUBLE_EQ(placement_box.y, 0.0);
}

TEST(HpwlLowerBoundTest, IncludesUnavoidableSpanBetweenFixedPins) {
  Circuit circuit;
  circuit.SetManufacturingGrid(0.25);
  circuit.SetUnitsDistanceMicrons(4);
  circuit.SetGridValue(0.5, 0.25);
  circuit.SetDieArea(0, 0, 100, 100);
  circuit.ReserveSpaceForDesignImp(2, 0, 1);
  circuit.AddMacro("cell", 2, 2);
  Macro* macro = circuit.GetMacroPtr("cell");
  circuit.AddMacroPin(macro, "p", true)->SetOffset(0, 0);
  circuit.AddComponent("fixed0", "cell", 0, 0, FIXED);
  circuit.AddComponent("fixed1", "cell", 20, 40, FIXED);
  circuit.AddNet("net", 2, 2.0);
  circuit.AddComponentPinToNet("fixed0", "p", "net");
  circuit.AddComponentPinToNet("fixed1", "p", "net");

  HpwlLowerBound lower_bound = ComputeFixedTerminalHpwlLowerBound(circuit);

  EXPECT_DOUBLE_EQ(lower_bound.x, 20.0);
  EXPECT_DOUBLE_EQ(lower_bound.y, 20.0);
  EXPECT_DOUBLE_EQ(lower_bound.Total(), 40.0);
}

}  // namespace dali
