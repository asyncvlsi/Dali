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
 * Validating and applying a value-only netlist change.
 *
 * Application must not be able to fail partway: components are addressed by
 * index and nets hold pointers into the component vector, so a delta abandoned
 * halfway leaves a circuit no caller can repair. Everything rejectable is
 * therefore rejected first, and these tests are mostly about what counts as
 * rejectable -- each case additionally asserts the circuit is untouched, since
 * a validator that rejected after mutating would be worse than none.
 */
#include "dali/circuit/topology_delta.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include "dali/circuit/circuit.h"

namespace dali {
namespace {

Circuit BuildCircuit() {
  Circuit circuit;
  circuit.SetDatabaseMicrons(1000);
  circuit.SetManufacturingGrid(0.001);
  circuit.AddMetalLayer("m1", 0.1, 0.1, 0.042, 0.2, 0.2, VERTICAL);
  circuit.AddMetalLayer("m2", 0.1, 0.1, 0.042, 0.2, 0.2, HORIZONTAL);
  circuit.SetGridValue(0.2, 0.2);
  circuit.SetRowHeight(0.2);
  Macro *inv = circuit.AddMacro("INV", 0.8, 1.6);
  circuit.AddMacroPin(inv, "A", true)->SetOffset(0.1, 0.8);
  circuit.AddMacroPin(inv, "Y", false)->SetOffset(0.7, 0.8);
  circuit.SetUnitsDistanceMicrons(1000);
  circuit.SetDieArea(0, 0, 10000, 10000);
  circuit.ReserveSpaceForDesignImp(4, 0, 4);
  circuit.AddComponent("u0", "INV", 1.0, 2.0, PLACED, N, true);
  circuit.AddComponent("u1", "INV", 3.0, 4.0, PLACED, N, true);
  circuit.AddNet("n0", 2);
  circuit.AddComponentPinToNet("u0", "Y", "n0");
  circuit.AddComponentPinToNet("u1", "A", "n0");
  return circuit;
}

/** A delta shaped like ACT elaboration output: named cells, named nets. */
TopologyDelta GoodDelta() {
  TopologyDelta delta;
  delta.added_components.push_back({"dl0_ainv_514_6", "INV", 5.0, 6.0});
  delta.added_nets.push_back(
      {"dl0_extnet_0", {{"u1", "Y"}, {"dl0_ainv_514_6", "A"}}});
  return delta;
}

TEST(TopologyDeltaTest, ValidDeltaIsAcceptedAndAppliesExactly) {
  Circuit circuit = BuildCircuit();
  const size_t components = circuit.Components().size();
  const size_t nets = circuit.Nets().size();
  const int u0_id = circuit.GetComponentPtr("u0")->Id();
  const double u0_x = circuit.GetComponentPtr("u0")->LLX();

  std::string error;
  ASSERT_TRUE(ValidateTopologyDelta(circuit, GoodDelta(), &error)) << error;
  ApplyTopologyDelta(circuit, GoodDelta());

  EXPECT_EQ(circuit.Components().size(), components + 1);
  EXPECT_EQ(circuit.Nets().size(), nets + 1);
  EXPECT_EQ(circuit.GetComponentPtr("u0")->Id(), u0_id);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("u0")->LLX(), u0_x);
  EXPECT_EQ(circuit.GetNetPtr("dl0_extnet_0")->PinCnt(), 2u);
  // The ACT-chosen name is used verbatim; Dali invents nothing.
  EXPECT_TRUE(circuit.IsComponentExisting("dl0_ainv_514_6"));
}

TEST(TopologyDeltaTest, ValidationDoesNotChangeTheCircuit) {
  Circuit circuit = BuildCircuit();
  const size_t components = circuit.Components().size();
  const size_t nets = circuit.Nets().size();
  std::string error;
  ASSERT_TRUE(ValidateTopologyDelta(circuit, GoodDelta(), &error));
  EXPECT_EQ(circuit.Components().size(), components);
  EXPECT_EQ(circuit.Nets().size(), nets);
}

TEST(TopologyDeltaTest, UnknownMasterIsRejected) {
  Circuit circuit = BuildCircuit();
  TopologyDelta delta;
  delta.added_components.push_back({"x", "NO_SUCH_MACRO", 0, 0});
  std::string error;
  EXPECT_FALSE(ValidateTopologyDelta(circuit, delta, &error));
  EXPECT_NE(error.find("unknown master"), std::string::npos) << error;
  EXPECT_EQ(circuit.Components().size(), 2u);
}

TEST(TopologyDeltaTest, DuplicateComponentNameIsRejected) {
  Circuit circuit = BuildCircuit();
  TopologyDelta delta;
  delta.added_components.push_back({"u0", "INV", 0, 0});
  std::string error;
  EXPECT_FALSE(ValidateTopologyDelta(circuit, delta, &error));
  EXPECT_NE(error.find("already taken"), std::string::npos) << error;
}

// Two additions in one delta may not claim the same name either; they are
// applied without a lookup in between.
TEST(TopologyDeltaTest, NameCollisionWithinTheDeltaIsRejected) {
  Circuit circuit = BuildCircuit();
  TopologyDelta delta;
  delta.added_components.push_back({"same", "INV", 0, 0});
  delta.added_components.push_back({"same", "INV", 0, 0});
  std::string error;
  EXPECT_FALSE(ValidateTopologyDelta(circuit, delta, &error));
}

TEST(TopologyDeltaTest, NetNamingAnUnknownComponentIsRejected) {
  Circuit circuit = BuildCircuit();
  TopologyDelta delta;
  delta.added_nets.push_back({"n_new", {{"ghost", "Y"}}});
  std::string error;
  EXPECT_FALSE(ValidateTopologyDelta(circuit, delta, &error));
  EXPECT_NE(error.find("unknown component"), std::string::npos) << error;
}

TEST(TopologyDeltaTest, NetNamingAPinTheMasterLacksIsRejected) {
  Circuit circuit = BuildCircuit();
  TopologyDelta delta;
  delta.added_nets.push_back({"n_new", {{"u0", "NOT_A_PIN"}}});
  std::string error;
  EXPECT_FALSE(ValidateTopologyDelta(circuit, delta, &error));
  EXPECT_NE(error.find("has no pin"), std::string::npos) << error;
}

// A net may reference a component the same delta introduces; that is the whole
// shape of a chain extension.
TEST(TopologyDeltaTest, NetMayReferenceAComponentAddedByTheSameDelta) {
  Circuit circuit = BuildCircuit();
  std::string error;
  EXPECT_TRUE(ValidateTopologyDelta(circuit, GoodDelta(), &error)) << error;
}

TEST(TopologyDeltaTest, RetiringAnUnknownNetIsRejected) {
  Circuit circuit = BuildCircuit();
  TopologyDelta delta;
  delta.retired_nets.push_back("no_such_net");
  std::string error;
  EXPECT_FALSE(ValidateTopologyDelta(circuit, delta, &error));
}

TEST(TopologyDeltaTest, RetiringDisconnectsButKeepsTheNetSlot) {
  Circuit circuit = BuildCircuit();
  const size_t nets = circuit.Nets().size();
  const int n0_id = circuit.GetNetPtr("n0")->Id();
  TopologyDelta delta;
  delta.retired_nets.push_back("n0");
  std::string error;
  ASSERT_TRUE(ValidateTopologyDelta(circuit, delta, &error)) << error;
  ApplyTopologyDelta(circuit, delta);
  EXPECT_EQ(circuit.Nets().size(), nets)
      << "net ids are indices; the slot stays";
  EXPECT_EQ(circuit.GetNetPtr("n0")->Id(), n0_id);
  EXPECT_EQ(circuit.GetNetPtr("n0")->PinCnt(), 0u);
}

// Capacity is a fixed budget: growing either vector would dangle every pointer
// a net holds and invalidate every id the placer carries.
TEST(TopologyDeltaTest, ExceedingComponentCapacityIsRejected) {
  Circuit circuit = BuildCircuit();
  const size_t headroom =
      circuit.Components().capacity() - circuit.Components().size();
  TopologyDelta delta;
  for (size_t i = 0; i <= headroom; ++i) {
    delta.added_components.push_back({"f" + std::to_string(i), "INV", 0, 0});
  }
  std::string error;
  EXPECT_FALSE(ValidateTopologyDelta(circuit, delta, &error));
  EXPECT_NE(error.find("reserved"), std::string::npos) << error;
  EXPECT_EQ(circuit.Components().size(), 2u) << "rejected after mutating";
}

// Raising a delay site rewires the net it drives: the name belongs to the
// enclosing design and stays, but a different cell now drives it. Measured on
// bd_pipeline as net `c1` moving from dl0_ainv_513_6 to dl0_ainv_523_6.
TEST(TopologyDeltaTest, RewiringReplacesMembershipAndKeepsIdentity) {
  Circuit circuit = BuildCircuit();
  const int n0_id = circuit.GetNetPtr("n0")->Id();
  const int u0_id = circuit.GetComponentPtr("u0")->Id();
  const double u0_x = circuit.GetComponentPtr("u0")->LLX();

  TopologyDelta delta;
  delta.added_components.push_back({"inserted", "INV", 5.0, 6.0});
  delta.rewired_nets.push_back({"n0", {{"inserted", "Y"}, {"u1", "A"}}});

  std::string error;
  ASSERT_TRUE(ValidateTopologyDelta(circuit, delta, &error)) << error;
  ApplyTopologyDelta(circuit, delta);

  Net *net = circuit.GetNetPtr("n0");
  EXPECT_EQ(net->Id(), n0_id) << "a rewire must not renumber the net";
  EXPECT_EQ(net->PinCnt(), 2u);
  bool driven_by_new = false;
  bool still_reaches_u1 = false;
  for (NetPin &pin : net->ComponentPins()) {
    if (pin.ComponentPtr()->Name() == "inserted")
      driven_by_new = true;
    if (pin.ComponentPtr()->Name() == "u1")
      still_reaches_u1 = true;
    EXPECT_NE(pin.ComponentPtr()->Name(), "u0") << "old driver still attached";
  }
  EXPECT_TRUE(driven_by_new);
  EXPECT_TRUE(still_reaches_u1);
  // Everything the rewire did not name is untouched.
  EXPECT_EQ(circuit.GetComponentPtr("u0")->Id(), u0_id);
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("u0")->LLX(), u0_x);
}

