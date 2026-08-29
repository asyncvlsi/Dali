/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/stripe_capacity_assignment.h"

#include <algorithm>
#include <string>
#include <vector>

namespace dali {

long long StripeSlotDistance(const StripeSlot &slot, int x, int y) {
  const long long dx = x < slot.llx   ? static_cast<long long>(slot.llx) - x
                       : x > slot.urx ? static_cast<long long>(x) - slot.urx
                                      : 0;
  const long long dy = y < slot.lly   ? static_cast<long long>(slot.lly) - y
                       : y > slot.ury ? static_cast<long long>(y) - slot.ury
                                      : 0;
  return dx + dy;
}

StripeAssignmentSeed SeedReassignedComponent(const StripeSlot &slot,
                                              int current_llx, int width,
                                              int height) {
  const int max_llx = std::max(slot.llx, slot.urx - width);
  const int max_lly = std::max(slot.lly, slot.ury - height);
  StripeAssignmentSeed seed;
  seed.llx = std::clamp(current_llx, slot.llx, max_llx);
  seed.lly = std::clamp((slot.lly + slot.ury - height) / 2, slot.lly, max_lly);
  return seed;
}

StripeAssignmentPlan PlanCapacityAwareStripeAssignment(
    const std::vector<StripeSlot> &slots,
    const std::vector<StripeDemandItem> &items,
    const StripeFitsPredicate &fits) {
  StripeAssignmentPlan plan;
  plan.slot_of_item.assign(items.size(), -1);
  if (slots.empty()) {
    plan.refusal = "no whitespace fragment is available";
    return plan;
  }

  // Components are considered in component-id order, not in the order the
  // caller happened to build its list. Both are deterministic for one run, but
  // only this one gives the same answer if a caller ever reorders its vector --
  // otherwise which component gets displaced is an artefact of iteration order.
  std::vector<std::size_t> order(items.size());
  for (std::size_t index = 0; index < items.size(); ++index) order[index] = index;
  std::sort(order.begin(), order.end(),
            [&items](std::size_t left, std::size_t right) {
              if (items[left].index != items[right].index) {
                return items[left].index < items[right].index;
              }
              return left < right;
            });

  // Start from what proximity chose, so a fragment that was never overloaded
  // keeps exactly the components it already had.
  std::vector<std::vector<int>> occupants(slots.size());
  std::vector<std::size_t> unowned;
  for (std::size_t item_index : order) {
    const int preferred = items[item_index].preferred_slot;
    if (preferred < 0 || preferred >= static_cast<int>(slots.size())) {
      unowned.push_back(item_index);
      continue;
    }
    occupants[preferred].push_back(static_cast<int>(item_index));
  }

  const auto slot_fits = [&](int slot_index) {
    ++plan.oracle_queries;
    return fits(slot_index, occupants[slot_index]);
  };

  // Evict from every over-full fragment until it fits, furthest component
  // first: the one with least claim to be there is the one that leaves.
  std::vector<std::size_t> displaced;
  for (std::size_t slot_index = 0; slot_index < slots.size(); ++slot_index) {
    if (occupants[slot_index].empty()) continue;
    if (slot_fits(static_cast<int>(slot_index))) continue;
    ++plan.overloaded_slots_before;

    std::vector<int> ordered = occupants[slot_index];
    std::sort(ordered.begin(), ordered.end(),
              [&](int left, int right) {
                const long long left_distance = StripeSlotDistance(
                    slots[slot_index], items[left].x, items[left].y);
                const long long right_distance = StripeSlotDistance(
                    slots[slot_index], items[right].x, items[right].y);
                if (left_distance != right_distance) {
                  return left_distance > right_distance;
                }
                return items[left].index > items[right].index;
              });
    for (int evicted : ordered) {
      if (slot_fits(static_cast<int>(slot_index))) break;
      occupants[slot_index].erase(
          std::find(occupants[slot_index].begin(), occupants[slot_index].end(),
                    evicted));
      displaced.push_back(static_cast<std::size_t>(evicted));
    }
  }

  // Everything without a home goes to the nearest fragment that still fits with
  // it added. Displaced components are re-homed in component-id order so the
  // result does not depend on which fragment evicted them first.
  unowned.insert(unowned.end(), displaced.begin(), displaced.end());
  std::sort(unowned.begin(), unowned.end(),
            [&items](std::size_t left, std::size_t right) {
              if (items[left].index != items[right].index) {
                return items[left].index < items[right].index;
              }
              return left < right;
            });

  bool feasible = true;
  for (std::size_t item_index : unowned) {
    const StripeDemandItem &item = items[item_index];
    std::vector<std::size_t> candidates(slots.size());
    for (std::size_t index = 0; index < slots.size(); ++index) {
      candidates[index] = index;
    }
    std::sort(candidates.begin(), candidates.end(),
              [&](std::size_t left, std::size_t right) {
                const long long left_distance =
                    StripeSlotDistance(slots[left], item.x, item.y);
                const long long right_distance =
                    StripeSlotDistance(slots[right], item.x, item.y);
                if (left_distance != right_distance) {
                  return left_distance < right_distance;
                }
                return left < right;
              });

    bool placed = false;
    for (std::size_t slot_index : candidates) {
      occupants[slot_index].push_back(static_cast<int>(item_index));
      if (slot_fits(static_cast<int>(slot_index))) {
        placed = true;
        if (static_cast<int>(slot_index) != item.preferred_slot) {
          ++plan.moved_count;
        }
        break;
      }
      occupants[slot_index].pop_back();
    }
    if (!placed) {
      feasible = false;
      if (plan.refusal.empty()) {
        plan.refusal = "component " + std::to_string(item.index) +
                       " has no whitespace fragment with room for it";
      }
    }
  }

  for (std::size_t slot_index = 0; slot_index < slots.size(); ++slot_index) {
    for (int item_index : occupants[slot_index]) {
      plan.slot_of_item[item_index] = static_cast<int>(slot_index);
    }
    if (!occupants[slot_index].empty() &&
        !slot_fits(static_cast<int>(slot_index))) {
      ++plan.overloaded_slots_after;
    }
  }
  plan.feasible = feasible && plan.overloaded_slots_after == 0;
  if (!plan.feasible && plan.refusal.empty()) {
    plan.refusal = "some fragment is still over capacity after reassignment";
  }
  return plan;
}

}  // namespace dali
