/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/
/*
 * Appending inverter pairs to a registered delay line in a live circuit.
 *
 * The property under test throughout is that an extension is invisible to
 * everything it did not touch. Placement is carried forward across the
 * mutation, so an id that shifts or a coordinate that moves would silently
 * corrupt a placement in progress rather than fail loudly.
 */
#include "dali/timing/delay_line_extension.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <algorithm>
#include <string>

#include "dali/timing/delay_line_detour.h"

namespace dali {
namespace {

/**
 * A three-element delay line driving one downstream sink, plus a second,
 * independent line used for the all-or-none checks.
 */
Circuit BuildCircuitWithDelayLines() {
  Circuit circuit;
  circuit.SetDatabaseMicrons(1000);
  circuit.SetManufacturingGrid(0.001);
  circuit.AddMetalLayer("m1", 0.1, 0.1, 0.042, 0.2, 0.2, VERTICAL);
  circuit.AddMetalLayer("m2", 0.1, 0.1, 0.042, 0.2, 0.2, HORIZONTAL);
  circuit.SetGridValue(0.2, 0.2);
  circuit.SetRowHeight(0.2);

  Macro *inv = circuit.AddMacro("INV2", 0.8, 1.6);
  circuit.AddMacroPin(inv, "IN", true)->SetOffset(0.1, 0.8);
  circuit.AddMacroPin(inv, "OUT", false)->SetOffset(0.7, 0.8);

  circuit.SetUnitsDistanceMicrons(1000);
  circuit.SetDieArea(0, 0, 10000, 10000);
  circuit.ReserveSpaceForDesignImp(8, 0, 8);

  circuit.AddComponent("src", "INV2", 0.0, 0.0, PLACED, N, true);
  for (int index = 0; index < 3; ++index) {
    circuit.AddComponent("dl0_e" + std::to_string(index), "INV2",
                         1.0 + index, 2.0 + index, PLACED, N, true);
  }
  circuit.AddComponent("dl1_e0", "INV2", 7.0, 8.0, PLACED, N, true);
  circuit.AddComponent("sink", "INV2", 9.0, 9.0, PLACED, N, true);

  circuit.AddNet("n_head", 2);
  circuit.AddComponentPinToNet("src", "OUT", "n_head");
  circuit.AddComponentPinToNet("dl0_e0", "IN", "n_head");
  for (int index = 0; index < 2; ++index) {
    const std::string net = "n_dl0_" + std::to_string(index);
    circuit.AddNet(net, 2);
    circuit.AddComponentPinToNet("dl0_e" + std::to_string(index), "OUT", net);
    circuit.AddComponentPinToNet("dl0_e" + std::to_string(index + 1), "IN", net);
  }
  circuit.AddNet("n_tail", 2);
  circuit.AddComponentPinToNet("dl0_e2", "OUT", "n_tail");
  circuit.AddComponentPinToNet("sink", "IN", "n_tail");

  circuit.AddNet("n_dl1_head", 2);
  circuit.AddComponentPinToNet("src", "OUT", "n_dl1_head");
  circuit.AddComponentPinToNet("dl1_e0", "IN", "n_dl1_head");
  circuit.AddNet("n_dl1_tail", 2);
  circuit.AddComponentPinToNet("dl1_e0", "OUT", "n_dl1_tail");
  circuit.AddComponentPinToNet("sink", "IN", "n_dl1_tail");
  return circuit;
}

std::vector<DelayLineExtensionPlan> Preflight(
    Circuit &circuit, const std::vector<DelayLineExtensionRequest> &requests,
    std::string *error) {
  std::vector<DelayLineExtensionPlan> plans;
  PreflightDelayLineExtensions(circuit, requests, &plans, error);
  return plans;
}

TEST(DelayLineExtensionTest, PreflightChangesNothing) {
  Circuit circuit = BuildCircuitWithDelayLines();
  const size_t components_before = circuit.Components().size();
  const size_t nets_before = circuit.Nets().size();

  std::string error;
  std::vector<DelayLineExtensionPlan> plans =
      Preflight(circuit, {{"dl0", 2}}, &error);

  ASSERT_EQ(plans.size(), 1u) << error;
  EXPECT_EQ(circuit.Components().size(), components_before);
  EXPECT_EQ(circuit.Nets().size(), nets_before);
  // Four cells for two pairs, and one more net than cells to carry the output.
  EXPECT_EQ(plans[0].component_names.size(), 4u);
  EXPECT_EQ(plans[0].net_names.size(), 5u);
}

TEST(DelayLineExtensionTest, ExtensionAddsExactlyTheRequestedCells) {
  Circuit circuit = BuildCircuitWithDelayLines();
  const size_t components_before = circuit.Components().size();
  const size_t nets_before = circuit.Nets().size();

  std::string error;
  std::vector<DelayLineExtensionPlan> plans =
      Preflight(circuit, {{"dl0", 3}}, &error);
  ASSERT_EQ(plans.size(), 1u) << error;
  ASSERT_TRUE(ApplyDelayLineExtensions(circuit, plans, &error)) << error;

  EXPECT_EQ(circuit.Components().size(), components_before + 6);
  EXPECT_EQ(circuit.Nets().size(), nets_before + 7);
}

// Placement is carried across the mutation, so nothing that existed before it
// may move or be renumbered.
TEST(DelayLineExtensionTest, ExistingIdsCoordinatesAndStatusAreUntouched) {
  Circuit circuit = BuildCircuitWithDelayLines();
  const int sink_id = circuit.GetComponentPtr("sink")->Id();
  const int head_id = circuit.GetComponentPtr("dl0_e0")->Id();
  const int tail_net_id = circuit.GetNetPtr("n_tail")->Id();
  const double head_x = circuit.GetComponentPtr("dl0_e0")->LLX();
  const double sink_y = circuit.GetComponentPtr("sink")->LLY();
  const PlaceStatus sink_status = circuit.GetComponentPtr("sink")->Status();

  std::string error;
  std::vector<DelayLineExtensionPlan> plans =
      Preflight(circuit, {{"dl0", 1}}, &error);
  ASSERT_TRUE(ApplyDelayLineExtensions(circuit, plans, &error)) << error;

  EXPECT_EQ(circuit.GetComponentPtr("sink")->Id(), sink_id);
  EXPECT_EQ(circuit.GetComponentPtr("dl0_e0")->Id(), head_id);
  EXPECT_EQ(circuit.GetNetPtr("n_tail")->Id(), tail_net_id);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("dl0_e0")->LLX(), head_x);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("sink")->LLY(), sink_y);
  EXPECT_EQ(circuit.GetComponentPtr("sink")->Status(), sink_status);
}

