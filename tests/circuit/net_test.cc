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
}

}  // namespace dali
