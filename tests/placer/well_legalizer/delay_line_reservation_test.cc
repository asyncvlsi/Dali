/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/placer/well_legalizer/delay_line_reservation.h"

#include <gtest/gtest.h>

#include <map>
#include <utility>
#include <set>
#include <string>
#include <vector>

namespace dali {

class DelayLineReservationTest : public testing::Test {
 protected:
  struct ComponentState {
    double llx;
    double lly;
    PlaceStatus status;
  };

  DelayLineReservationTest() {
    circuit_.SetManufacturingGrid(1);
    circuit_.SetUnitsDistanceMicrons(1);
    circuit_.SetGridValue(1, 1);
    circuit_.SetDieArea(0, 0, 100, 100);
    circuit_.ReserveSpaceForDesignImp(256, 0, 0);
    circuit_.AddMacro("cell", 4, 4);
    circuit_.AddMacro("wide", 101, 4);
  }

 public:
  int Add(const std::string& name, const std::string& macro, double x, double y,
          PlaceStatus status) {
    return AddComponent(name, macro, x, y, status);
  }

 protected:
  int AddComponent(const std::string& name, const std::string& macro, double x,
                   double y, PlaceStatus status) {
    circuit_.AddComponent(name, macro, x, y, status);
    return circuit_.GetComponentId(name);
  }

  std::map<int, ComponentState> Snapshot(
      const std::vector<int>& component_ids) const {
    std::map<int, ComponentState> snapshot;
    for (int component_id : component_ids) {
      const Component& component = circuit_.Components()[component_id];
      snapshot.emplace(component_id,
                       ComponentState{component.LLX(), component.LLY(),
                                      component.Status()});
    }
    return snapshot;
  }

  void ExpectUnchanged(const std::map<int, ComponentState>& snapshot) const {
    for (const auto& [component_id, expected] : snapshot) {
      const Component& component = circuit_.Components()[component_id];
      EXPECT_DOUBLE_EQ(component.LLX(), expected.llx);
      EXPECT_DOUBLE_EQ(component.LLY(), expected.lly);
      EXPECT_EQ(component.Status(), expected.status);
    }
  }