// The interrupted net keeps its slot and empties; the extension's last net
// takes over driving what it used to drive.
TEST(DelayLineExtensionTest, InterruptedNetIsRetiredAndItsSinkReconnected) {
  Circuit circuit = BuildCircuitWithDelayLines();
  std::string error;
  std::vector<DelayLineExtensionPlan> plans =
      Preflight(circuit, {{"dl0", 1}}, &error);
  ASSERT_TRUE(ApplyDelayLineExtensions(circuit, plans, &error)) << error;

  EXPECT_EQ(circuit.GetNetPtr("n_tail")->PinCnt(), 0u);
  Net *last = circuit.GetNetPtr(plans[0].net_names.back());
  ASSERT_NE(last, nullptr);
  bool drives_sink = false;
  for (NetPin &net_pin : last->ComponentPins()) {
    if (net_pin.ComponentPtr()->Name() == "sink") drives_sink = true;
  }
  EXPECT_TRUE(drives_sink);
}

// The extension is only correct if the chain builder reads it back as one
// longer simple chain, in the order the cells were spliced.
TEST(DelayLineExtensionTest, ChainReadsBackLongerAndInOrder) {
  Circuit circuit = BuildCircuitWithDelayLines();
  DelayLineChain before;
  std::string error;
  ASSERT_TRUE(BuildDelayLineChain(circuit, "dl0", &before, &error)) << error;
  ASSERT_EQ(before.nodes.size(), 3u);

  std::vector<DelayLineExtensionPlan> plans =
      Preflight(circuit, {{"dl0", 2}}, &error);
  ASSERT_TRUE(ApplyDelayLineExtensions(circuit, plans, &error)) << error;

  DelayLineChain after;
  ASSERT_TRUE(BuildDelayLineChain(circuit, "dl0", &after, &error)) << error;
  ASSERT_EQ(after.nodes.size(), 7u);
  for (size_t index = 0; index < before.nodes.size(); ++index) {
    EXPECT_EQ(after.nodes[index].component_id, before.nodes[index].component_id);
  }
  for (size_t index = 0; index < plans[0].component_names.size(); ++index) {
    EXPECT_EQ(circuit.Components()[after.nodes[3 + index].component_id].Name(),
              plans[0].component_names[index]);
  }
}