TEST(TopologyDeltaTest, RewiringAnUnknownNetIsRejected) {
  Circuit circuit = BuildCircuit();
  TopologyDelta delta;
  delta.rewired_nets.push_back({"no_such_net", {{"u0", "Y"}}});
  std::string error;
  EXPECT_FALSE(ValidateTopologyDelta(circuit, delta, &error));
  EXPECT_NE(error.find("unknown net"), std::string::npos) << error;
}

TEST(TopologyDeltaTest, RewiringToAnUnknownPinIsRejected) {
  Circuit circuit = BuildCircuit();
  TopologyDelta delta;
  delta.rewired_nets.push_back({"n0", {{"u0", "NOT_A_PIN"}}});
  std::string error;
  EXPECT_FALSE(ValidateTopologyDelta(circuit, delta, &error));
  EXPECT_EQ(circuit.GetNetPtr("n0")->PinCnt(), 2u) << "rejected after mutating";
}

// A net's pin capacity is fixed when it is created, so a rewire that needs more
// endpoints than the net was built for is refused rather than half-applied.
TEST(TopologyDeltaTest, RewiringBeyondTheNetsCapacityIsRejected) {
  Circuit circuit = BuildCircuit();
  TopologyDelta delta;
  delta.added_components.push_back({"extra", "INV", 0, 0});
  delta.rewired_nets.push_back(
      {"n0", {{"u0", "Y"}, {"u1", "A"}, {"extra", "A"}}});
  std::string error;
  EXPECT_FALSE(ValidateTopologyDelta(circuit, delta, &error));
  EXPECT_NE(error.find("room for"), std::string::npos) << error;
  EXPECT_EQ(circuit.Components().size(), 2u) << "rejected after mutating";
}

