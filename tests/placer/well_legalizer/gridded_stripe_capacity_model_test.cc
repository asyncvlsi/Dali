/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/gridded_stripe_capacity_model.h"

#include <gtest/gtest.h>

namespace dali {

class GriddedStripeCapacityModelTest : public testing::Test {
 protected:
  GriddedStripeCapacityModelTest() {
    circuit_.SetManufacturingGrid(1);
    circuit_.SetUnitsDistanceMicrons(1);
    circuit_.SetGridValue(1, 1);
    circuit_.ReserveSpaceForDesignImp(20, 0, 30);
    Macro* macro = circuit_.AddMacro("cell", 4, 10);
    macro->AddWellRect(false, 0, 0, 4, 4);
    macro->AddWellRect(true, 0, 4, 4, 10);
  }

  Component* AddComponent(const std::string& name) {
    circuit_.AddComponent(name, "cell", 0, 0, PLACED);
    return circuit_.GetComponentPtr(name);
  }

  Stripe MakeStripe(int lx, int ly) const {
    Stripe stripe;
    stripe.lx_ = lx;
    stripe.ly_ = ly;
    stripe.width_ = 10;
    stripe.height_ = 10;
    return stripe;
  }

  Circuit circuit_;
};

TEST_F(GriddedStripeCapacityModelTest, EstimatesEveryFragmentIndependently) {
  StripeColumn column;
  column.stripe_list_.push_back(MakeStripe(0, 0));
  column.stripe_list_.push_back(MakeStripe(0, 20));
  column.stripe_list_[0].component_ptrs_vec_.push_back(AddComponent("lower"));
  column.stripe_list_[1].component_ptrs_vec_.push_back(AddComponent("upper"));

  GriddedCapacityConfig config;
  config.reserved_width = 2;
  const GriddedStripeCapacitySummary summary =
      GriddedStripeCapacityModel(config).Estimate(column);

  EXPECT_EQ(summary.entries.size(), 2U);
  EXPECT_EQ(summary.estimated_row_count, 2);
  EXPECT_EQ(summary.required_gridded_area, 160U);
  EXPECT_EQ(summary.available_gridded_area, 160U);
  EXPECT_EQ(summary.predicted_overflow_area, 0U);
}

TEST_F(GriddedStripeCapacityModelTest,
       DoesNotLetDisconnectedFragmentsShareOneGriddedRow) {
  Component* lower = AddComponent("lower");
  Component* upper = AddComponent("upper");
  StripeColumn column;
  column.stripe_list_.push_back(MakeStripe(0, 0));
  column.stripe_list_.push_back(MakeStripe(0, 20));
  column.stripe_list_[0].component_ptrs_vec_.push_back(lower);
  column.stripe_list_[1].component_ptrs_vec_.push_back(upper);

  GriddedCapacityConfig config;
  config.reserved_width = 2;
  const GriddedStripeCapacitySummary summary =
      GriddedStripeCapacityModel(config).Estimate(column);
  const GriddedCapacityEstimate combined =
      GriddedCapacityEstimator(config).Estimate({lower, upper}, 10, 20, 200);

  EXPECT_EQ(summary.required_gridded_area, 160U);
  EXPECT_EQ(combined.required_gridded_area, 80U);
  EXPECT_EQ(summary.estimated_row_count, 2);
  EXPECT_EQ(combined.estimated_row_count, 1);
}

}  // namespace dali
