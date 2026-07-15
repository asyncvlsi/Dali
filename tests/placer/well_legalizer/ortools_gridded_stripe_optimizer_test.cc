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
  EXPECT_EQ(result.accepted_reassignment_models, 0);
  EXPECT_EQ(result.accepted_reassigned_components, 0);
  EXPECT_DOUBLE_EQ(result.fixed_row_hpwl_improvement,
                   result.hpwl_before - result.hpwl_after);
  EXPECT_DOUBLE_EQ(result.reassignment_hpwl_improvement, 0.0);
  EXPECT_LT(result.hpwl_after, result.hpwl_before);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("move_left")->LLX(), 0.0);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("move_right")->LLX(), 2.0);
  EXPECT_TRUE(row.HasLegalComponentPlacement());
}

TEST(OrToolsGriddedStripeOptimizerTest,
     AppliesAnImprovingRowSwapTransactionally) {
  if (!OrToolsCompactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(4, 0, 2);
  Macro* macro = circuit.AddMacro("cell", 4, 2);
  macro->AddWellRect(false, 0, 0, 4, 1);
  macro->AddWellRect(true, 0, 1, 4, 2);
  circuit.AddMacroPin(macro, "pin", true)->SetOffset(1, 1);
  circuit.AddComponent("move_upper", "cell", 0, 0, PLACED, N);
  circuit.AddComponent("move_lower", "cell", 0, 2, PLACED, FS);
  circuit.AddComponent("lower_anchor", "cell", 0, 0, FIXED, N);
  circuit.AddComponent("upper_anchor", "cell", 0, 2, FIXED, FS);
  circuit.AddNet("upper_net", 2);
  circuit.AddComponentPinToNet("move_upper", "pin", "upper_net");
  circuit.AddComponentPinToNet("upper_anchor", "pin", "upper_net");
  circuit.AddNet("lower_net", 2);
  circuit.AddComponentPinToNet("move_lower", "pin", "lower_net");
  circuit.AddComponentPinToNet("lower_anchor", "pin", "lower_net");

  std::vector<StripeColumn> columns(1);
  Stripe& stripe = columns[0].stripe_list_.emplace_back();
  stripe.lx_ = 0;
  stripe.ly_ = 0;
  stripe.width_ = 6;
  stripe.height_ = 4;
  stripe.gridded_rows_.reserve(2);
  GriddedRow& lower_row = stripe.gridded_rows_.emplace_back();
  lower_row.SetLLX(0);
  lower_row.SetLLY(0);
  lower_row.SetWidth(6);
  lower_row.UpdateWellHeightUpward(1, 1);
  lower_row.AddComponent(circuit.GetComponentPtr("move_upper"));
  lower_row.SetOrient(true);
  GriddedRow& upper_row = stripe.gridded_rows_.emplace_back();
  upper_row.SetLLX(0);
  upper_row.SetLLY(2);
  upper_row.SetWidth(6);
  upper_row.UpdateWellHeightUpward(1, 1);
  upper_row.AddComponent(circuit.GetComponentPtr("move_lower"));
  upper_row.SetOrient(false);

  OrToolsGriddedStripeOptimizerConfig config;
  config.maximum_time_seconds_per_stripe = 10.0;
  config.maximum_total_time_seconds = 20.0;
  config.maximum_sweeps = 1;
  config.minimum_p_well_height = 1;
  config.minimum_n_well_height = 1;
  config.maximum_row_displacement = 1;
  config.maximum_row_assignment_changes = 2;
  OrToolsGriddedStripeOptimizerResult result =
      OrToolsGriddedStripeOptimizer(&circuit, config).Optimize(&columns);

  EXPECT_EQ(result.accepted_models, 1);
  EXPECT_EQ(result.accepted_reassignment_models, 1);
  EXPECT_EQ(result.accepted_reassigned_components, 2);
  EXPECT_DOUBLE_EQ(result.fixed_row_hpwl_improvement, 0.0);
  EXPECT_DOUBLE_EQ(result.reassignment_hpwl_improvement,
                   result.hpwl_before - result.hpwl_after);
  EXPECT_LT(result.hpwl_after, result.hpwl_before);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("move_upper")->LLY(), 2.0);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("move_lower")->LLY(), 0.0);
  ASSERT_EQ(lower_row.Components().size(), 1U);
  ASSERT_EQ(upper_row.Components().size(), 1U);
  EXPECT_EQ(lower_row.Components()[0], circuit.GetComponentPtr("move_lower"));
  EXPECT_EQ(upper_row.Components()[0], circuit.GetComponentPtr("move_upper"));
  EXPECT_TRUE(lower_row.HasLegalComponentPlacement());
  EXPECT_TRUE(upper_row.HasLegalComponentPlacement());
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

}  // namespace dali
