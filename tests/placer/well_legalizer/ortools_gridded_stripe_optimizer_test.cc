/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/ortools_gridded_stripe_optimizer.h"

#include <gtest/gtest.h>

#include "dali/placer/well_legalizer/ortools_compact_gridded_legalizer.h"

namespace dali {

TEST(OrToolsGriddedStripeOptimizerTest,
     ReordersOneStripeAgainstExternalAnchors) {
  if (!OrToolsCompactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(4, 0, 2);
  Macro* macro = circuit.AddMacro("cell", 2, 2);
  macro->AddWellRect(false, 0, 0, 2, 1);
  macro->AddWellRect(true, 0, 1, 2, 2);
  circuit.AddMacroPin(macro, "pin", true)->SetOffset(1, 1);
  circuit.AddComponent("move_right", "cell", 0, 0, PLACED);
  circuit.AddComponent("move_left", "cell", 2, 0, PLACED);
  circuit.AddComponent("left_anchor", "cell", -1, 0, FIXED);
  circuit.AddComponent("right_anchor", "cell", 9, 0, FIXED);
  circuit.AddNet("right_net", 2);
  circuit.AddComponentPinToNet("move_right", "pin", "right_net");
  circuit.AddComponentPinToNet("right_anchor", "pin", "right_net");
  circuit.AddNet("left_net", 2);
  circuit.AddComponentPinToNet("move_left", "pin", "left_net");
  circuit.AddComponentPinToNet("left_anchor", "pin", "left_net");

  std::vector<StripeColumn> columns(1);
  Stripe& stripe = columns[0].stripe_list_.emplace_back();
  stripe.lx_ = 0;
  stripe.ly_ = 0;
  stripe.width_ = 4;
  stripe.height_ = 2;
  GriddedRow& row = stripe.gridded_rows_.emplace_back();
  row.SetLLX(0);
  row.SetLLY(0);
  row.SetWidth(4);
  row.UpdateWellHeightUpward(1, 1);
  row.AddComponent(circuit.GetComponentPtr("move_right"));
  row.AddComponent(circuit.GetComponentPtr("move_left"));

  OrToolsGriddedStripeOptimizerConfig config;
  config.maximum_time_seconds_per_stripe = 10.0;
  config.maximum_total_time_seconds = 20.0;
  config.maximum_sweeps = 1;
  config.minimum_p_well_height = 1;
  config.minimum_n_well_height = 1;
  OrToolsGriddedStripeOptimizerResult result =
      OrToolsGriddedStripeOptimizer(&circuit, config).Optimize(&columns);

  EXPECT_TRUE(result.available);
  EXPECT_EQ(result.attempted_stripes, 1);
  EXPECT_EQ(result.solved_stripes, 1);
  EXPECT_EQ(result.accepted_stripes, 1);
  EXPECT_LT(result.hpwl_after, result.hpwl_before);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("move_left")->LLX(), 0.0);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("move_right")->LLX(), 2.0);
  EXPECT_TRUE(row.HasLegalComponentPlacement());
}

}  // namespace dali
