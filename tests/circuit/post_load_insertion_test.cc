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
 * Adding components to a design that is already loaded, netlisted, and placed.
 *
 * This is what lets a delay line grow between placement rounds without tearing
 * the design down and rebuilding it, which in turn is what lets placement carry
 * forward from the previous round instead of restarting from a fresh, and
 * differently-noisy, solve.
 *
 * The invariant these tests guard is narrow and load-bearing: nets hold
 * pointers into the component vector and the placer holds component ids, so an
 * insertion is safe exactly when the vector does not reallocate. Reserved
 * capacity is what guarantees that, so the tests check both that an append
 * inside capacity disturbs nothing and that exhausting capacity is refused
 * rather than silently reallocating.
 */
#include <gtest/gtest.h>

#include <cstddef>

#include <algorithm>

#include "dali/circuit/circuit.h"

namespace dali {
namespace {

/** A two-inverter chain with nets already built, as if just loaded. */
Circuit BuildLoadedCircuit() {
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
  circuit.ReserveSpaceForDesignImp(2, 0, 1);
  circuit.AddComponent("inv1", "INV2", 1.0, 2.0, PLACED, N, true);
  circuit.AddComponent("inv2", "INV2", 3.0, 4.0, PLACED, N, true);
  circuit.AddNet("chain", 2);
  circuit.AddComponentPinToNet("inv1", "OUT", "chain");
  circuit.AddComponentPinToNet("inv2", "IN", "chain");
  return circuit;
}

TEST(PostLoadInsertionTest, ComponentCanBeAddedWhileNetsExist) {
  Circuit circuit = BuildLoadedCircuit();
  ASSERT_FALSE(circuit.Nets().empty());
  circuit.AddComponent("inv3", "INV2", 5.0, 6.0, PLACED, N, true);
  EXPECT_EQ(circuit.Components().size(), 3u);
  EXPECT_TRUE(circuit.IsComponentExisting("inv3"));
}

// The whole point of carrying placement forward: nothing that existed before
// the insertion may move, and no id may be renumbered.
TEST(PostLoadInsertionTest, InsertionDisturbsNeitherPositionsNorIds) {
  Circuit circuit = BuildLoadedCircuit();
  const int id1 = circuit.GetComponentPtr("inv1")->Id();
  const int id2 = circuit.GetComponentPtr("inv2")->Id();

  circuit.AddComponent("inv3", "INV2", 5.0, 6.0, PLACED, N, true);

  EXPECT_EQ(circuit.GetComponentPtr("inv1")->Id(), id1);
  EXPECT_EQ(circuit.GetComponentPtr("inv2")->Id(), id2);
  EXPECT_DOUBLE_EQ(circuit.Components()[id1].LLX(), 1.0);
  EXPECT_DOUBLE_EQ(circuit.Components()[id1].LLY(), 2.0);
  EXPECT_DOUBLE_EQ(circuit.Components()[id2].LLX(), 3.0);
  EXPECT_DOUBLE_EQ(circuit.Components()[id2].LLY(), 4.0);
}

// Nets hold pointers into the component vector. A reallocation would dangle
// them, so an insertion must leave the existing net's pins resolving to the
// same components they did before.
TEST(PostLoadInsertionTest, ExistingNetPinsSurviveTheInsertion) {
  Circuit circuit = BuildLoadedCircuit();
  Net *chain = circuit.GetNetPtr("chain");
  ASSERT_NE(chain, nullptr);
  const size_t pin_count = chain->PinCnt();
  const Component *first_before = chain->ComponentPins()[0].ComponentPtr();

  circuit.AddComponent("inv3", "INV2", 5.0, 6.0, PLACED, N, true);

  Net *chain_after = circuit.GetNetPtr("chain");
  ASSERT_NE(chain_after, nullptr);
  EXPECT_EQ(chain_after->PinCnt(), pin_count);
  EXPECT_EQ(chain_after->ComponentPins()[0].ComponentPtr(), first_before);
  EXPECT_EQ(chain_after->ComponentPins()[0].ComponentPtr()->Name(), "inv1");
}

// A net's pin capacity is fixed when the net is created, so an existing net
// cannot absorb the new cell. Splicing must therefore create fresh nets rather
// than extend the one it interrupts -- which is the same conclusion the
// retire-rather-than-delete approach reaches from the other direction.
TEST(PostLoadInsertionTest, ExistingNetCannotAbsorbAnotherPin) {
  Circuit circuit = BuildLoadedCircuit();
  circuit.AddComponent("inv3", "INV2", 5.0, 6.0, PLACED, N, true);
  // DaliExpects reports through its own stream, which the death test does not
  // capture, so this asserts the refusal rather than its wording.
  EXPECT_DEATH(circuit.AddComponentPinToNet("inv3", "IN", "chain"), "");
}

// The pattern extension actually uses: add the cell, then give it a new net of
// its own. Net capacity carries the same insertion headroom as components.
TEST(PostLoadInsertionTest, InsertedComponentGetsItsOwnNewNet) {
  Circuit circuit = BuildLoadedCircuit();
  const size_t nets_before = circuit.Nets().size();
  circuit.AddComponent("inv3", "INV2", 5.0, 6.0, PLACED, N, true);
  circuit.AddNet("chain_extension", 2);
  circuit.AddComponentPinToNet("inv3", "OUT", "chain_extension");
  circuit.AddComponentPinToNet("inv2", "IN", "chain_extension");
  EXPECT_EQ(circuit.Nets().size(), nets_before + 1);
  EXPECT_EQ(circuit.GetNetPtr("chain_extension")->PinCnt(), 2u);
  // The pre-existing net is untouched and still resolves to its own pins.
  EXPECT_EQ(circuit.GetNetPtr("chain")->PinCnt(), 2u);
}

// Refusing is the correct behaviour at the capacity limit: growing the vector
// would reallocate and dangle every pointer a net holds.
TEST(PostLoadInsertionTest, ExhaustingReservedCapacityIsRefused) {
  Circuit circuit = BuildLoadedCircuit();
  const size_t headroom =
      circuit.Components().capacity() - circuit.Components().size();
  for (size_t i = 0; i < headroom; ++i) {
    circuit.AddComponent("filler" + std::to_string(i), "INV2", 0, 0, PLACED, N,
                         true);
  }
  ASSERT_EQ(circuit.Components().size(), circuit.Components().capacity());
  EXPECT_DEATH(circuit.AddComponent("overflow", "INV2", 0, 0, PLACED, N, true),
               "");
}

// Retiring is how a spliced-out connection stops connecting. The net keeps its
// slot -- indices elsewhere stay valid -- and empties.
TEST(PostLoadInsertionTest, RetiringANetDisconnectsItSymmetrically) {
  Circuit circuit = BuildLoadedCircuit();
  const size_t nets_before = circuit.Nets().size();
  Net *chain = circuit.GetNetPtr("chain");
  const int chain_id = chain->Id();
  Component *inv1 = circuit.GetComponentPtr("inv1");
  ASSERT_NE(std::find(inv1->NetList().begin(), inv1->NetList().end(), chain_id),
            inv1->NetList().end());

  chain->Retire();

  EXPECT_EQ(chain->PinCnt(), 0u);
  EXPECT_EQ(circuit.Nets().size(), nets_before) << "the slot must remain";
  EXPECT_EQ(circuit.GetNetPtr("chain")->Id(), chain_id);
  // Symmetric: the components no longer claim membership either.
  EXPECT_EQ(std::find(inv1->NetList().begin(), inv1->NetList().end(), chain_id),
            inv1->NetList().end());
  Component *inv2 = circuit.GetComponentPtr("inv2");
  EXPECT_EQ(std::find(inv2->NetList().begin(), inv2->NetList().end(), chain_id),
            inv2->NetList().end());
}

// A retired net falls below the quadratic builder's own pin-count floor, so it
// leaves the placement problem without any consumer needing to know about it.
TEST(PostLoadInsertionTest, RetiredNetIsBelowThePlacementPinFloor) {
  Circuit circuit = BuildLoadedCircuit();
  circuit.GetNetPtr("chain")->Retire();
  EXPECT_LE(circuit.GetNetPtr("chain")->PinCnt(), 1u);
}

// The full splice: interrupt inv1->inv2, insert inv3 between them.
TEST(PostLoadInsertionTest, ChainCanBeSplicedInPlace) {
  Circuit circuit = BuildLoadedCircuit();
  Component *inv2 = circuit.GetComponentPtr("inv2");
  const double inv2_x = inv2->LLX();

  circuit.GetNetPtr("chain")->Retire();
  circuit.AddComponent("inv3", "INV2", 5.0, 6.0, PLACED, N, true);
  circuit.AddNet("chain_a", 2);
  circuit.AddComponentPinToNet("inv1", "OUT", "chain_a");
  circuit.AddComponentPinToNet("inv3", "IN", "chain_a");
  circuit.AddNet("chain_b", 2);
  circuit.AddComponentPinToNet("inv3", "OUT", "chain_b");
  circuit.AddComponentPinToNet("inv2", "IN", "chain_b");

  EXPECT_EQ(circuit.GetNetPtr("chain")->PinCnt(), 0u);
  EXPECT_EQ(circuit.GetNetPtr("chain_a")->PinCnt(), 2u);
  EXPECT_EQ(circuit.GetNetPtr("chain_b")->PinCnt(), 2u);
  // Placement carried forward untouched, which is the reason for all of this.
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("inv2")->LLX(), inv2_x);
}

} // namespace
} // namespace dali
