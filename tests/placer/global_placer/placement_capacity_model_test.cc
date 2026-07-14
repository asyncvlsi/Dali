/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/global_placer/placement_capacity_model.h"

#include <gtest/gtest.h>

#include <utility>

#include "dali/circuit/circuit.h"

namespace dali {

class PlacementCapacityModelTest : public testing::Test {
 protected:
  PlacementCapacityModelTest() {
    circuit_.SetManufacturingGrid(1);
    circuit_.SetUnitsDistanceMicrons(1);
    circuit_.SetGridValue(1, 1);
    circuit_.ReserveSpaceForDesignImp(3, 0, 0);
    circuit_.AddMacro("cell", 2, 5);
  }

  Component* AddComponent(const std::string& name,
                          PlaceStatus status = PLACED) {
    circuit_.AddComponent(name, "cell", 0, 0, status);
    return circuit_.GetComponentPtr(name);
  }

  Circuit circuit_;
};

TEST_F(PlacementCapacityModelTest, AppliesAreaWeightedDemandMultipliers) {
  Component* first = AddComponent("first");
  Component* second = AddComponent("second");
  std::vector<Component*> components = {first, second};
  std::vector<double> multipliers(2, 1.0);
  multipliers[first->Id()] = 1.5;

  LegalizationPressureCapacityModel model(
      std::make_shared<AreaCapacityModel>());
  model.SetDemandMultipliers(std::move(multipliers));
  PlacementCapacity capacity =
      model.Evaluate(components, 10, 10, 100, 1.0,
                     CapacityEvaluationPurpose::kSpreadingRegion);

  EXPECT_DOUBLE_EQ(capacity.demand, 25.0);
  EXPECT_DOUBLE_EQ(capacity.capacity, 100.0);
}

TEST_F(PlacementCapacityModelTest, IgnoresFixedComponentPressure) {
  Component* movable = AddComponent("movable");
  Component* fixed = AddComponent("fixed", FIXED);
  std::vector<Component*> components = {movable, fixed};
  std::vector<double> multipliers(2, 1.0);
  multipliers[fixed->Id()] = 4.0;

  LegalizationPressureCapacityModel model(
      std::make_shared<AreaCapacityModel>());
  model.SetDemandMultipliers(std::move(multipliers));
  PlacementCapacity capacity =
      model.Evaluate(components, 10, 10, 100, 1.0,
                     CapacityEvaluationPurpose::kSpreadingRegion);

  EXPECT_DOUBLE_EQ(capacity.demand, 10.0);
}

TEST_F(PlacementCapacityModelTest, LeavesUnknownComponentsUnscaled) {
  Component* component = AddComponent("component");
  LegalizationPressureCapacityModel model(
      std::make_shared<AreaCapacityModel>());

  PlacementCapacity capacity =
      model.Evaluate({component}, 10, 10, 100, 1.0,
                     CapacityEvaluationPurpose::kSpreadingRegion);

  EXPECT_DOUBLE_EQ(capacity.demand, 10.0);
}

}  // namespace dali