  Circuit circuit_;
};

TEST_F(DelayLineReservationTest,
       RejectsFixedMemberTranslationBeforeChangingAnyComponent) {
  const int blocker = AddComponent("blocker", "cell", 0, 0, PLACED);
  const int fixed_endpoint =
      AddComponent("fixed_endpoint", "cell", 0, 0, FIXED);
  const int movable_member =
      AddComponent("movable_member", "cell", 4, 0, PLACED);
  const std::vector<int> component_ids =
      {blocker, fixed_endpoint, movable_member};
  const auto before = Snapshot(component_ids);

  const DelayLineReservationPlan plan = PlanDelayLineReservations(
      circuit_, {{"line0", {blocker}},
                 {"line1", {fixed_endpoint, movable_member}}});

  EXPECT_FALSE(plan.valid());
  EXPECT_NE(plan.error.find("non-movable"), std::string::npos);
  ExpectUnchanged(before);
}

TEST_F(DelayLineReservationTest,
       RejectsLaterInfeasibleLineWithoutApplyingEarlierReservation) {
  const int first_line = AddComponent("first_line", "cell", -5, 0, PLACED);
  const int infeasible_line =
      AddComponent("infeasible_line", "wide", 0, 20, PLACED);
  const std::vector<int> component_ids = {first_line, infeasible_line};
  const auto before = Snapshot(component_ids);

  const DelayLineReservationPlan plan = PlanDelayLineReservations(
      circuit_, {{"valid", {first_line}}, {"infeasible", {infeasible_line}}});

  EXPECT_FALSE(plan.valid());
  EXPECT_NE(plan.error.find("fit"), std::string::npos);
  ExpectUnchanged(before);
}

TEST_F(DelayLineReservationTest, KeepsFixedAnchorWhenNoTranslationIsNeeded) {
  const int fixed_endpoint =
      AddComponent("fixed_anchor", "cell", 20, 20, FIXED);
  const int movable_member =
      AddComponent("movable_anchor_member", "cell", 24, 20, PLACED);

  const DelayLineReservationPlan plan = PlanDelayLineReservations(
      circuit_, {{"anchored", {fixed_endpoint, movable_member}}});

  ASSERT_TRUE(plan.valid()) << plan.error;
  ASSERT_EQ(plan.lines.size(), 1U);
  ASSERT_EQ(plan.lines[0].components.size(), 2U);
  EXPECT_EQ(plan.lines[0].components[0].component_id, fixed_endpoint);
  EXPECT_EQ(plan.lines[0].components[0].llx, 20);
  EXPECT_EQ(plan.lines[0].components[0].lly, 20);
  EXPECT_EQ(plan.lines[0].components[0].original_status, FIXED);
  EXPECT_EQ(plan.lines[0].components[1].original_status, PLACED);
}

TEST_F(DelayLineReservationTest, NamesBothLinesWhenReservationsCannotBePacked) {
  // Two lines each wider than half the 100-unit region: the second cannot be
  // shifted clear of the first, and wrapping to the left edge lands back on it.
  // The message has to say which two, because a run that shapes eight lines at
  // once otherwise gets the same sentence whichever pair collided.
  circuit_.AddMacro("half", 60, 4);
  const int first = AddComponent("first_member", "half", 0, 20, PLACED);
  const int second = AddComponent("second_member", "half", 20, 20, PLACED);

  const DelayLineReservationPlan plan = PlanDelayLineReservations(
      circuit_, {{"dl_left", {first}}, {"dl_right", {second}}});

  ASSERT_FALSE(plan.valid());
  EXPECT_NE(plan.error.find("dl_right"), std::string::npos) << plan.error;
  EXPECT_NE(plan.error.find("dl_left"), std::string::npos)
      << "the already-reserved line must be named too: " << plan.error;
  EXPECT_NE(plan.error.find("60 x 4"), std::string::npos)
      << "the size that could not be packed is the actionable part: "
      << plan.error;
}


// ---- the exact-footprint model ---------------------------------------------

namespace {

/**
 * A folded delay line: `pairs * 2` cells in two rows `separation` apart.
 *
 * This is the shape the whole question is about. Its bounding box is
 * `separation` rows tall and only two of those rows carry cells, so two such
 * boxes intersect long before any cell does.
 */
std::vector<int> FoldedLine(DelayLineReservationTest* fixture,
                            const std::string& prefix, int x, int y, int pairs,
                            int separation, PlaceStatus status = PLACED) {
  std::vector<int> ids;
  for (int index = 0; index < pairs; ++index) {
    ids.push_back(fixture->Add(prefix + "_lo_" + std::to_string(index), "cell",
                               x + 4 * index, y, status));
  }
  for (int index = 0; index < pairs; ++index) {
    ids.push_back(fixture->Add(prefix + "_hi_" + std::to_string(index), "cell",
                               x + 4 * index, y + separation, status));
  }
  return ids;
}

/** Whole-bounding-box exclusion, as the planner used to decide conflicts. */
bool BoundingBoxesIntersect(const DelayLineReservation& left,
                            const DelayLineReservation& right) {
  return left.llx < right.urx && right.llx < left.urx &&
         left.lly < right.ury && right.lly < left.ury;
}

int SharedSites(const DelayLineReservation& left,
                const DelayLineReservation& right, int cell_size) {
  std::set<std::pair<int, int>> sites;
  for (const DelayLineReservationComponent& component : left.components) {
    for (int y = component.lly; y < component.lly + cell_size; ++y) {
      for (int x = component.llx; x < component.llx + cell_size; ++x) {
        sites.insert({x, y});
      }
    }
  }
  int shared = 0;
  for (const DelayLineReservationComponent& component : right.components) {
    for (int y = component.lly; y < component.lly + cell_size; ++y) {
      for (int x = component.llx; x < component.llx + cell_size; ++x) {
        if (sites.count({x, y}) != 0) ++shared;
      }
    }
  }
  return shared;
}

} // namespace

TEST_F(DelayLineReservationTest, InterlockingSparseLinesAreAccepted) {
  // Two folded lines whose boxes overlap almost completely and whose cells
  // never touch: dl_a occupies rows 0 and 40, dl_b rows 8 and 48, and the two
  // boxes share rows 8 to 40.
  const std::vector<int> a = FoldedLine(this, "dl_a", 0, 0, 3, 40);
  const std::vector<int> b = FoldedLine(this, "dl_b", 0, 8, 3, 40);
  const auto before = Snapshot(a);

  const DelayLineReservationPlan plan =
      PlanDelayLineReservations(circuit_, {{"dl_a", a}, {"dl_b", b}});

  ASSERT_TRUE(plan.valid()) << plan.error;
  ASSERT_EQ(plan.lines.size(), 2U);
  EXPECT_TRUE(BoundingBoxesIntersect(plan.lines[0], plan.lines[1]))
      << "the case is only interesting if the boxes do overlap";
  EXPECT_EQ(SharedSites(plan.lines[0], plan.lines[1], 4), 0);
  ExpectUnchanged(before);  // planning mutates nothing
}

TEST_F(DelayLineReservationTest, WholeBoundingBoxRejectionWouldRefuseThat) {
  // The negative control. Restoring box exclusion -- the rule the planner used
  // before -- refuses the placement above, in which nothing touches anything.
  const std::vector<int> a = FoldedLine(this, "dl_a", 0, 0, 3, 40);
  const std::vector<int> b = FoldedLine(this, "dl_b", 0, 8, 3, 40);

  const DelayLineReservationPlan plan =
      PlanDelayLineReservations(circuit_, {{"dl_a", a}, {"dl_b", b}});

  ASSERT_TRUE(plan.valid()) << plan.error;
  const bool box_rule_would_reject =
      BoundingBoxesIntersect(plan.lines[0], plan.lines[1]);
  EXPECT_TRUE(box_rule_would_reject)
      << "if the boxes no longer intersect this control proves nothing";
  EXPECT_EQ(SharedSites(plan.lines[0], plan.lines[1], 4), 0)
      << "and the cells must genuinely be disjoint";
}

TEST_F(DelayLineReservationTest, ZeroTranslationWinsWhenItIsLegal) {
  const std::vector<int> a = FoldedLine(this, "dl_a", 8, 0, 2, 40);
  const std::vector<int> b = FoldedLine(this, "dl_b", 8, 8, 2, 40);

  const DelayLineReservationPlan plan =
      PlanDelayLineReservations(circuit_, {{"dl_a", a}, {"dl_b", b}});

  ASSERT_TRUE(plan.valid()) << plan.error;
  for (const DelayLineReservation& line : plan.lines) {
    EXPECT_EQ(line.llx, 8) << line.name << " was moved without needing to be";
  }
}

TEST_F(DelayLineReservationTest, RealCellOverlapTakesTheSmallestLegalShift) {
  // Same rows, so the cells genuinely collide and a translation is required.
  const std::vector<int> a = FoldedLine(this, "dl_a", 0, 0, 2, 40);
  const std::vector<int> b = FoldedLine(this, "dl_b", 4, 0, 2, 40);

  const DelayLineReservationPlan plan =
      PlanDelayLineReservations(circuit_, {{"dl_a", a}, {"dl_b", b}});

  ASSERT_TRUE(plan.valid()) << plan.error;
  EXPECT_EQ(SharedSites(plan.lines[0], plan.lines[1], 4), 0);
  EXPECT_EQ(plan.lines[0].llx, 0) << "the first line does not move";
  EXPECT_EQ(plan.lines[1].llx, 8)
      << "and the second moves just clear of it, not further";
}

TEST_F(DelayLineReservationTest, RelativeCoordinatesSurviveATranslation) {
  const std::vector<int> a = FoldedLine(this, "dl_a", 0, 0, 2, 40);
  const std::vector<int> b = FoldedLine(this, "dl_b", 4, 0, 3, 40);

  const DelayLineReservationPlan plan =
      PlanDelayLineReservations(circuit_, {{"dl_a", a}, {"dl_b", b}});

  ASSERT_TRUE(plan.valid()) << plan.error;
  const DelayLineReservation& moved = plan.lines[1];
  ASSERT_EQ(moved.components.size(), 6U);
  const int shift = moved.components[0].llx - 4;
  for (std::size_t index = 0; index < moved.components.size(); ++index) {
    const Component& original =
        circuit_.Components()[moved.components[index].component_id];
    EXPECT_EQ(moved.components[index].llx,
              static_cast<int>(original.LLX()) + shift)
        << "component " << index << " moved by a different amount";
    EXPECT_EQ(moved.components[index].lly, static_cast<int>(original.LLY()))
        << "a rigid translation must not change y";
  }
}

TEST_F(DelayLineReservationTest, DuplicateSitesWithinOneLineAreRejected) {
  const int first = Add("dup_a", "cell", 20, 20, PLACED);
  const int second = Add("dup_b", "cell", 20, 20, PLACED);
  const auto before = Snapshot({first, second});

  const DelayLineReservationPlan plan =
      PlanDelayLineReservations(circuit_, {{"dl_dup", {first, second}}});

  EXPECT_FALSE(plan.valid());
  EXPECT_NE(plan.error.find("same site"), std::string::npos) << plan.error;
  ExpectUnchanged(before);
}

TEST_F(DelayLineReservationTest, AComponentHangingOverAnEdgeIsBroughtInside) {
  // One cell past the top edge by its own height: a single rigid shift down
  // fixes it, and the planner takes the smallest one.
  const int outside = Add("outside", "cell", 20, 98, PLACED);

  const DelayLineReservationPlan plan =
      PlanDelayLineReservations(circuit_, {{"dl_out", {outside}}});

  ASSERT_TRUE(plan.valid()) << plan.error;
  EXPECT_EQ(plan.lines[0].components[0].lly, 96);
  EXPECT_EQ(plan.lines[0].components[0].llx, 20) << "x must not move";
}

TEST_F(DelayLineReservationTest, ANonMovableObstacleIsClearedWhenPossible) {
  // A fixed cell that belongs to no delay line, standing where one sits. The
  // line may move off it, and the plan must not leave the two on top of
  // each other whichever way it resolves that.
  Add("obstacle", "cell", 20, 20, FIXED);
  const int member = Add("member", "cell", 20, 20, PLACED);

  const DelayLineReservationPlan plan =
      PlanDelayLineReservations(circuit_, {{"dl_blocked", {member}}});

  ASSERT_TRUE(plan.valid()) << plan.error;
  ASSERT_EQ(plan.lines.size(), 1U);
  EXPECT_EQ(plan.lines[0].components[0].llx, 24)
      << "the smallest shift that clears the obstacle, and no more";
  EXPECT_EQ(plan.lines[0].components[0].lly, 20);
}

TEST_F(DelayLineReservationTest, ANonMovableObstacleWithNoWayPastIsRejected) {
  // The same collision with nowhere to go: a fixed cell spanning the whole
  // region on the line's row. A rigid x-translation cannot clear it, so the
  // planner has to refuse rather than emit a plan that overlaps a cell
  // legalization is not allowed to move.
  Add("wall", "wide", 0, 20, FIXED);
  const int member = Add("member", "cell", 20, 20, PLACED);
  const auto before = Snapshot({member});

  const DelayLineReservationPlan plan =
      PlanDelayLineReservations(circuit_, {{"dl_blocked", {member}}});

  EXPECT_FALSE(plan.valid());
  EXPECT_NE(plan.error.find("non-movable"), std::string::npos) << plan.error;
  ExpectUnchanged(before);
}

TEST_F(DelayLineReservationTest, ALineTallerThanTheRegionIsRejected) {
  // A folded line whose two rows are further apart than the region is tall. A
  // rigid translation cannot make it fit, so the planner says so instead of
  // placing part of it outside.
  const std::vector<int> tall = FoldedLine(this, "dl_tall", 20, 0, 2, 98);
  const auto before = Snapshot(tall);

  const DelayLineReservationPlan plan =
      PlanDelayLineReservations(circuit_, {{"dl_tall", tall}});

  EXPECT_FALSE(plan.valid());
  EXPECT_NE(plan.error.find("fit"), std::string::npos) << plan.error;
  ExpectUnchanged(before);
}

TEST_F(DelayLineReservationTest, RequestOrderDoesNotChangeTheFinalPlan) {
  const std::vector<int> a = FoldedLine(this, "dl_a", 0, 0, 2, 40);
  const std::vector<int> b = FoldedLine(this, "dl_b", 0, 8, 2, 40);
  const std::vector<int> c = FoldedLine(this, "dl_c", 0, 16, 2, 40);

  const DelayLineReservationPlan forward = PlanDelayLineReservations(
      circuit_, {{"dl_a", a}, {"dl_b", b}, {"dl_c", c}});
  const DelayLineReservationPlan reversed = PlanDelayLineReservations(
      circuit_, {{"dl_c", c}, {"dl_b", b}, {"dl_a", a}});

  ASSERT_TRUE(forward.valid()) << forward.error;
  ASSERT_TRUE(reversed.valid()) << reversed.error;
  std::map<std::string, std::pair<int, int>> forward_positions;
  for (const DelayLineReservation& line : forward.lines) {
    forward_positions[line.name] = {line.llx, line.lly};
  }
  for (const DelayLineReservation& line : reversed.lines) {
    EXPECT_EQ(forward_positions[line.name].first, line.llx) << line.name;
    EXPECT_EQ(forward_positions[line.name].second, line.lly) << line.name;
  }
}

TEST_F(DelayLineReservationTest, ThePlanCarriesEveryComponentAndItsOldStatus) {
  // What the caller fixes during legalization and restores afterwards is
  // exactly this list, so a line missing from it would be a cell the legalizer
  // was free to move and the restore would never put back.
  const std::vector<int> a = FoldedLine(this, "dl_a", 0, 0, 3, 40);

  const DelayLineReservationPlan plan =
      PlanDelayLineReservations(circuit_, {{"dl_a", a}});

  ASSERT_TRUE(plan.valid()) << plan.error;
  ASSERT_EQ(plan.lines.size(), 1U);
  EXPECT_EQ(plan.lines[0].components.size(), a.size());
  std::set<int> planned;
  for (const DelayLineReservationComponent& component :
       plan.lines[0].components) {
    planned.insert(component.component_id);
    EXPECT_EQ(component.original_status, PLACED);
  }
  EXPECT_EQ(planned, std::set<int>(a.begin(), a.end()));
}

TEST_F(DelayLineReservationTest, EightFoldedLinesPlanWithoutSharedSites) {
  // The width-64 shape in miniature: eight folded lines, two of them long,
  // interleaved so that every box overlaps several others.
  std::vector<DelayLineReservationRequest> requests;
  const int pairs[] = {3, 12, 3, 3, 3, 12, 3, 3};
  for (int line = 0; line < 8; ++line) {
    const std::string name = "dl" + std::to_string(line);
    requests.push_back(
        {name, FoldedLine(this, name, 0, 2 * line, pairs[line], 60)});
  }

  const DelayLineReservationPlan plan =
      PlanDelayLineReservations(circuit_, requests);

  ASSERT_TRUE(plan.valid()) << plan.error;
  ASSERT_EQ(plan.lines.size(), 8U);
  int box_intersections = 0;
  for (std::size_t left = 0; left < plan.lines.size(); ++left) {
    for (std::size_t right = left + 1; right < plan.lines.size(); ++right) {
      if (BoundingBoxesIntersect(plan.lines[left], plan.lines[right])) {
        ++box_intersections;
      }
      EXPECT_EQ(SharedSites(plan.lines[left], plan.lines[right], 4), 0)
          << plan.lines[left].name << " and " << plan.lines[right].name;
    }
  }
  EXPECT_GT(box_intersections, 0)
      << "eight interleaved folded lines must overlap by box, or the case is "
         "not the one that was failing";
}

}  // namespace dali
