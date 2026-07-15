/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/exact_gridded_legalization_model_builder.h"

#include <gtest/gtest.h>

namespace dali {

Circuit MakeExactModelBuilderCircuit() {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(2, 3);
  circuit.ReserveSpaceForDesignImp(3, 0, 3);

  Macro* macro = circuit.AddMacro("cell", 4, 6);
  macro->AddWellRect(false, 0, 0, 2, 1);
  macro->AddWellRect(true, 0, 1, 2, 2);
  circuit.AddMacroPin(macro, "pin", true)->SetOffset(0.5, 0.25);
  circuit.AddComponent("first", "cell", 0, 0, PLACED, N, true);
  circuit.AddComponent("outside", "cell", 8, 6, PLACED, FS, true);
  circuit.AddComponent("fixed", "cell", 16, 12, FIXED, N, true);

  circuit.AddNet("crossing", 2);
  circuit.AddComponentPinToNet("first", "pin", "crossing");
  circuit.AddComponentPinToNet("outside", "pin", "crossing");
  circuit.AddNet("anchored", 2);
  circuit.AddComponentPinToNet("first", "pin", "anchored");
  circuit.AddComponentPinToNet("fixed", "pin", "anchored");
  circuit.AddNet("high_fanout", 3);
  circuit.AddComponentPinToNet("first", "pin", "high_fanout");
  circuit.AddComponentPinToNet("outside", "pin", "high_fanout");
  circuit.AddComponentPinToNet("fixed", "pin", "high_fanout");
  return circuit;
}

TEST(ExactGriddedLegalizationModelBuilderTest,
     PreservesExternalPinsOrientationAndPhysicalScales) {
  Circuit circuit = MakeExactModelBuilderCircuit();
  Component* component = circuit.GetComponentPtr("first");
  Pin* pin = component->MacroPtr()->GetPinPtr("pin");
  std::vector<ExactGriddedComponentDomain> domains = {{component, {7}}};
  std::vector<ExactGriddedStripe> stripes = {{7, 0, 0, 20, 20, 10, 1, 2, 1, 1}};
  ExactGriddedModelBuilderConfig config;
  config.net_ignore_threshold = 3;

  ExactGriddedLegalizationModel model =
      ExactGriddedLegalizationModelBuilder(&circuit, config)
          .Build(domains, stripes);

  ASSERT_EQ(model.Validate(), "");
  EXPECT_DOUBLE_EQ(model.distance_scale_x, 2.0);
  EXPECT_DOUBLE_EQ(model.distance_scale_y, 3.0);
  ASSERT_EQ(model.cells.size(), 1U);
  EXPECT_EQ(model.cells[0].component_id, component->Id());
  EXPECT_EQ(model.cells[0].candidate_stripe_ids, std::vector<int>({7}));
  ASSERT_EQ(model.cells[0].regions.size(), 1U);
  EXPECT_EQ(model.cells[0].regions[0].p_well_height, 1);
  EXPECT_EQ(model.cells[0].regions[0].n_well_height, 1);

  ASSERT_EQ(model.nets.size(), 2U);
  for (const ExactGriddedNet& net : model.nets) {
    ASSERT_EQ(net.pins.size(), 2U);
    EXPECT_EQ(net.pins[0].component_id, component->Id());
    EXPECT_DOUBLE_EQ(net.pins[0].offset_x_n, pin->OffsetX(N));
    EXPECT_DOUBLE_EQ(net.pins[0].offset_y_n, pin->OffsetY(N));
    EXPECT_DOUBLE_EQ(net.pins[0].offset_x_fs, pin->OffsetX(FS));
    EXPECT_DOUBLE_EQ(net.pins[0].offset_y_fs, pin->OffsetY(FS));
    EXPECT_EQ(net.pins[1].component_id, -1);
  }
}

TEST(ExactGriddedLegalizationModelBuilderTest,
     ModelsOtherSelectedComponentsAsVariables) {
  Circuit circuit = MakeExactModelBuilderCircuit();
  Component* first = circuit.GetComponentPtr("first");
  Component* outside = circuit.GetComponentPtr("outside");
  std::vector<ExactGriddedComponentDomain> domains = {{first, {0}},
                                                      {outside, {0}}};
  std::vector<ExactGriddedStripe> stripes = {{0, 0, 0, 20, 20, 10, 0, 0, 1, 1}};

  ExactGriddedLegalizationModel model =
      ExactGriddedLegalizationModelBuilder(&circuit).Build(domains, stripes);

  ASSERT_EQ(model.Validate(), "");
  ASSERT_EQ(model.nets.size(), 3U);
  int variable_pin_count = 0;
  for (const ExactGriddedNet& net : model.nets) {
    for (const ExactGriddedNetPin& pin : net.pins) {
      if (pin.component_id >= 0) ++variable_pin_count;
    }
  }
  EXPECT_EQ(variable_pin_count, 5);
}

}  // namespace dali
