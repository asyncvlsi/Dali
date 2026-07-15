/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/exact_gridded_legalization_window_analyzer.h"

#include <gtest/gtest.h>

#include "dali/placer/well_legalizer/ortools_compact_gridded_legalizer.h"

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
  EXPECT_EQ(result.feasible_hint_windows, 1);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_DOUBLE_EQ(result.hinted_hpwl_sum,
                   result.windows[0].current_weighted_hpwl);
  EXPECT_GT(result.solved_current_hpwl_sum, result.solved_incumbent_hpwl_sum);
  EXPECT_EQ(result.windows[0].hint_validation_status,
            ExactGriddedLegalizationStatus::kOptimal);
  EXPECT_LT(result.windows[0].solved_weighted_hpwl,
            result.windows[0].current_weighted_hpwl);
  EXPECT_DOUBLE_EQ(result.windows[0].solved_weighted_hpwl,
                   result.windows[0].best_objective_bound);
  EXPECT_DOUBLE_EQ(first->LLX(), first_x);
  EXPECT_DOUBLE_EQ(second->LLX(), second_x);
}

TEST(ExactGriddedLegalizationWindowAnalyzerTest,
     FindsAdjacentRowHeadroomWithoutChangingProductionPlacement) {
  if (!OrToolsCompactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(5, 0, 1);
  Macro* macro = circuit.AddMacro("cell", 2, 2);
  macro->AddWellRect(false, 0, 0, 2, 1);
  macro->AddWellRect(true, 0, 1, 2, 2);
  circuit.AddMacroPin(macro, "pin", true)->SetOffset(1, 1);
  circuit.AddComponent("stay_lower", "cell", 0, 0, PLACED, N);
  circuit.AddComponent("move_upper", "cell", 2, 0, PLACED, N);
  circuit.AddComponent("stay_upper", "cell", 0, 2, PLACED, FS);
  circuit.AddComponent("stay_top", "cell", 0, 4, PLACED, N);
  circuit.AddComponent("anchor", "cell", 10, 2, FIXED, N);
  circuit.AddNet("move_net", 2);
  circuit.AddComponentPinToNet("move_upper", "pin", "move_net");
  circuit.AddComponentPinToNet("anchor", "pin", "move_net");

  std::vector<StripeColumn> columns(1);
  columns[0].stripe_list_.resize(1);
  Stripe& stripe = columns[0].stripe_list_[0];
  stripe.lx_ = 0;
  stripe.ly_ = 0;
  stripe.width_ = 4;
  stripe.height_ = 6;
  stripe.gridded_rows_.resize(3);
  GriddedRow& lower_row = stripe.gridded_rows_[0];
  lower_row.SetLLX(0);
  lower_row.SetLLY(0);
  lower_row.SetWidth(4);
  lower_row.UpdateWellHeightUpward(1, 1);
  lower_row.AddComponent(circuit.GetComponentPtr("stay_lower"));
  Component* move_upper = circuit.GetComponentPtr("move_upper");
  lower_row.AddComponent(move_upper);
  lower_row.SetOrient(true);
  GriddedRow& upper_row = stripe.gridded_rows_[1];
  upper_row.SetLLX(0);
  upper_row.SetLLY(2);
  upper_row.SetWidth(4);
  upper_row.UpdateWellHeightUpward(1, 1);
  upper_row.AddComponent(circuit.GetComponentPtr("stay_upper"));
  upper_row.SetOrient(false);
  GriddedRow& top_row = stripe.gridded_rows_[2];
  top_row.SetLLX(0);
  top_row.SetLLY(4);
  top_row.SetWidth(4);
  top_row.UpdateWellHeightUpward(1, 1);
  top_row.AddComponent(circuit.GetComponentPtr("stay_top"));
  top_row.SetOrient(true);

  const double original_x = move_upper->LLX();
  const double original_y = move_upper->LLY();
  const ComponentOrient original_orient = move_upper->Orient();
  ExactGriddedWindowAnalyzerConfig config;
  config.target_components_per_window = 3;
  config.maximum_components_per_window = 6;
  config.minimum_rows_per_window = 2;
  config.maximum_windows = 1;
  config.maximum_time_seconds_per_window = 10.0;
  config.maximum_row_displacement = 1;
  config.fix_row_geometry = true;
  config.use_compact_solver = true;
  config.overlap_row_windows = true;
  ExactGriddedWindowAnalysis result =
      ExactGriddedLegalizationWindowAnalyzer(&circuit, config)
          .Analyze(&columns);

  EXPECT_TRUE(result.available);
  EXPECT_EQ(result.candidate_windows, 2);
  EXPECT_EQ(result.attempted_windows, 1);
  EXPECT_EQ(result.solved_windows, 1);
  EXPECT_EQ(result.optimal_windows, 1);
  EXPECT_EQ(result.improved_windows, 1);
  EXPECT_GT(result.reassigned_components, 0);
  ASSERT_EQ(result.windows.size(), 1U);
  EXPECT_GT(result.windows[0].row_assignment_choice_count,
            result.windows[0].component_count);
  EXPECT_GT(result.windows[0].reassigned_component_count, 0);
  EXPECT_LT(result.windows[0].solved_weighted_hpwl,
            result.windows[0].current_weighted_hpwl);
  EXPECT_DOUBLE_EQ(move_upper->LLX(), original_x);
  EXPECT_DOUBLE_EQ(move_upper->LLY(), original_y);
  EXPECT_EQ(move_upper->Orient(), original_orient);
}

}  // namespace dali
