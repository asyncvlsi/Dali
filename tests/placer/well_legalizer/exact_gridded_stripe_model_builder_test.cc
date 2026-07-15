/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/exact_gridded_stripe_model_builder.h"

#include <gtest/gtest.h>

namespace dali {

TEST(ExactGriddedStripeModelBuilderTest,
     BuildsLocalVariablesAndFixedExternalAnchors) {
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
  circuit.AddComponent("outside", "cell", 10, 0, PLACED);
  circuit.AddNet("internal", 2);
  circuit.AddComponentPinToNet("first", "pin", "internal");
  circuit.AddComponentPinToNet("second", "pin", "internal");
  circuit.AddNet("crossing", 2);
  circuit.AddComponentPinToNet("first", "pin", "crossing");
  circuit.AddComponentPinToNet("outside", "pin", "crossing");

  Stripe stripe;
  stripe.lx_ = 0;
  stripe.ly_ = 0;
  stripe.width_ = 8;
  stripe.height_ = 4;
  GriddedRow& row = stripe.gridded_rows_.emplace_back();
  row.SetLLX(0);
  row.SetLLY(0);
  row.SetWidth(8);
  row.UpdateWellHeightUpward(1, 1);
  row.AddComponent(circuit.GetComponentPtr("first"));
  row.AddComponent(circuit.GetComponentPtr("second"));

  ExactGriddedStripeModelBuilderConfig config;
  config.minimum_p_well_height = 1;
  config.minimum_n_well_height = 1;
  ExactGriddedStripeBuildResult result =
      ExactGriddedStripeModelBuilder(&circuit, config).Build(&stripe, 7);

  EXPECT_TRUE(result.model.Validate().empty());
  ASSERT_EQ(result.model.stripes.size(), 1U);
  EXPECT_EQ(result.model.stripes[0].stripe_id, 7);
  EXPECT_EQ(result.model.stripes[0].maximum_rows, 1);
  ASSERT_EQ(result.model.cells.size(), 2U);
  EXPECT_EQ(result.model.cells[0].candidate_stripe_ids, (std::vector<int>{7}));
  EXPECT_EQ(result.model.cells[0].initial_start_row, 0);
  ASSERT_EQ(result.model.nets.size(), 2U);

  int fixed_pin_count = 0;
  for (const ExactGriddedNet& net : result.model.nets) {
    for (const ExactGriddedNetPin& pin : net.pins) {
      if (pin.component_id < 0) ++fixed_pin_count;
    }
  }
  EXPECT_EQ(fixed_pin_count, 1);
  EXPECT_EQ(result.rows, (std::vector<GriddedRow*>{&row}));
  EXPECT_EQ(result.components.size(), 2U);
  EXPECT_EQ(result.affected_net_ids.size(), 2U);
}

}  // namespace dali
