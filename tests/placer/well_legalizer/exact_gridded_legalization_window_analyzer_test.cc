/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/exact_gridded_legalization_window_analyzer.h"

#include <gtest/gtest.h>

namespace dali {

TEST(ExactGriddedLegalizationWindowAnalyzerTest,
     FindsHeadroomWithoutChangingProductionPlacement) {
  if (!OrToolsExactGriddedLegalizer::IsAvailable()) {
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
  circuit.AddComponent("first", "cell", 0, 0, PLACED);
  circuit.AddComponent("second", "cell", 2, 0, PLACED);
  circuit.AddComponent("anchor", "cell", 18, 0, FIXED);
  circuit.AddNet("first_net", 2);
  circuit.AddComponentPinToNet("first", "pin", "first_net");
  circuit.AddComponentPinToNet("anchor", "pin", "first_net");
  circuit.AddNet("second_net", 2);
  circuit.AddComponentPinToNet("second", "pin", "second_net");
  circuit.AddComponentPinToNet("anchor", "pin", "second_net");

  std::vector<StripeColumn> columns(1);
  columns[0].stripe_list_.resize(1);
  Stripe& stripe = columns[0].stripe_list_[0];
  stripe.lx_ = 0;
  stripe.ly_ = 0;
  stripe.width_ = 20;
  stripe.height_ = 2;
  stripe.gridded_rows_.resize(1);
  GriddedRow& row = stripe.gridded_rows_[0];
  row.SetLLX(0);
  row.SetLLY(0);
  row.SetWidth(20);
  row.UpdateWellHeightUpward(1, 1);
  Component* first = circuit.GetComponentPtr("first");
  Component* second = circuit.GetComponentPtr("second");
  row.AddComponent(first);
  row.AddComponent(second);

  const double first_x = first->LLX();
  const double second_x = second->LLX();
  ExactGriddedWindowAnalyzerConfig config;
  config.target_components_per_window = 2;
  config.maximum_components_per_window = 4;
  config.maximum_windows = 1;
  config.maximum_time_seconds_per_window = 10.0;
  ExactGriddedWindowAnalysis result =
      ExactGriddedLegalizationWindowAnalyzer(&circuit, config)
          .Analyze(&columns);

  EXPECT_TRUE(result.available);
  EXPECT_EQ(result.candidate_windows, 1);
  EXPECT_EQ(result.attempted_windows, 1);
  EXPECT_EQ(result.solved_windows, 1);
  EXPECT_EQ(result.optimal_windows, 1);
  EXPECT_GT(result.solved_current_hpwl_sum, result.solved_incumbent_hpwl_sum);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_LT(result.windows[0].solved_weighted_hpwl,
            result.windows[0].current_weighted_hpwl);
  EXPECT_DOUBLE_EQ(result.windows[0].solved_weighted_hpwl,
                   result.windows[0].best_objective_bound);
  EXPECT_DOUBLE_EQ(first->LLX(), first_x);
  EXPECT_DOUBLE_EQ(second->LLX(), second_x);
}

}  // namespace dali
