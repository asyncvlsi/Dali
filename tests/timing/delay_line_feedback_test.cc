#include "dali/timing/delay_line_feedback.h"

#include <gtest/gtest.h>

namespace dali {

namespace delay_line_feedback_test {

RelativeTimingConstraintSnapshot Constraint(
    int id, double slack, std::initializer_list<std::string> candidates) {
  RelativeTimingConstraintSnapshot constraint;
  constraint.constraint_id = id;
  constraint.slack = slack;
  constraint.delay_repair_candidate_nets.assign(candidates);
  return constraint;
}

RelativeTimingConstraintSnapshot IdentifiedConstraint(
    int id, double slack, const std::string &root, const std::string &slow,
    const std::string &fast,
    std::initializer_list<std::string> candidates = {}) {
  RelativeTimingConstraintSnapshot constraint = Constraint(id, slack, candidates);
  constraint.fast_path.root_pin = root;
  constraint.slow_path.root_pin = root;
  constraint.slow_path.terminal_pin = slow;
  constraint.fast_path.terminal_pin = fast;
  return constraint;
}

} // namespace delay_line_feedback_test

using delay_line_feedback_test::Constraint;
using delay_line_feedback_test::IdentifiedConstraint;

TEST(DelayLineFeedbackTest, AttributesByEvidenceWhenConstraintIdsArePermuted) {
  const auto result = AttributeDelayLineConstraints(
      {"dl0", "dl1"},
      {Constraint(1, -12.0, {"dl0_at_1"}),
       Constraint(0, -34.0, {"dl1_at_1"})});

  ASSERT_TRUE(result.valid()) << result.error;
  ASSERT_EQ(result.per_line.size(), 2U);
  EXPECT_EQ(result.per_line[0].constraint_ids, (std::vector<int>{1}));
  EXPECT_EQ(result.per_line[1].constraint_ids, (std::vector<int>{0}));
  EXPECT_DOUBLE_EQ(result.per_line[0].binding_slack, -12.0);
  EXPECT_DOUBLE_EQ(result.per_line[1].binding_slack, -34.0);
}

TEST(DelayLineFeedbackTest, AttributesPassingConstraintsToo) {
  const auto result = AttributeDelayLineConstraints(
      {"dl0"},
      {Constraint(3, 18.0, {"dl0.pass"}),
       Constraint(4, -2.0, {"dl0.fail"})});

  ASSERT_TRUE(result.valid()) << result.error;
  ASSERT_EQ(result.per_line[0].constraint_ids,
            (std::vector<int>{3, 4}));
  EXPECT_DOUBLE_EQ(result.per_line[0].binding_slack, -2.0);
}

TEST(DelayLineFeedbackTest, UsesMinimumSlackForSeveralConstraints) {
  const auto result = AttributeDelayLineConstraints(
      {"dl0"},
      {Constraint(8, 7.0, {"dl0_a"}), Constraint(2, -9.0, {"dl0_b"}),
       Constraint(5, 3.0, {"dl0_c"})});

  ASSERT_TRUE(result.valid()) << result.error;
  EXPECT_DOUBLE_EQ(result.per_line[0].binding_slack, -9.0);
  EXPECT_EQ(result.per_line[0].binding_constraint_id, 2);
}

// A stability study that compared slacks alone would call two samples identical
// when they are bound by different paths that happen to be equally tight, so
// the identity of the binding constraint travels with its value.
TEST(DelayLineFeedbackTest, EquallyTightConstraintsBindDeterministically) {
  const auto result = AttributeDelayLineConstraints(
      {"dl0"}, {Constraint(8, -4.0, {"dl0_a"}), Constraint(2, -4.0, {"dl0_b"})});

  ASSERT_TRUE(result.valid()) << result.error;
  EXPECT_DOUBLE_EQ(result.per_line[0].binding_slack, -4.0);
  EXPECT_EQ(result.per_line[0].binding_constraint_id, 8)
      << "a tie must resolve the same way every time it is measured";
}

TEST(DelayLineFeedbackTest, DoesNotMatchLongerToken) {
  const auto result = AttributeDelayLineConstraints(
      {"dl1"}, {Constraint(10, -3.0, {"dl10_at_1"})});

  EXPECT_FALSE(result.valid());
  EXPECT_NE(result.error.find("missing"), std::string::npos);
}

TEST(DelayLineFeedbackTest, ReportsMissingAttribution) {
  const auto result = AttributeDelayLineConstraints(
      {"dl0", "dl1"}, {Constraint(0, -3.0, {"dl0_at_1"})});

  EXPECT_FALSE(result.valid());
  EXPECT_NE(result.error.find("dl1"), std::string::npos);
}

TEST(DelayLineFeedbackTest, ReportsAmbiguousAttribution) {
  const auto result = AttributeDelayLineConstraints(
      {"foo", "foo.bar"}, {Constraint(0, -3.0, {"foo.bar_at_1"})});

  EXPECT_FALSE(result.valid());
  EXPECT_NE(result.error.find("ambiguous"), std::string::npos);
}

TEST(DelayLineAttributionEvidenceTest, RestoresOwnersAfterIdsAreRenumbered) {
  const std::vector<RelativeTimingConstraintSnapshot> original = {
      IdentifiedConstraint(0, -8.0, "root:A", "dl0:Y", "logic:Y",
                           {"dl0_at_1"}),
      IdentifiedConstraint(1, 3.0, "root:B", "dl1:Y", "logic:Z",
                           {"dl1_at_2"})};
  std::vector<ConstraintAttributionEvidence> evidence;
  std::string error;
  ASSERT_TRUE(CaptureConstraintAttributionEvidence(
      {"dl0", "dl1"}, original, &evidence, &error)) << error;

  std::vector<RelativeTimingConstraintSnapshot> refreshed = {
      IdentifiedConstraint(401, 7.0, "root:B", "dl1:Y", "logic:Z"),
      IdentifiedConstraint(400, -2.0, "root:A", "dl0:Y", "logic:Y")};
  ASSERT_TRUE(RestoreConstraintAttributionEvidence(evidence, &refreshed, &error))
      << error;
  const auto result =
      AttributeDelayLineConstraints({"dl0", "dl1"}, refreshed);
  ASSERT_TRUE(result.valid()) << result.error;
  EXPECT_EQ(result.per_line[0].constraint_ids, (std::vector<int>{400}));
  EXPECT_EQ(result.per_line[1].constraint_ids, (std::vector<int>{401}));
  EXPECT_DOUBLE_EQ(result.per_line[0].binding_slack, -2.0);
  EXPECT_DOUBLE_EQ(result.per_line[1].binding_slack, 7.0);
}

TEST(DelayLineAttributionEvidenceTest, PreservesUnattributedConstraints) {
  const std::vector<RelativeTimingConstraintSnapshot> original = {
      IdentifiedConstraint(0, -8.0, "root:A", "dl0:Y", "logic:Y",
                           {"dl0_at_1"}),
      IdentifiedConstraint(1, 3.0, "root:B", "other:Y", "logic:Z")};
  std::vector<ConstraintAttributionEvidence> evidence;
  std::string error;
  ASSERT_TRUE(CaptureConstraintAttributionEvidence(
      {"dl0"}, original, &evidence, &error)) << error;
  std::vector<RelativeTimingConstraintSnapshot> refreshed = {
      IdentifiedConstraint(0, -4.0, "root:A", "dl0:Y", "logic:Y"),
      IdentifiedConstraint(1, 1.0, "root:B", "other:Y", "logic:Z")};
  ASSERT_TRUE(RestoreConstraintAttributionEvidence(evidence, &refreshed, &error))
      << error;
  EXPECT_TRUE(refreshed[1].delay_repair_candidate_nets.empty());
}

TEST(DelayLineAttributionEvidenceTest, RejectsAmbiguousOriginalOwner) {
  const std::vector<RelativeTimingConstraintSnapshot> original = {
      IdentifiedConstraint(0, -8.0, "root:A", "slow:Y", "fast:Y",
                           {"foo.bar_at_1"})};
  std::vector<ConstraintAttributionEvidence> evidence;
  std::string error;
  EXPECT_FALSE(CaptureConstraintAttributionEvidence(
      {"foo", "foo.bar"}, original, &evidence, &error));
  EXPECT_NE(error.find("ambiguous"), std::string::npos);
}

TEST(DelayLineAttributionEvidenceTest, RejectsChangedSemanticIdentity) {
  const std::vector<RelativeTimingConstraintSnapshot> original = {
      IdentifiedConstraint(0, -8.0, "root:A", "dl0:Y", "logic:Y",
                           {"dl0_at_1"})};
  std::vector<ConstraintAttributionEvidence> evidence;
  std::string error;
  ASSERT_TRUE(CaptureConstraintAttributionEvidence(
      {"dl0"}, original, &evidence, &error)) << error;
  std::vector<RelativeTimingConstraintSnapshot> refreshed = {
      IdentifiedConstraint(0, -8.0, "root:A", "dl0:Y", "changed:Y")};
  EXPECT_FALSE(RestoreConstraintAttributionEvidence(evidence, &refreshed,
                                                    &error));
  EXPECT_NE(error.find("does not match"), std::string::npos);
}

TEST(DelayLineAttributionEvidenceTest, RejectsMissingCurrentConstraint) {
  const std::vector<RelativeTimingConstraintSnapshot> original = {
      IdentifiedConstraint(0, -8.0, "root:A", "dl0:Y", "logic:Y",
                           {"dl0_at_1"}),
      IdentifiedConstraint(1, 2.0, "root:B", "dl0:Y", "logic:Z",
                           {"dl0_at_2"})};
  std::vector<ConstraintAttributionEvidence> evidence;
  std::string error;
  ASSERT_TRUE(CaptureConstraintAttributionEvidence(
      {"dl0"}, original, &evidence, &error)) << error;
  std::vector<RelativeTimingConstraintSnapshot> refreshed = {
      IdentifiedConstraint(0, -8.0, "root:A", "dl0:Y", "logic:Y")};
  EXPECT_FALSE(RestoreConstraintAttributionEvidence(evidence, &refreshed,
                                                    &error));
}

TEST(DelayLineAttributionEvidenceTest, RejectsExtraCurrentConstraint) {
  const std::vector<RelativeTimingConstraintSnapshot> original = {
      IdentifiedConstraint(0, -8.0, "root:A", "dl0:Y", "logic:Y",
                           {"dl0_at_1"})};
  std::vector<ConstraintAttributionEvidence> evidence;
  std::string error;
  ASSERT_TRUE(CaptureConstraintAttributionEvidence(
      {"dl0"}, original, &evidence, &error)) << error;
  std::vector<RelativeTimingConstraintSnapshot> refreshed = {
      IdentifiedConstraint(0, -8.0, "root:A", "dl0:Y", "logic:Y"),
      IdentifiedConstraint(1, 2.0, "root:B", "dl0:Y", "logic:Z")};
  EXPECT_FALSE(RestoreConstraintAttributionEvidence(evidence, &refreshed,
                                                    &error));
}

TEST(SeparationEscalationGateTest, SuppressesAnIncreaseWhenEscalationIsOff) {
  EXPECT_EQ(ApplySeparationEscalationGate(0, 10, false), 0);
  EXPECT_EQ(ApplySeparationEscalationGate(69, 85, false), 69);
}

TEST(SeparationEscalationGateTest, AppliesAnIncreaseWhenEscalationIsOn) {
  EXPECT_EQ(ApplySeparationEscalationGate(0, 10, true), 10);
  EXPECT_EQ(ApplySeparationEscalationGate(69, 85, true), 85);
}

TEST(SeparationEscalationGateTest, AppliesADecreaseWhicheverWay) {
  // The gate stops separation from consuming die height, not from giving it
  // back; refusing a decrease would leave a line holding rows it no longer
  // needs and would make the stride mechanism strictly worse off.
  EXPECT_EQ(ApplySeparationEscalationGate(85, 78, false), 78);
  EXPECT_EQ(ApplySeparationEscalationGate(85, 78, true), 78);
}

TEST(SeparationEscalationGateTest, LeavesAnUnchangedProposalAlone) {
  EXPECT_EQ(ApplySeparationEscalationGate(42, 42, false), 42);
  EXPECT_EQ(ApplySeparationEscalationGate(42, 42, true), 42);
}

TEST(DelayLineFeedbackEventTest, KeepsAProposalDistinctFromWhatWasApplied) {
  // The regression this pins: an event that carried only `new_separation` was
  // filled with the proposal, so a run holding separation at 0 logged
  // `new_separation 10` on every iteration and read as a working controller.
  const int current = 0;
  const int proposed = 10;
  const int applied = ApplySeparationEscalationGate(current, proposed, false);
  const DelayLineFeedbackEvent event{
      2, "dl0", {1}, -575.194, current, proposed, applied, 0.0, false};

  EXPECT_EQ(event.proposed_separation, 10);
  EXPECT_EQ(event.new_separation, 0);
  EXPECT_EQ(event.new_separation, event.old_separation)
      << "escalation is off, so nothing may have moved";
  EXPECT_FALSE(event.escalation_enabled);
  EXPECT_NE(event.proposed_separation, event.new_separation)
      << "a suppressed proposal must stay visible as a proposal";
}

TEST(DelayLineFeedbackEventTest, RecordsAnAppliedIncreaseAsBoth) {
  const int applied = ApplySeparationEscalationGate(10, 80, true);
  const DelayLineFeedbackEvent event{
      3, "dl0", {1}, -499.732, 10, 80, applied, 7.54621, true};

  EXPECT_EQ(event.proposed_separation, 80);
  EXPECT_EQ(event.new_separation, 80);
  EXPECT_TRUE(event.escalation_enabled);
}

} // namespace dali
