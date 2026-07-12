/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/gridded_row_location_optimizer.h"

#include <gtest/gtest.h>

namespace dali {

TEST(GriddedRowLocationOptimizerTest, MovesLegalRowTowardExternalPin) {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(2, 0, 1);
  Macro* macro = circuit.AddMacro("cell", 4, 10);
  macro->AddWellRect(false, 0, 0, 4, 4);
  macro->AddWellRect(true, 0, 4, 4, 10);
  circuit.AddMacroPin(macro, "p", true)->SetOffset(2, 2);
  circuit.AddComponent("movable", "cell", 0, 0, PLACED);
  circuit.AddComponent("anchor", "cell", 0, 80, FIXED);
  circuit.AddNet("pull_up", 2);
  circuit.AddComponentPinToNet("movable", "p", "pull_up");
  circuit.AddComponentPinToNet("anchor", "p", "pull_up");

  std::vector<StripeColumn> columns(1);
  columns[0].stripe_list_.resize(1);
  Stripe& stripe = columns[0].stripe_list_[0];
  stripe.lx_ = 0;
  stripe.ly_ = 0;
  stripe.width_ = 20;
  stripe.height_ = 100;
  stripe.gridded_rows_.resize(1);
  GriddedRow& row = stripe.gridded_rows_[0];
  row.SetLLX(0);
  row.SetWidth(20);
  row.SetLLY(0);
  row.UpdateWellHeightUpward(4, 6);
  row.SetOrient(true);
  row.AddComponent(circuit.GetComponentPtr("movable"));
  row.UpdateComponentLocY();

  double hpwl_before = circuit.WeightedHPWL();
  GriddedRowLocationResult result =
      GriddedRowLocationOptimizer(&circuit).Optimize(&columns);

  EXPECT_EQ(row.LLY(), 80);
  EXPECT_EQ(result.groups_moved, 1);
  EXPECT_LT(result.hpwl_after, hpwl_before);
  EXPECT_DOUBLE_EQ(circuit.WeightedHPWL(), 0.0);
  EXPECT_TRUE(stripe.HasNoRowsSpillingOut());
}

}  // namespace dali