// A delta that both adds and rewires must not be applied in pieces: if any part
// is invalid, none of it happens.
TEST(TopologyDeltaTest, AnInvalidRewireBlocksTheWholeDelta) {
  Circuit circuit = BuildCircuit();
  const size_t components = circuit.Components().size();
  TopologyDelta delta;
  delta.added_components.push_back({"good", "INV", 0, 0});
  delta.added_nets.push_back({"good_net", {{"good", "Y"}, {"u1", "A"}}});
  delta.rewired_nets.push_back({"no_such_net", {{"good", "Y"}}});
  std::string error;
  EXPECT_FALSE(ValidateTopologyDelta(circuit, delta, &error));
  EXPECT_EQ(circuit.Components().size(), components);
  EXPECT_FALSE(circuit.IsComponentExisting("good"));
}

TEST(TopologyDeltaTest, EmptyDeltaIsValidAndDoesNothing) {
  Circuit circuit = BuildCircuit();
  TopologyDelta delta;
  EXPECT_TRUE(delta.IsEmpty());
  std::string error;
  EXPECT_TRUE(ValidateTopologyDelta(circuit, delta, &error));
  ApplyTopologyDelta(circuit, delta);
  EXPECT_EQ(circuit.Components().size(), 2u);
}

// Added cells are created unplaced, because at that instant that is true: they
// carry their line's centroid as a seed and no legal site. Promotion belongs
// after legalization, and a fix that marked them placed here would export cells
// claiming positions nothing had assigned them whenever legalization failed.
TEST(TopologyDeltaTest, AddedComponentsAreCreatedUnplacedAtTheirSeed) {
  Circuit circuit = BuildCircuit();
  std::string error;
  TopologyDelta delta = GoodDelta();
  ASSERT_TRUE(ValidateTopologyDelta(circuit, delta, &error)) << error;
  ApplyTopologyDelta(circuit, delta);

  Component *added = circuit.GetComponentPtr("dl0_ainv_514_6");
  ASSERT_NE(added, nullptr);
  EXPECT_EQ(added->Status(), UNPLACED)
      << "a cell that has only been seeded must not claim to be placed";
  EXPECT_DOUBLE_EQ(added->LLX(), 5.0);
  EXPECT_DOUBLE_EQ(added->LLY(), 6.0);

  // And the status is writable, which is what the promotion after legalization
  // relies on.
  added->SetPlacementStatus(PLACED);
  EXPECT_EQ(added->Status(), PLACED);
}

