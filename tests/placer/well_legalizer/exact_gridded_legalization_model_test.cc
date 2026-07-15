/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/exact_gridded_legalization_model.h"

#include <gtest/gtest.h>

namespace dali {

ExactGriddedLegalizationModel MakeValidExactModel() {
  ExactGriddedLegalizationModel model;
  model.stripes = {{0, 0, 0, 20, 20, 4, 1, 1, 1, 1, {}}};
  model.cells = {
      {0, 4, 3, 2, 3, {{1, 2, true}}, {0}},
      {1, 3, 6, 8, 7, {{2, 1, false}, {1, 2, true}}, {0}},
  };
  model.nets = {
      {{{{0, 1.0, 1.0, 1.0, 2.0, 0.0, 0.0}, {1, 1.5, 1.0, 1.5, 3.0, 0.0, 0.0}}},
       2.0}};
  return model;
}

TEST(ExactGriddedLegalizationModelTest, AcceptsCompleteModel) {
  EXPECT_TRUE(MakeValidExactModel().Validate().empty());
}

TEST(ExactGriddedLegalizationModelTest, AcceptsLegalRowHints) {
  ExactGriddedLegalizationModel model = MakeValidExactModel();
  model.stripes[0].initial_rows = {
      {true, 0, 2, 2},
      {true, 6, 1, 2},
      {false, 10, 0, 0},
      {false, 10, 0, 0},
  };
  EXPECT_TRUE(model.Validate().empty());
}

TEST(ExactGriddedLegalizationModelTest, RejectsOverlappingRowHints) {
  ExactGriddedLegalizationModel model = MakeValidExactModel();
  model.stripes[0].initial_rows = {
      {true, 0, 2, 2},
      {true, 3, 1, 2},
      {false, 10, 0, 0},
      {false, 10, 0, 0},
  };
  EXPECT_EQ(model.Validate(), "stripe row hints overlap vertically");
}

TEST(ExactGriddedLegalizationModelTest, RejectsUnknownCandidateStripe) {
  ExactGriddedLegalizationModel model = MakeValidExactModel();
  model.cells[0].candidate_stripe_ids = {7};
  EXPECT_EQ(model.Validate(),
            "component refers to an unknown candidate stripe");
}

TEST(ExactGriddedLegalizationModelTest, RejectsDuplicateComponentId) {
  ExactGriddedLegalizationModel model = MakeValidExactModel();
  model.cells[1].component_id = model.cells[0].component_id;
  EXPECT_EQ(model.Validate(), "component ids must be unique");
}

TEST(ExactGriddedLegalizationModelTest, RejectsNonPositiveCellHeight) {
  ExactGriddedLegalizationModel model = MakeValidExactModel();
  model.cells[0].height = 0;
  EXPECT_EQ(model.Validate(), "component dimensions must be positive");
}

TEST(ExactGriddedLegalizationModelTest, RejectsComponentTallerThanStripe) {
  ExactGriddedLegalizationModel model = MakeValidExactModel();
  model.stripes[0].maximum_rows = 1;
  EXPECT_EQ(model.Validate(),
            "component has more regions than a candidate stripe has rows");
}

TEST(ExactGriddedLegalizationModelTest, RejectsPartialPlacementHint) {
  ExactGriddedLegalizationModel model = MakeValidExactModel();
  model.cells[0].initial_stripe_id = 0;
  EXPECT_EQ(model.Validate(),
            "component placement hints require both stripe and row ids");
}

TEST(ExactGriddedLegalizationModelTest, RejectsHintOutsideCandidateStripes) {
  ExactGriddedLegalizationModel model = MakeValidExactModel();
  model.stripes.push_back({1, 0, 0, 20, 20, 4, 1, 1, 1, 1, {}});
  model.cells[0].initial_stripe_id = 1;
  model.cells[0].initial_start_row = 0;
  EXPECT_EQ(model.Validate(),
            "component placement hint refers to a non-candidate stripe");
}

TEST(ExactGriddedLegalizationModelTest, RejectsUnknownNetComponent) {
  ExactGriddedLegalizationModel model = MakeValidExactModel();
  model.nets[0].pins[0].component_id = 99;
  EXPECT_EQ(model.Validate(), "net pin refers to an unknown component");
}

}  // namespace dali
