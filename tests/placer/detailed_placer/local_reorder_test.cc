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

#include <algorithm>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/placer/detailed_placer/detailed_placer.h"

namespace dali {
namespace {

class DetailedPlacerLocalReorderTest : public testing::Test {
 protected:
  void SetUp() override {
    circuit_.SetDatabaseMicrons(1000);
    circuit_.SetManufacturingGrid(1);
    circuit_.SetUnitsDistanceMicrons(1);
    circuit_.SetGridValue(1, 1);
    circuit_.SetDieArea(0, 0, 200, 20);
    circuit_.ReserveSpaceForDesignImp(6, 2, 6);

    Macro* cell = circuit_.AddMacro("cell", 10, 10);
    ASSERT_NE(cell, nullptr);
    circuit_.AddMacroPin(cell, "p", true)->SetOffset(5, 5);

    circuit_.AddComponent("a", "cell", 0, 0, PLACED);
    circuit_.AddComponent("b", "cell", 10, 0, PLACED);
    circuit_.AddComponent("c", "cell", 20, 0, PLACED);
    circuit_.AddComponent("left_anchor", "cell", -100, 0, FIXED);
    circuit_.AddComponent("right_anchor", "cell", 100, 0, FIXED);

    circuit_.AddNet("pull_b_left", 2);
    circuit_.AddComponentPinToNet("b", "p", "pull_b_left");
    circuit_.AddComponentPinToNet("left_anchor", "p", "pull_b_left");
    circuit_.AddNet("pull_a_right", 2);
    circuit_.AddComponentPinToNet("a", "p", "pull_a_right");
    circuit_.AddComponentPinToNet("right_anchor", "p", "pull_a_right");

    GeneralRow row;
    row.SetLY(0);
    row.SetHeight(10);
    GeneralRowSegment segment;
    segment.SetLX(0);
    segment.SetWidth(30);
    segment.AddComponent(circuit_.GetComponentPtr("a"));
    segment.AddComponent(circuit_.GetComponentPtr("b"));
    segment.AddComponent(circuit_.GetComponentPtr("c"));
    row.RowSegments().push_back(segment);
    circuit_.design().Rows().push_back(row);
  }

  Circuit circuit_;
};

TEST_F(DetailedPlacerLocalReorderTest, ReordersThreeCellWindowWhenHpwlImproves) {
  double hpwl_before = circuit_.WeightedHPWL();

  DetailedPlacer placer;
  placer.SetCircuit(&circuit_);
  EXPECT_TRUE(placer.StartPlacement());

  EXPECT_LT(circuit_.WeightedHPWL(), hpwl_before);
  std::vector<double> locations = {
      circuit_.GetComponentPtr("a")->LLX(),
      circuit_.GetComponentPtr("b")->LLX(),
      circuit_.GetComponentPtr("c")->LLX(),
  };
  std::sort(locations.begin(), locations.end());
  EXPECT_EQ(locations, std::vector<double>({0, 10, 20}));
}

}  // namespace
}  // namespace dali