TEST(TopologyDeltaTest, LegalizedAdditionMustNotOverlapAnExistingComponent) {
  Circuit circuit = BuildCircuit();
  std::string error;
  TopologyDelta delta = GoodDelta();
  ASSERT_TRUE(ValidateTopologyDelta(circuit, delta, &error)) << error;
  ApplyTopologyDelta(circuit, delta);

  Component *added = circuit.GetComponentPtr("dl0_ainv_514_6");
  ASSERT_NE(added, nullptr);
  added->SetLLX(circuit.GetComponentPtr("u0")->LLX());
  added->SetLLY(circuit.GetComponentPtr("u0")->LLY());

  EXPECT_FALSE(
      ValidateTopologyAddedPlacement(circuit, {"dl0_ainv_514_6"}, &error));
  EXPECT_NE(error.find("overlaps component 'u0'"), std::string::npos) << error;
  EXPECT_EQ(added->Status(), UNPLACED);
}

TEST(TopologyDeltaTest, LegalizedAdditionMustStayInsideTheRegion) {
  Circuit circuit = BuildCircuit();
  std::string error;
  TopologyDelta delta = GoodDelta();
  ASSERT_TRUE(ValidateTopologyDelta(circuit, delta, &error)) << error;
  ApplyTopologyDelta(circuit, delta);

  Component *added = circuit.GetComponentPtr("dl0_ainv_514_6");
  ASSERT_NE(added, nullptr);
  added->SetLLX(circuit.RegionURX());

  EXPECT_FALSE(
      ValidateTopologyAddedPlacement(circuit, {"dl0_ainv_514_6"}, &error));
  EXPECT_NE(error.find("outside the placement region"), std::string::npos)
      << error;
}

TEST(TopologyDeltaTest, NonoverlappingLegalizedAdditionMayBePromoted) {
  Circuit circuit = BuildCircuit();
  std::string error;
  TopologyDelta delta = GoodDelta();
  ASSERT_TRUE(ValidateTopologyDelta(circuit, delta, &error)) << error;
  ApplyTopologyDelta(circuit, delta);

  Component *added = circuit.GetComponentPtr("dl0_ainv_514_6");
  ASSERT_NE(added, nullptr);
  added->SetLLX(20.0);
  added->SetLLY(20.0);

  EXPECT_TRUE(
      ValidateTopologyAddedPlacement(circuit, {"dl0_ainv_514_6"}, &error))
      << error;
  EXPECT_EQ(added->Status(), UNPLACED)
      << "validation must not promote before its caller commits";
}

} // namespace
} // namespace dali
