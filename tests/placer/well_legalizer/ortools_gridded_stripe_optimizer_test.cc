/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/ortools_gridded_stripe_optimizer.h"

#include <gtest/gtest.h>

#include <string>

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
  EXPECT_EQ(result.attempted_models, 1);
  EXPECT_EQ(result.solved_models, 1);
  EXPECT_EQ(result.accepted_models, 1);
  EXPECT_LT(result.hpwl_after, result.hpwl_before);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("move_left")->LLX(), 0.0);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("move_right")->LLX(), 2.0);
  EXPECT_TRUE(row.HasLegalComponentPlacement());
}

TEST(OrToolsGriddedStripeOptimizerTest,
     BuildsOverlappingBandsWithoutRepeatingTheStripeTail) {
  if (!OrToolsCompactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(4, 0, 0);
  Macro* macro = circuit.AddMacro("cell", 2, 2);
  macro->AddWellRect(false, 0, 0, 2, 1);
  macro->AddWellRect(true, 0, 1, 2, 2);
  for (int row_index = 0; row_index < 4; ++row_index) {
    circuit.AddComponent("cell_" + std::to_string(row_index), "cell", 0,
                         2 * row_index, PLACED, row_index % 2 == 0 ? N : FS);
  }

  std::vector<StripeColumn> columns(1);
  Stripe& stripe = columns[0].stripe_list_.emplace_back();
  stripe.lx_ = 0;
  stripe.ly_ = 0;
  stripe.width_ = 2;
  stripe.height_ = 8;
  for (int row_index = 0; row_index < 4; ++row_index) {
    GriddedRow& row = stripe.gridded_rows_.emplace_back();
    row.SetLLX(0);
    row.SetLLY(2 * row_index);
    row.SetWidth(2);
    row.UpdateWellHeightUpward(1, 1);
    row.AddComponent(
        circuit.GetComponentPtr("cell_" + std::to_string(row_index)));
  }

  OrToolsGriddedStripeOptimizerConfig config;
  config.maximum_time_seconds_per_stripe = 10.0;
  config.maximum_total_time_seconds = 40.0;
  config.maximum_sweeps = 1;
  config.minimum_p_well_height = 1;
  config.minimum_n_well_height = 1;
  config.target_components_per_model = 2;
  config.maximum_components_per_model = 4;
  const OrToolsGriddedStripeOptimizerResult result =
      OrToolsGriddedStripeOptimizer(&circuit, config).Optimize(&columns);

  ASSERT_EQ(result.stripes.size(), 3U);
  EXPECT_EQ(result.attempted_models, 3);
  EXPECT_EQ(result.stripes[0].first_row_index, 0);
  EXPECT_EQ(result.stripes[0].last_row_index, 1);
  EXPECT_EQ(result.stripes[1].first_row_index, 1);
  EXPECT_EQ(result.stripes[1].last_row_index, 2);
  EXPECT_EQ(result.stripes[2].first_row_index, 2);
  EXPECT_EQ(result.stripes[2].last_row_index, 3);
}

TEST(OrToolsGriddedStripeOptimizerTest,
     PrioritizesReducibleHeadroomInsteadOfRawAffectedNetHpwl) {
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
  circuit.AddComponent("low", "cell", 0, 0, PLACED);
  circuit.AddComponent("high", "cell", 20, 2, PLACED);
  circuit.AddComponent("near_anchor", "cell", 9, 0, FIXED);
  circuit.AddComponent("far_anchor", "cell", 50, 2, FIXED);
  circuit.AddNet("low_net", 2);
  circuit.AddComponentPinToNet("low", "pin", "low_net");
  circuit.AddComponentPinToNet("near_anchor", "pin", "low_net");
  circuit.AddNet("high_net", 2);
  circuit.AddComponentPinToNet("high", "pin", "high_net");
  circuit.AddComponentPinToNet("far_anchor", "pin", "high_net");

  std::vector<StripeColumn> columns(2);
  for (int column_index = 0; column_index < 2; ++column_index) {
    Stripe& stripe = columns[column_index].stripe_list_.emplace_back();
    stripe.lx_ = column_index == 0 ? 0 : 20;
    stripe.ly_ = 2 * column_index;
    stripe.width_ = column_index == 0 ? 10 : 2;
    stripe.height_ = 2;
    GriddedRow& row = stripe.gridded_rows_.emplace_back();
    row.SetLLX(stripe.lx_);
    row.SetLLY(stripe.ly_);
    row.SetWidth(stripe.width_);
    row.UpdateWellHeightUpward(1, 1);
    row.AddComponent(
        circuit.GetComponentPtr(column_index == 0 ? "low" : "high"));
  }

  OrToolsGriddedStripeOptimizerConfig config;
  config.maximum_time_seconds_per_stripe = 1.0;
  config.maximum_total_time_seconds = 5.0;
  config.maximum_sweeps = 1;
  config.minimum_p_well_height = 1;
  config.minimum_n_well_height = 1;
  config.target_components_per_model = 1;
  config.maximum_components_per_model = 2;
  const OrToolsGriddedStripeOptimizerResult result =
      OrToolsGriddedStripeOptimizer(&circuit, config).Optimize(&columns);

  ASSERT_EQ(result.stripes.size(), 2U);
  EXPECT_EQ(result.stripes[0].column_index, 0);
  EXPECT_EQ(result.stripes[1].column_index, 1);
  EXPECT_DOUBLE_EQ(result.stripes[0].initial_priority_headroom, 8.0);
  EXPECT_DOUBLE_EQ(result.stripes[1].initial_priority_headroom, 0.0);
  EXPECT_GT(result.stripes[0].initial_priority_headroom,
            result.stripes[1].initial_priority_headroom);
}

}  // namespace dali
