/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/ortools_gridded_boundary_refiner.h"

#include <gtest/gtest.h>

#include "dali/placer/well_legalizer/ortools_compact_gridded_legalizer.h"

namespace dali {

TEST(OrToolsGriddedBoundaryRefinerTest, FindsAndAppliesAnAdjacentColumnSwap) {
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
  circuit.AddComponent("move_right", "cell", 0, 0, PLACED);
  circuit.AddComponent("move_left", "cell", 6, 0, PLACED);
  circuit.AddComponent("left_anchor", "cell", 0, 0, FIXED);
  circuit.AddComponent("right_anchor", "cell", 6, 0, FIXED);
  circuit.AddNet("right_net", 2);
  circuit.AddComponentPinToNet("move_right", "pin", "right_net");
  circuit.AddComponentPinToNet("right_anchor", "pin", "right_net");
  circuit.AddNet("left_net", 2);
  circuit.AddComponentPinToNet("move_left", "pin", "left_net");
  circuit.AddComponentPinToNet("left_anchor", "pin", "left_net");

  std::vector<StripeColumn> columns(2);
  columns[0].lx_ = 0;
  columns[0].width_ = 4;
  Stripe& left_stripe = columns[0].stripe_list_.emplace_back();
  left_stripe.lx_ = 0;
  left_stripe.ly_ = 0;
  left_stripe.width_ = 4;
  left_stripe.height_ = 2;
  GriddedRow& left_row = left_stripe.gridded_rows_.emplace_back();
  left_row.SetLLX(0);
  left_row.SetLLY(0);
  left_row.SetWidth(4);
  left_row.UpdateWellHeightUpward(1, 1);
  left_row.AddComponent(circuit.GetComponentPtr("move_right"));

  columns[1].lx_ = 6;
  columns[1].width_ = 4;
  Stripe& right_stripe = columns[1].stripe_list_.emplace_back();
  right_stripe.lx_ = 6;
  right_stripe.ly_ = 0;
  right_stripe.width_ = 4;
  right_stripe.height_ = 2;
  GriddedRow& right_row = right_stripe.gridded_rows_.emplace_back();
  right_row.SetLLX(6);
  right_row.SetLLY(0);
  right_row.SetWidth(4);
  right_row.UpdateWellHeightUpward(1, 1);
  right_row.AddComponent(circuit.GetComponentPtr("move_left"));

  OrToolsGriddedBoundaryRefinerConfig config;
  config.maximum_time_seconds_per_model = 10.0;
  config.maximum_total_time_seconds = 20.0;
  config.minimum_p_well_height = 1;
  config.minimum_n_well_height = 1;
  config.maximum_assignment_changes = 2;
  const OrToolsGriddedBoundaryRefinerResult result =
      OrToolsGriddedBoundaryRefiner(&circuit, config).Optimize(&columns);

  EXPECT_EQ(result.candidate_windows, 1);
  EXPECT_EQ(result.attempted_models, 1);
  EXPECT_EQ(result.solved_models, 1);
  EXPECT_EQ(result.accepted_models, 1);
  EXPECT_EQ(result.accepted_cross_stripe_components, 2);
  EXPECT_LT(result.hpwl_after, result.hpwl_before);
  EXPECT_EQ(left_row.Components()[0], circuit.GetComponentPtr("move_left"));
  EXPECT_EQ(right_row.Components()[0], circuit.GetComponentPtr("move_right"));
}

}  // namespace dali
