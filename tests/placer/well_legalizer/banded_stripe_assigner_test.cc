/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/banded_stripe_assigner.h"

#include <gtest/gtest.h>

#include <string>

namespace dali {

TEST(BandedStripeAssignerTest, BalancesCapacityIndependentlyInEachYBand) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(8, 0, 0);
  Macro* macro = circuit.AddMacro("cell", 1, 2);
  macro->AddWellRect(false, 0, 0, 1, 1);
  macro->AddWellRect(true, 0, 1, 1, 2);

  for (int index = 0; index < 4; ++index) {
    circuit.AddComponent("lower_" + std::to_string(index), "cell", index, 10,
                         PLACED);
    circuit.AddComponent("upper_" + std::to_string(index), "cell", 10 + index,
                         60, PLACED);
  }

  std::vector<StripeColumn> columns(2);
  for (int column_index = 0; column_index < 2; ++column_index) {
    StripeColumn& column = columns[column_index];
    column.lx_ = column_index * 10;
    column.width_ = 10;
    Stripe& stripe = column.stripe_list_.emplace_back();
    stripe.lx_ = column.lx_;
    stripe.ly_ = 0;
    stripe.width_ = 10;
    stripe.height_ = 100;
  }
  for (int index = 0; index < 4; ++index) {
    columns[0].component_list_.push_back(
        circuit.GetComponentPtr("lower_" + std::to_string(index)));
    columns[1].component_list_.push_back(
        circuit.GetComponentPtr("upper_" + std::to_string(index)));
  }

  BandedStripeAssignmentConfig config;
  config.band_count = 2;
  config.require_projected_hpwl_improvement = false;
  const BandedStripeAssignmentResult result =
      BandedStripeAssigner(&circuit, config).Assign(&columns);

  EXPECT_EQ(result.populated_band_count, 2);
  EXPECT_EQ(result.assigned_component_count, 8);
  EXPECT_EQ(result.moved_component_count, 4);
  EXPECT_DOUBLE_EQ(result.average_column_displacement, 0.5);
  EXPECT_EQ(result.maximum_column_displacement, 1);
  ASSERT_EQ(columns[0].component_list_.size(), 4U);
  ASSERT_EQ(columns[1].component_list_.size(), 4U);

  int lower_in_left = 0;
  int upper_in_left = 0;
  for (const Component* component : columns[0].component_list_) {
    if (component->Name().find("lower_") == 0) ++lower_in_left;
    if (component->Name().find("upper_") == 0) ++upper_in_left;
  }
  EXPECT_EQ(lower_in_left, 2);
  EXPECT_EQ(upper_in_left, 2);
}

}  // namespace dali
