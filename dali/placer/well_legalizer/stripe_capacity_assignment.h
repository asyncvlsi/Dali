/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#ifndef DALI_PLACER_WELL_LEGALIZER_STRIPE_CAPACITY_ASSIGNMENT_H_
#define DALI_PLACER_WELL_LEGALIZER_STRIPE_CAPACITY_ASSIGNMENT_H_

#include <functional>
#include <string>
#include <vector>

namespace dali {

/**
 * One whitespace fragment a movable component may be assigned to.
 *
 * Fragments are the unit because fixed obstacles cut rows inside a column, and
 * components in different fragments of one column cannot share a gridded row.
 *
 * There is deliberately no capacity number here. Whether a set of components
 * fits is decided by the caller's `fits` oracle, which asks the existing
 * gridded capacity estimator -- the same one the legalizer already trusts.
 * An area budget was tried first and was wrong: a fragment 134 wide and 9 tall
 * has ample area for fourteen cells and room for only one of the two shelves
 * they pack into, so area said yes where row height said no.
 */
struct StripeSlot {
  int index = -1;
  int llx = 0;
  int lly = 0;
  int urx = 0;
  int ury = 0;
};

/** One movable component seeking a fragment, with what proximity chose. */
struct StripeDemandItem {
  int index = -1;
  int x = 0;
  int y = 0;
  int preferred_slot = -1;
};

/**
 * Whether a fragment can hold exactly this set of items.
 *
 * Item indices are positions in the demands vector. The oracle is consulted
 * rather than a stored budget so that this file never has to know how rows,
 * wells or tap reservations turn components into height.
 */
using StripeFitsPredicate =
    std::function<bool(int slot_index, const std::vector<int> &item_indices)>;

struct StripeAssignmentPlan {
  /** Chosen slot index per item, parallel to the input demands. */
  std::vector<int> slot_of_item;
  int moved_count = 0;
  int overloaded_slots_before = 0;
  int overloaded_slots_after = 0;
  int oracle_queries = 0;
  bool feasible = false;
  std::string refusal;
};

/** A lower-left seed for an item reassigned to a whitespace fragment. */
struct StripeAssignmentSeed {
  int llx = 0;
  int lly = 0;
};

/**
 * Rectangle distance from a point to a slot, zero when the point is inside.
 *
 * Exposed so the ordering a plan uses can be asserted directly rather than
 * inferred from which slot happened to win.
 */
long long StripeSlotDistance(const StripeSlot &slot, int x, int y);

/**
 * Seed a reassigned component inside its fragment before row clustering.
 *
 * Capacity planning assumes that components assigned to one fragment may be
 * packed together. The clusterer starts from current coordinates, so leaving a
 * reassigned component at its former fragment can manufacture extra rows that
 * the capacity oracle never modeled. X is changed only as much as containment
 * requires; Y is centered so reassigned components overlap and can form the
 * shelves the oracle predicted. The capacity oracle must first establish that
 * the component fits the fragment.
 */
StripeAssignmentSeed SeedReassignedComponent(const StripeSlot &slot,
                                              int current_llx, int width,
                                              int height);

/**
 * Re-own components by fragment capacity, preferring where they already are.
 *
 * Proximity ownership assigns every component to its nearest fragment with no
 * capacity term, which is correct until fixed obstacles fragment the rows: then
 * the nearest fragment is often a sliver beside an obstacle while the spare
 * capacity sits a few fragments away. This keeps every component that already
 * fits exactly where proximity put it, and moves only the overflow, to the
 * nearest fragment that can hold it.
 *
 * Deterministic throughout: items are considered in index order, candidate
 * slots are ordered by distance and then by slot index, and no random or
 * pointer-address ordering participates. A component whose preferred slot still
 * has room is never moved, so a placement that was already feasible comes back
 * unchanged.
 *
 * `feasible` is false when some item has no slot with room anywhere. The plan
 * is still fully populated in that case -- callers report the refusal and apply
 * nothing.
 */
StripeAssignmentPlan PlanCapacityAwareStripeAssignment(
    const std::vector<StripeSlot> &slots,
    const std::vector<StripeDemandItem> &items,
    const StripeFitsPredicate &fits);

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_STRIPE_CAPACITY_ASSIGNMENT_H_
