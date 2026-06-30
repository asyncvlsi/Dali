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
#include <gtest/gtest.h>

#include "dali/circuit/circuit.h"

namespace dali {
namespace {

TEST(CircuitProgrammaticConstructionTest, BuildsSmallCircuitThroughPublicApi) {
  Circuit circuit;
  circuit.SetDatabaseMicrons(1000);
  circuit.SetManufacturingGrid(0.001);
  circuit.AddMetalLayer("m1", 0.1, 0.1, 0.042, 0.2, 0.2, VERTICAL);
  circuit.AddMetalLayer("m2", 0.1, 0.1, 0.042, 0.2, 0.2, HORIZONTAL);
  circuit.SetGridValue(0.2, 0.2);
  circuit.SetRowHeight(0.2);

  Macro* inv = circuit.AddMacro("INV2", 0.8, 1.6);
  ASSERT_NE(inv, nullptr);
  circuit.AddMacroPin(inv, "IN", true)->SetOffset(0.1, 0.8);
  circuit.AddMacroPin(inv, "OUT", false)->SetOffset(0.7, 0.8);

  circuit.SetUnitsDistanceMicrons(1000);
  circuit.SetDieArea(0, 0, 10000, 10000);
  circuit.ReserveSpaceForDesignImp(2, 2, 3);
  circuit.AddComponent("inv1", "INV2", 0, 0, UNPLACED, N, true);
  circuit.AddComponent("inv2", "INV2", 0, 0, UNPLACED, N, true);
  circuit.AddIoPin("io_in", UNPLACED, SIGNAL, INPUT, 0, 0);
  circuit.AddIoPin("io_out", UNPLACED, SIGNAL, OUTPUT, 0, 0);

  circuit.AddNet("net_in", 2);
  circuit.AddIoPinToNet("io_in", "net_in");
  circuit.AddComponentPinToNet("inv1", "IN", "net_in");
  circuit.AddNet("net_between", 2);
  circuit.AddComponentPinToNet("inv1", "OUT", "net_between");
  circuit.AddComponentPinToNet("inv2", "IN", "net_between");
  circuit.AddNet("net_out", 2);
  circuit.AddComponentPinToNet("inv2", "OUT", "net_out");
  circuit.AddIoPinToNet("io_out", "net_out");

  EXPECT_EQ(circuit.DatabaseMicrons(), 1000);
  EXPECT_DOUBLE_EQ(circuit.ManufacturingGrid(), 0.001);
  EXPECT_EQ(circuit.Metals().size(), 2);
  EXPECT_EQ(circuit.Macros().size(), 2);
  EXPECT_TRUE(circuit.IsMacroExisting("INV2"));
  EXPECT_EQ(circuit.Components().size(), 2);
  EXPECT_EQ(circuit.IoPins().size(), 2);
  EXPECT_EQ(circuit.Nets().size(), 3);
  EXPECT_TRUE(circuit.IsComponentExisting("inv1"));
  EXPECT_TRUE(circuit.IsIoPinExisting("io_out"));
  EXPECT_TRUE(circuit.IsNetExisting("net_between"));

  Macro* stored_inv = circuit.GetMacroPtr("INV2");
  ASSERT_NE(stored_inv, nullptr);
  ASSERT_NE(stored_inv->GetPinPtr("IN"), nullptr);
  ASSERT_NE(stored_inv->GetPinPtr("OUT"), nullptr);
  EXPECT_TRUE(stored_inv->GetPinPtr("IN")->IsInput());
  EXPECT_FALSE(stored_inv->GetPinPtr("OUT")->IsInput());
  EXPECT_DOUBLE_EQ(stored_inv->GetPinPtr("IN")->OffsetX(), 0.1);
  EXPECT_DOUBLE_EQ(stored_inv->GetPinPtr("IN")->OffsetY(), 0.8);
  EXPECT_DOUBLE_EQ(stored_inv->GetPinPtr("OUT")->OffsetX(), 0.7);
  EXPECT_DOUBLE_EQ(stored_inv->GetPinPtr("OUT")->OffsetY(), 0.8);

  Net* net_between = circuit.GetNetPtr("net_between");
  ASSERT_NE(net_between, nullptr);
  ASSERT_EQ(net_between->ComponentPins().size(), 2);
  EXPECT_EQ(net_between->ComponentPins()[0].ComponentName(), "inv1");
  EXPECT_EQ(net_between->ComponentPins()[0].PinName(), "OUT");
  EXPECT_EQ(net_between->ComponentPins()[1].ComponentName(), "inv2");
  EXPECT_EQ(net_between->ComponentPins()[1].PinName(), "IN");
  EXPECT_TRUE(net_between->IoPinPtrs().empty());

  Net* net_in = circuit.GetNetPtr("net_in");
  ASSERT_NE(net_in, nullptr);
  ASSERT_EQ(net_in->ComponentPins().size(), 1);
  ASSERT_EQ(net_in->IoPinPtrs().size(), 1);
  EXPECT_EQ(net_in->IoPinPtrs()[0]->Name(), "io_in");
}

}  // namespace
}  // namespace dali
