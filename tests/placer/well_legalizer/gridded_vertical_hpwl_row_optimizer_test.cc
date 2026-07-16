/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 ******************************************************************************/
#include "dali/placer/well_legalizer/gridded_vertical_hpwl_row_optimizer.h"

#include <gtest/gtest.h>

#include <string>

namespace dali {

TEST(GriddedVerticalHpwlRowOptimizerTest,
     CommitsLegalInterleavedRowAssignment) {
  if (!OrToolsVerticalHpwlRowAssignment::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }

  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(12, 0, 6);
  Macro* macro = circuit.AddMacro("cell", 1, 2);
  macro->AddWellRect(false, 0, 0, 1, 1);
  macro->AddWellRect(true, 0, 1, 1, 2);
  circuit.AddMacroPin(macro, "pin", true)->SetOffset(0.5, 1.0);

  for (int component_id = 0; component_id < 6; ++component_id) {
    const double y = component_id < 3 ? 0.0 : 2.0;
    circuit.AddComponent("cell_" + std::to_string(component_id), "cell",
                         component_id % 3, y, PLACED,
                         component_id < 3 ? N : FS);
    circuit.AddComponent("anchor_" + std::to_string(component_id), "cell", -2,
                         component_id % 2 == 0 ? 0.0 : 2.0, FIXED,
                         component_id % 2 == 0 ? N : FS);
  }
  for (int component_id = 0; component_id < 6; ++component_id) {
    const std::string net_name = "net_" + std::to_string(component_id);
    circuit.AddNet(net_name, 2);
    circuit.AddComponentPinToNet("cell_" + std::to_string(component_id), "pin",
                                 net_name);
    circuit.AddComponentPinToNet("anchor_" + std::to_string(component_id),
                                 "pin", net_name);
  }

  std::vector<StripeColumn> columns(1);
  Stripe& stripe = columns[0].stripe_list_.emplace_back();
  stripe.lx_ = 0;
  stripe.ly_ = 0;
  stripe.width_ = 3;
  stripe.height_ = 4;
  stripe.gridded_rows_.reserve(2);
  for (int row_index = 0; row_index < 2; ++row_index) {
    GriddedRow& row = stripe.gridded_rows_.emplace_back();
    row.SetLLX(0);
    row.SetLLY(2 * row_index);
    row.SetWidth(3);
    row.UpdateWellHeightUpward(1, 1);
    row.SetOrient(row_index == 0);
    for (int offset = 0; offset < 3; ++offset) {
      row.AddComponent(circuit.GetComponentPtr(
          "cell_" + std::to_string(3 * row_index + offset)));
    }
    row.SetUsedSize(3);
  }

  GriddedVerticalHpwlRowOptimizerConfig config;
  config.maximum_time_seconds_per_window = 10.0;
  config.maximum_total_time_seconds = 20.0;
  const GriddedVerticalHpwlRowOptimizerResult result =
      GriddedVerticalHpwlRowOptimizer(&circuit, config).Optimize(&columns);

  EXPECT_EQ(result.attempted_windows, 1);
  EXPECT_EQ(result.solved_windows, 1);
  EXPECT_EQ(result.accepted_windows, 1);
  EXPECT_EQ(result.reassigned_components, 2);
  EXPECT_LT(result.hpwl_after, result.hpwl_before);
  for (const GriddedRow& row : stripe.gridded_rows_) {
    EXPECT_TRUE(row.HasLegalComponentPlacement());
  }
}

}  // namespace dali
