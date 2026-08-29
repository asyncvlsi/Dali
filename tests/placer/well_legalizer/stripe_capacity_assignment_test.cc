/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/stripe_capacity_assignment.h"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

namespace dali {

namespace {

StripeSlot Slot(int index, int llx, int lly, int urx, int ury) {
  StripeSlot slot;
  slot.index = index;
  slot.llx = llx;
  slot.lly = lly;
  slot.urx = urx;
  slot.ury = ury;
  return slot;
}

StripeDemandItem Item(int index, int x, int y, int preferred) {
  StripeDemandItem item;
  item.index = index;
  item.x = x;
  item.y = y;
  item.preferred_slot = preferred;
  return item;
}

/**
 * A stand-in for the gridded estimator: each fragment holds a fixed budget and
 * each item costs a fixed amount.
 *
 * Real capacity is row-height packing, which is exactly why production asks the
 * estimator rather than storing a number -- an area budget was tried and said
 * yes to fourteen cells that needed two shelves in a fragment one shelf tall.
 * A linear budget is still enough to exercise every ordering, eviction and
 * refusal rule in the planner, which is all this file is about.
 */
class BudgetOracle {
 public:
  BudgetOracle(std::vector<unsigned long long> capacities,
               std::vector<unsigned long long> demands)
      : capacities_(std::move(capacities)), demands_(std::move(demands)) {}

  StripeFitsPredicate Predicate() {
    return [this](int slot_index, const std::vector<int> &item_indices) {
      unsigned long long load = 0;
      for (int item_index : item_indices) load += demands_[item_index];
      return load <= capacities_[slot_index];
    };
  }

  unsigned long long Load(const StripeAssignmentPlan &plan, int slot) const {
    unsigned long long load = 0;
    for (std::size_t index = 0; index < plan.slot_of_item.size(); ++index) {
      if (plan.slot_of_item[index] == slot) load += demands_[index];
    }
    return load;
  }
  unsigned long long Capacity(int slot) const { return capacities_[slot]; }
  unsigned long long TotalDemand() const {
    unsigned long long total = 0;
    for (unsigned long long demand : demands_) total += demand;
    return total;
  }

