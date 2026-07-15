/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/exact_gridded_boundary_model_builder.h"

#include <gtest/gtest.h>

#include "dali/placer/well_legalizer/ortools_compact_gridded_legalizer.h"

namespace dali {

TEST(ExactGriddedBoundaryModelBuilderTest, BuildsAndSolvesACrossStripeSwap) {
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

  Stripe left_stripe;
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

  Stripe right_stripe;
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

  ExactGriddedBoundaryModelBuilderConfig builder_config;
  builder_config.minimum_p_well_height = 1;
  builder_config.minimum_n_well_height = 1;
  ExactGriddedBoundaryBuildResult build =
      ExactGriddedBoundaryModelBuilder(&circuit, builder_config)
          .Build(&left_stripe, 3, 0, 0, &right_stripe, 4, 0, 0);

  ASSERT_EQ(build.model.stripes.size(), 2U);
  ASSERT_EQ(build.model.cells.size(), 2U);
  EXPECT_EQ(build.model.cells[0].candidate_stripe_ids,
            (std::vector<int>{3, 4}));
  EXPECT_EQ(build.model.cells[1].candidate_stripe_ids,
            (std::vector<int>{3, 4}));
  ASSERT_EQ(build.row_sets.size(), 2U);
  EXPECT_EQ(build.row_sets[0].rows, (std::vector<GriddedRow*>{&left_row}));
  EXPECT_EQ(build.row_sets[1].rows, (std::vector<GriddedRow*>{&right_row}));

  if (!OrToolsCompactGriddedLegalizer::IsAvailable()) {
    GTEST_SKIP() << "Dali was built without OR-Tools 9.15.x";
  }
  ExactGriddedLegalizationConfig solver_config;
  solver_config.maximum_time_seconds = 10.0;
  solver_config.maximum_row_displacement = 0;
  solver_config.maximum_row_assignment_changes = 2;
  solver_config.fix_row_geometry = true;
  ExactGriddedLegalizationResult solution =
      OrToolsCompactGriddedLegalizer().Solve(build.model, solver_config);

  ASSERT_TRUE(solution.HasSolution()) << solution.message;
  ASSERT_EQ(solution.cells.size(), 2U);
  for (const ExactGriddedCellPlacement& placement : solution.cells) {
    if (placement.component_id == circuit.GetComponentPtr("move_right")->Id()) {
      EXPECT_EQ(placement.stripe_id, 4);
    } else if (placement.component_id ==
               circuit.GetComponentPtr("move_left")->Id()) {
      EXPECT_EQ(placement.stripe_id, 3);
    }
  }
}

}  // namespace dali
