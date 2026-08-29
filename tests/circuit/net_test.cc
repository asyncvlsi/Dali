#include <gtest/gtest.h>

#include "dali/circuit/circuit.h"

namespace dali {

static Circuit MakeUnitGridCircuit() {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(4, 0, 0);
  return circuit;
}

TEST(NetTest, UpdatesCenterToCenterExtremesByComponentCenter) {
  Circuit circuit = MakeUnitGridCircuit();
  circuit.AddMacro("left_cell", 10, 10);
  circuit.AddMacro("right_cell", 10, 10);
  Macro* left_macro = circuit.GetMacroPtr("left_cell");
  Macro* right_macro = circuit.GetMacroPtr("right_cell");
  circuit.AddMacroPin(left_macro, "p", true)->SetOffset(10, 0);
  circuit.AddMacroPin(right_macro, "p", true)->SetOffset(-10, 0);

  circuit.AddComponent("u_left", "left_cell", 0, 0);
  circuit.AddComponent("u_right", "right_cell", 20, 0);
  Net* net = circuit.AddNet("n0", 2);
  circuit.AddComponentPinToNet("u_left", "p", "n0");
  circuit.AddComponentPinToNet("u_right", "p", "n0");

  net->UpdateMaxMinCtoC();

  EXPECT_EQ(net->MinComponentPtrX()->Name(), "u_left");
  EXPECT_EQ(net->MaxComponentPtrX()->Name(), "u_right");
}

TEST(NetTest, ReportsPhysicalHpwlOnAnisotropicGrid) {
  Circuit circuit;
  circuit.SetManufacturingGrid(0.1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(0.6, 0.3);
  circuit.ReserveSpaceForDesignImp(2, 0, 1);
  circuit.AddMacro("cell", 1, 1);
  Macro* macro = circuit.GetMacroPtr("cell");
  circuit.AddMacroPin(macro, "p", true)->SetOffset(0, 0);
  circuit.AddComponent("u0", "cell", 0, 0);
  circuit.AddComponent("u1", "cell", 10, 20);
  Net* net = circuit.AddNet("n0", 2);
  circuit.AddComponentPinToNet("u0", "p", "n0");
  circuit.AddComponentPinToNet("u1", "p", "n0");

  EXPECT_DOUBLE_EQ(circuit.NetWeightedHPWL(net->Id()), 12.0);
  net->SetWeight(2.0);
  EXPECT_DOUBLE_EQ(circuit.NetWeightedHPWL(net->Id()), 24.0);
  EXPECT_DOUBLE_EQ(circuit.UnweightedHPWL(), 12.0);
}

TEST(NetTest, RefreshesQuadraticWeightWhenNetWeightChanges) {
  Circuit circuit = MakeUnitGridCircuit();
  circuit.AddMacro("cell", 1, 1);
  Macro* macro = circuit.GetMacroPtr("cell");
  circuit.AddMacroPin(macro, "p", true)->SetOffset(0, 0);
  for (int index = 0; index < 3; ++index) {
    circuit.AddComponent("u" + std::to_string(index), "cell", index, 0);
  }
  Net* net = circuit.AddNet("n0", 3, 1.0);
  for (int index = 0; index < 3; ++index) {
    circuit.AddComponentPinToNet("u" + std::to_string(index), "p", "n0");
  }

  EXPECT_DOUBLE_EQ(net->InvP(), 0.5);
  net->SetWeight(4.0);
  EXPECT_DOUBLE_EQ(net->Weight(), 4.0);
  EXPECT_DOUBLE_EQ(net->InvP(), 2.0);
}

}  // namespace dali
