/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/gridded_stripe_balancer.h"

#include <gtest/gtest.h>

namespace dali {

TEST(GriddedStripeBalancerTest, RemovesOverflowWithoutMovingComponents) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(4, 0, 0);
  Macro* macro = circuit.AddMacro("cell", 4, 10);
  macro->AddWellRect(false, 0, 0, 4, 4);
  macro->AddWellRect(true, 0, 4, 4, 10);

  std::vector<Component*> components;
  for (int id = 0; id < 3; ++id) {
    std::string name = "cell_" + std::to_string(id);
    circuit.AddComponent(name, "cell", id, id, PLACED);
    components.push_back(circuit.GetComponentPtr(name));
  }
  std::vector<std::pair<double, double>> original_locations;
  for (const Component* component : components) {
    original_locations.push_back({component->LLX(), component->LLY()});
  }

  std::vector<StripeColumn> columns(2);
  columns[0].lx_ = 0;
  columns[0].width_ = 10;
  columns[0].stripe_list_.resize(1);
  columns[0].stripe_list_[0].lx_ = 0;
  columns[0].stripe_list_[0].ly_ = 0;
  columns[0].stripe_list_[0].width_ = 10;
  columns[0].stripe_list_[0].height_ = 10;
  columns[0].stripe_list_[0].component_ptrs_vec_ = components;
  columns[1].lx_ = 10;
  columns[1].width_ = 10;
  columns[1].stripe_list_.resize(1);
  columns[1].stripe_list_[0].lx_ = 10;
  columns[1].stripe_list_[0].ly_ = 0;
  columns[1].stripe_list_[0].width_ = 10;
  columns[1].stripe_list_[0].height_ = 10;

  GriddedCapacityConfig config;
  config.reserved_width = 2;
  GriddedStripeBalanceResult result =
      GriddedStripeBalancer(&circuit, config).Balance(&columns);

  EXPECT_EQ(result.overflowing_stripes_before, 1);
  EXPECT_EQ(result.overflowing_stripes_after, 0);
  EXPECT_GT(result.moved_component_count, 0);
  for (size_t id = 0; id < components.size(); ++id) {
    EXPECT_DOUBLE_EQ(components[id]->LLX(), original_locations[id].first);
    EXPECT_DOUBLE_EQ(components[id]->LLY(), original_locations[id].second);
  }

  columns[0].stripe_list_[0].component_ptrs_vec_ = components;
  columns[0].stripe_list_[0].used_height_ = 20;
  columns[1].stripe_list_[0].component_ptrs_vec_.clear();
  columns[1].stripe_list_[0].used_height_ = 0;
  result = GriddedStripeBalancer(&circuit, config)
               .BalanceObservedOverflow(&columns);

  EXPECT_EQ(result.overflowing_stripes_before, 1);
  EXPECT_GT(result.moved_component_count, 0);
  for (size_t id = 0; id < components.size(); ++id) {
    EXPECT_DOUBLE_EQ(components[id]->LLX(), original_locations[id].first);
    EXPECT_DOUBLE_EQ(components[id]->LLY(), original_locations[id].second);
  }
}

}  // namespace dali
