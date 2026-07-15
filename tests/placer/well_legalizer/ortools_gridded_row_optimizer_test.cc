/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/ortools_gridded_row_optimizer.h"

#include <gtest/gtest.h>

namespace dali {

TEST(OrToolsGriddedRowOptimizerTest,
     ImprovesHpwlWithoutChangingRowOrderOrLegality) {
  if (!OrToolsFixedRowDisplacementOptimizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(3, 0, 2);
  Macro* macro = circuit.AddMacro("cell", 2, 2);
  macro->AddWellRect(false, 0, 0, 2, 1);
  macro->AddWellRect(true, 0, 1, 2, 2);
  circuit.AddMacroPin(macro, "pin", true)->SetOffset(1, 1);
  circuit.AddComponent("left", "cell", 0, 0, PLACED);
  circuit.AddComponent("right", "cell", 2, 0, PLACED);
  circuit.AddComponent("anchor", "cell", 18, 0, FIXED);
  circuit.AddNet("left_net", 2);
  circuit.AddComponentPinToNet("left", "pin", "left_net");
  circuit.AddComponentPinToNet("anchor", "pin", "left_net");
  circuit.AddNet("right_net", 2);
  circuit.AddComponentPinToNet("right", "pin", "right_net");
  circuit.AddComponentPinToNet("anchor", "pin", "right_net");

  std::vector<StripeColumn> columns(1);
  columns[0].stripe_list_.resize(1);
  Stripe& stripe = columns[0].stripe_list_[0];
  stripe.lx_ = 0;
  stripe.width_ = 20;
  stripe.gridded_rows_.resize(1);
  GriddedRow& row = stripe.gridded_rows_[0];
  row.SetLLX(0);
  row.SetWidth(20);
  row.AddComponent(circuit.GetComponentPtr("left"));
  row.AddComponent(circuit.GetComponentPtr("right"));

  const double hpwl_before = circuit.WeightedHPWL();
  OrToolsGriddedRowOptimizerConfig config;
  config.maximum_time_seconds_per_model = 10.0;
  OrToolsGriddedRowOptimizerResult result =
      OrToolsGriddedRowOptimizer(&circuit, config).Optimize(&columns);

  EXPECT_TRUE(result.available);
  EXPECT_EQ(result.attempted_models, 1);
  EXPECT_EQ(result.solved_models, 1);
  EXPECT_EQ(result.accepted_models, 1);
  EXPECT_EQ(result.improved_models, 1);
  EXPECT_LT(result.hpwl_after, hpwl_before);
  EXPECT_TRUE(row.HasLegalComponentPlacement());
  EXPECT_LE(circuit.GetComponentPtr("left")->URX(),
            circuit.GetComponentPtr("right")->LLX());
}

}  // namespace dali