 private:
  std::vector<unsigned long long> capacities_;
  std::vector<unsigned long long> demands_;
};

/** Uniform budget helper: every fragment the same size, every item the same. */
BudgetOracle Uniform(std::size_t slots, unsigned long long capacity,
                     std::size_t items, unsigned long long demand) {
  return BudgetOracle(std::vector<unsigned long long>(slots, capacity),
                      std::vector<unsigned long long>(items, demand));
}

}  // namespace

TEST(StripeCapacityAssignmentTest, AFeasibleProximityPlanIsLeftAlone) {
  const std::vector<StripeSlot> slots = {Slot(0, 0, 0, 100, 10),
                                         Slot(1, 0, 20, 100, 30)};
  const std::vector<StripeDemandItem> items = {
      Item(0, 10, 5, 0), Item(1, 20, 5, 0), Item(2, 10, 25, 1)};
  BudgetOracle oracle = Uniform(2, 100, 3, 40);

  const StripeAssignmentPlan plan =
      PlanCapacityAwareStripeAssignment(slots, items, oracle.Predicate());

  ASSERT_TRUE(plan.feasible) << plan.refusal;
  EXPECT_EQ(plan.moved_count, 0) << "nothing was overloaded, so nothing moves";
  EXPECT_EQ(plan.overloaded_slots_before, 0);
  EXPECT_EQ(plan.overloaded_slots_after, 0);
  for (std::size_t index = 0; index < items.size(); ++index) {
    EXPECT_EQ(plan.slot_of_item[index], items[index].preferred_slot);
  }
}

TEST(StripeCapacityAssignmentTest, OverflowMovesToTheNearestFragmentWithRoom) {
  const std::vector<StripeSlot> slots = {Slot(0, 0, 0, 100, 10),
                                         Slot(1, 0, 20, 100, 30),
                                         Slot(2, 0, 200, 100, 210)};
  const std::vector<StripeDemandItem> items = {
      Item(0, 10, 5, 0), Item(1, 20, 5, 0), Item(2, 30, 5, 0), Item(3, 40, 5, 0)};
  BudgetOracle oracle = Uniform(3, 30, 4, 10);

  const StripeAssignmentPlan plan =
      PlanCapacityAwareStripeAssignment(slots, items, oracle.Predicate());

  ASSERT_TRUE(plan.feasible) << plan.refusal;
  EXPECT_EQ(plan.overloaded_slots_before, 1);
  EXPECT_EQ(plan.overloaded_slots_after, 0)
      << "an accepted plan must not leave a fragment over budget";
  EXPECT_EQ(plan.moved_count, 1) << "only the overflow moves";
  EXPECT_EQ(plan.slot_of_item[3], 1) << "and to the nearer of the two options";
}

TEST(StripeCapacityAssignmentTest, ConservesDemandAndRespectsEveryCapacity) {
  const std::vector<StripeSlot> slots = {
      Slot(0, 0, 0, 100, 10), Slot(1, 0, 20, 100, 30), Slot(2, 0, 40, 100, 50)};
  std::vector<StripeDemandItem> items;
  for (int index = 0; index < 12; ++index) {
    items.push_back(Item(index, 10 * index, 5, 0));
  }
  BudgetOracle oracle = Uniform(3, 50, 12, 10);

  const StripeAssignmentPlan plan =
      PlanCapacityAwareStripeAssignment(slots, items, oracle.Predicate());

  ASSERT_TRUE(plan.feasible) << plan.refusal;
  unsigned long long placed = 0;
  for (const StripeSlot &slot : slots) {
    const unsigned long long load = oracle.Load(plan, slot.index);
    EXPECT_LE(load, oracle.Capacity(slot.index))
        << "slot " << slot.index << " is over budget";
    placed += load;
  }
  EXPECT_EQ(placed, oracle.TotalDemand())
      << "every component is placed exactly once";
  for (int slot : plan.slot_of_item) EXPECT_GE(slot, 0) << "nothing was dropped";
}

TEST(StripeCapacityAssignmentTest, SmallestFeasibleDisplacementWins) {
  const std::vector<StripeSlot> slots = {Slot(0, 0, 0, 10, 10),
                                         Slot(1, 0, 300, 10, 310),
                                         Slot(2, 0, 40, 10, 50),
                                         Slot(3, 0, 100, 10, 110)};
  const std::vector<StripeDemandItem> items = {Item(0, 5, 5, 0)};
  BudgetOracle oracle({0, 10, 10, 10}, {10});

  const StripeAssignmentPlan plan =
      PlanCapacityAwareStripeAssignment(slots, items, oracle.Predicate());

  ASSERT_TRUE(plan.feasible) << plan.refusal;
  EXPECT_EQ(plan.slot_of_item[0], 2)
      << "slot 2 is nearest among those with room, though slot 1 comes first";
}

TEST(StripeCapacityAssignmentTest, EqualDistanceResolvesToTheLowerSlotIndex) {
  const std::vector<StripeSlot> slots = {Slot(0, 0, 100, 10, 110),
                                         Slot(1, 0, 80, 10, 90),
                                         Slot(2, 0, 120, 10, 130)};
  const std::vector<StripeDemandItem> items = {Item(0, 5, 105, 0)};
  BudgetOracle oracle({0, 10, 10}, {10});

  const StripeAssignmentPlan plan =
      PlanCapacityAwareStripeAssignment(slots, items, oracle.Predicate());

  ASSERT_TRUE(plan.feasible) << plan.refusal;
  EXPECT_EQ(StripeSlotDistance(slots[1], 5, 105),
            StripeSlotDistance(slots[2], 5, 105))
      << "the tie has to be a real tie or the test proves nothing";
  EXPECT_EQ(plan.slot_of_item[0], 1);
}

TEST(StripeCapacityAssignmentTest, ItemOrderDoesNotChangeTheResult) {
  const std::vector<StripeSlot> slots = {Slot(0, 0, 0, 100, 10),
                                         Slot(1, 0, 20, 100, 30)};
  const std::vector<StripeDemandItem> forward = {
      Item(0, 10, 5, 0), Item(1, 20, 5, 0), Item(2, 30, 5, 0)};
  const std::vector<StripeDemandItem> reversed(forward.rbegin(), forward.rend());
  BudgetOracle a_oracle = Uniform(2, 20, 3, 10);
  BudgetOracle b_oracle = Uniform(2, 20, 3, 10);

  const StripeAssignmentPlan a =
      PlanCapacityAwareStripeAssignment(slots, forward, a_oracle.Predicate());
  const StripeAssignmentPlan b =
      PlanCapacityAwareStripeAssignment(slots, reversed, b_oracle.Predicate());

  ASSERT_TRUE(a.feasible) << a.refusal;
  ASSERT_TRUE(b.feasible) << b.refusal;
  for (std::size_t index = 0; index < forward.size(); ++index) {
    EXPECT_EQ(a.slot_of_item[index],
              b.slot_of_item[reversed.size() - 1 - index])
        << "component " << forward[index].index << " landed differently";
  }
}

TEST(StripeCapacityAssignmentTest, NoRoomAnywhereRefusesExplicitly) {
  const std::vector<StripeSlot> slots = {Slot(0, 0, 0, 100, 10),
                                         Slot(1, 0, 20, 100, 30)};
  const std::vector<StripeDemandItem> items = {
      Item(0, 10, 5, 0), Item(1, 20, 5, 1), Item(2, 30, 5, 0)};
  BudgetOracle oracle = Uniform(2, 10, 3, 10);

  const StripeAssignmentPlan plan =
      PlanCapacityAwareStripeAssignment(slots, items, oracle.Predicate());

  EXPECT_FALSE(plan.feasible);
  EXPECT_NE(plan.refusal.find("no whitespace fragment"), std::string::npos)
      << plan.refusal;
}

TEST(StripeCapacityAssignmentTest, NoFragmentsAtAllRefuses) {
  BudgetOracle oracle({}, {10});
  const StripeAssignmentPlan plan = PlanCapacityAwareStripeAssignment(
      {}, {Item(0, 0, 0, -1)}, oracle.Predicate());

  EXPECT_FALSE(plan.feasible);
  EXPECT_NE(plan.refusal.find("no whitespace fragment"), std::string::npos);
}

TEST(StripeCapacityAssignmentTest, AZeroCapacityFragmentIsNeverUsed) {
  // A fragment a fixed obstacle left with no usable rows: nearest, and unusable.
  const std::vector<StripeSlot> slots = {Slot(0, 0, 0, 100, 10),
                                         Slot(1, 0, 50, 100, 60)};
  const std::vector<StripeDemandItem> items = {Item(0, 10, 5, 0)};
  BudgetOracle oracle({0, 20}, {10});

  const StripeAssignmentPlan plan =
      PlanCapacityAwareStripeAssignment(slots, items, oracle.Predicate());

  ASSERT_TRUE(plan.feasible) << plan.refusal;
  EXPECT_EQ(plan.slot_of_item[0], 1);
  EXPECT_EQ(oracle.Load(plan, 0), 0ULL);
}

TEST(StripeCapacityAssignmentTest, OverflowStrictlyDecreases) {
  const std::vector<StripeSlot> slots = {
      Slot(0, 0, 0, 100, 10), Slot(1, 0, 20, 100, 30), Slot(2, 0, 40, 100, 50),
      Slot(3, 0, 60, 100, 70)};
  std::vector<StripeDemandItem> items;
  for (int index = 0; index < 6; ++index) {
    items.push_back(Item(index, 10 * index, 5, 0));
  }
  BudgetOracle oracle = Uniform(4, 20, 6, 10);

  const StripeAssignmentPlan plan =
      PlanCapacityAwareStripeAssignment(slots, items, oracle.Predicate());

  ASSERT_TRUE(plan.feasible) << plan.refusal;
  EXPECT_GT(plan.overloaded_slots_before, 0);
  EXPECT_LT(plan.overloaded_slots_after, plan.overloaded_slots_before);
  EXPECT_EQ(plan.overloaded_slots_after, 0);
}

TEST(StripeCapacityAssignmentTest, TheFurthestOccupantIsTheOneEvicted) {
  // Two components own a fragment that holds one. The one sitting inside it
  // stays; the one reaching in from far away is the one that leaves.
  const std::vector<StripeSlot> slots = {Slot(0, 0, 0, 100, 10),
                                         Slot(1, 0, 20, 100, 30)};
  const std::vector<StripeDemandItem> items = {Item(0, 500, 5, 0),
                                               Item(1, 50, 5, 0)};
  BudgetOracle oracle = Uniform(2, 10, 2, 10);

  const StripeAssignmentPlan plan =
      PlanCapacityAwareStripeAssignment(slots, items, oracle.Predicate());

  ASSERT_TRUE(plan.feasible) << plan.refusal;
  EXPECT_EQ(plan.slot_of_item[1], 0) << "the local occupant keeps its fragment";
  EXPECT_EQ(plan.slot_of_item[0], 1) << "the distant one is displaced";
}

TEST(StripeCapacityAssignmentTest, AnUnknownPreferredSlotIsPlacedNotDropped) {
  const std::vector<StripeSlot> slots = {Slot(0, 0, 0, 100, 10)};
  const std::vector<StripeDemandItem> items = {Item(0, 10, 5, -1),
                                               Item(1, 20, 5, 99)};
  BudgetOracle oracle = Uniform(1, 20, 2, 10);

  const StripeAssignmentPlan plan =
      PlanCapacityAwareStripeAssignment(slots, items, oracle.Predicate());

  ASSERT_TRUE(plan.feasible) << plan.refusal;
  EXPECT_EQ(plan.slot_of_item[0], 0);
  EXPECT_EQ(plan.slot_of_item[1], 0);
}

TEST(StripeCapacityAssignmentTest, ManyFragmentedLinesRedistribute) {
  // The width-64 shape in miniature: many fragments, proximity piling
  // everything into the first few, ample total capacity.
  std::vector<StripeSlot> slots;
  for (int index = 0; index < 16; ++index) {
    slots.push_back(Slot(index, 0, 40 * index, 100, 40 * index + 10));
  }
  std::vector<StripeDemandItem> items;
  for (int index = 0; index < 80; ++index) {
    items.push_back(Item(index, 10, 5 + 40 * (index % 3), index % 3));
  }
  BudgetOracle oracle = Uniform(16, 100, 80, 10);

  const StripeAssignmentPlan plan =
      PlanCapacityAwareStripeAssignment(slots, items, oracle.Predicate());

  ASSERT_TRUE(plan.feasible) << plan.refusal;
  EXPECT_GT(plan.overloaded_slots_before, 0);
  EXPECT_EQ(plan.overloaded_slots_after, 0);
  unsigned long long placed = 0;
  for (const StripeSlot &slot : slots) {
    const unsigned long long load = oracle.Load(plan, slot.index);
    EXPECT_LE(load, oracle.Capacity(slot.index));
    placed += load;
  }
  EXPECT_EQ(placed, oracle.TotalDemand());
}

TEST(StripeCapacityAssignmentTest, TheOracleIsTheOnlyCapacityAuthority) {
  // The negative control for the whole design: an oracle that refuses
  // everything must produce a refusal, whatever the geometry suggests. If the
  // planner ever grew its own capacity notion, this would start passing.
  const std::vector<StripeSlot> slots = {Slot(0, 0, 0, 1000, 1000)};
  const std::vector<StripeDemandItem> items = {Item(0, 10, 5, 0)};

  const StripeAssignmentPlan plan = PlanCapacityAwareStripeAssignment(
      slots, items, [](int, const std::vector<int> &) { return false; });

  EXPECT_FALSE(plan.feasible);
  EXPECT_GT(plan.oracle_queries, 0) << "the oracle must actually be consulted";
}

TEST(StripeSlotDistanceTest, IsZeroInsideAndGrowsOutside) {
  const StripeSlot slot = Slot(0, 10, 10, 20, 20);

  EXPECT_EQ(StripeSlotDistance(slot, 15, 15), 0);
  EXPECT_EQ(StripeSlotDistance(slot, 10, 10), 0);
  EXPECT_EQ(StripeSlotDistance(slot, 20, 20), 0);
  EXPECT_EQ(StripeSlotDistance(slot, 25, 15), 5);
  EXPECT_EQ(StripeSlotDistance(slot, 15, 5), 5);
  EXPECT_EQ(StripeSlotDistance(slot, 25, 25), 10);
}

TEST(StripeAssignmentSeedTest, SeedsReassignedComponentInsideFragment) {
  const StripeSlot slot = Slot(0, 100, 200, 180, 240);
  const StripeAssignmentSeed seed =
      SeedReassignedComponent(slot, 70, 20, 10);

  EXPECT_EQ(seed.llx, 100);
  EXPECT_EQ(seed.lly, 215);
}

TEST(StripeAssignmentSeedTest, PreservesContainedXAndCentersY) {
  const StripeSlot slot = Slot(0, 100, 200, 180, 240);
  const StripeAssignmentSeed seed =
      SeedReassignedComponent(slot, 130, 20, 12);

  EXPECT_EQ(seed.llx, 130);
  EXPECT_EQ(seed.lly, 214);
}

TEST(StripeAssignmentSeedTest, ClampsOversizedComponentToFragmentOrigin) {
  const StripeSlot slot = Slot(0, 100, 200, 105, 205);
  const StripeAssignmentSeed seed =
      SeedReassignedComponent(slot, 130, 20, 12);

  EXPECT_EQ(seed.llx, 100);
  EXPECT_EQ(seed.lly, 200);
}

}  // namespace dali