TEST(DelayLineExtensionTest, EachAddedCellAppearsExactlyOnce) {
  Circuit circuit = BuildCircuitWithDelayLines();
  std::string error;
  std::vector<DelayLineExtensionPlan> plans =
      Preflight(circuit, {{"dl0", 2}}, &error);
  ASSERT_TRUE(ApplyDelayLineExtensions(circuit, plans, &error)) << error;

  DelayLineChain chain;
  ASSERT_TRUE(BuildDelayLineChain(circuit, "dl0", &chain, &error)) << error;
  for (const std::string &name : plans[0].component_names) {
    const int matches = static_cast<int>(std::count_if(
        chain.nodes.begin(), chain.nodes.end(),
        [&circuit, &name](const DelayLineNode &node) {
          return circuit.Components()[node.component_id].Name() == name;
        }));
    EXPECT_EQ(matches, 1) << name;
  }
}

TEST(DelayLineExtensionTest, UnknownLineFailsPreflightWithoutMutating) {
  Circuit circuit = BuildCircuitWithDelayLines();
  const size_t components_before = circuit.Components().size();
  std::string error;
  std::vector<DelayLineExtensionPlan> plans;
  EXPECT_FALSE(PreflightDelayLineExtensions(circuit, {{"nosuchline", 1}},
                                            &plans, &error));
  EXPECT_FALSE(error.empty());
  EXPECT_EQ(circuit.Components().size(), components_before);
}

// A batch is preflighted as a whole, so one impossible line stops the others
// before any of them is applied.
TEST(DelayLineExtensionTest, OneBadLineRejectsTheWholeBatch) {
  Circuit circuit = BuildCircuitWithDelayLines();
  const size_t components_before = circuit.Components().size();
  const size_t nets_before = circuit.Nets().size();

  std::string error;
  std::vector<DelayLineExtensionPlan> plans;
  EXPECT_FALSE(PreflightDelayLineExtensions(
      circuit, {{"dl0", 1}, {"nosuchline", 1}}, &plans, &error));

  EXPECT_EQ(circuit.Components().size(), components_before);
  EXPECT_EQ(circuit.Nets().size(), nets_before);
}

// Capacity is a budget fixed when the design was read. Asking for more than
// remains is refused at preflight rather than reallocating a vector that nets
// hold pointers into.
TEST(DelayLineExtensionTest, ExceedingReservedCapacityFailsPreflight) {
  Circuit circuit = BuildCircuitWithDelayLines();
  const size_t headroom =
      circuit.Components().capacity() - circuit.Components().size();
  const size_t components_before = circuit.Components().size();

  std::string error;
  std::vector<DelayLineExtensionPlan> plans;
  EXPECT_FALSE(PreflightDelayLineExtensions(
      circuit, {{"dl0", static_cast<int>(headroom)}}, &plans, &error));
  EXPECT_NE(error.find("reserved"), std::string::npos) << error;
  EXPECT_EQ(circuit.Components().size(), components_before);
}

TEST(DelayLineExtensionTest, ZeroPairsIsANoOp) {
  Circuit circuit = BuildCircuitWithDelayLines();
  const size_t components_before = circuit.Components().size();
  std::string error;
  std::vector<DelayLineExtensionPlan> plans =
      Preflight(circuit, {{"dl0", 0}}, &error);
  EXPECT_TRUE(plans.empty());
  ASSERT_TRUE(ApplyDelayLineExtensions(circuit, plans, &error)) << error;
  EXPECT_EQ(circuit.Components().size(), components_before);
}

// Two lines in one batch each grow by their own amount, and neither disturbs
// the other's chain.
TEST(DelayLineExtensionTest, MultipleLinesExtendIndependently) {
  Circuit circuit = BuildCircuitWithDelayLines();
  std::string error;
  std::vector<DelayLineExtensionPlan> plans =
      Preflight(circuit, {{"dl0", 1}, {"dl1", 2}}, &error);
  ASSERT_EQ(plans.size(), 2u) << error;
  ASSERT_TRUE(ApplyDelayLineExtensions(circuit, plans, &error)) << error;

  DelayLineChain dl0;
  DelayLineChain dl1;
  ASSERT_TRUE(BuildDelayLineChain(circuit, "dl0", &dl0, &error)) << error;
  ASSERT_TRUE(BuildDelayLineChain(circuit, "dl1", &dl1, &error)) << error;
  EXPECT_EQ(dl0.nodes.size(), 5u);
  EXPECT_EQ(dl1.nodes.size(), 5u);
}

} // namespace
} // namespace dali
