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
 * What the timing pane selects and shows.
 *
 * Built from captured metadata rather than from a placement run, so these
 * assert the pane's own behaviour: which constraint it lands on, what survives
 * a re-elaboration that renumbers the timer's ids, and what it says when a
 * piece of evidence does not exist yet.
 *
 * The numbers are the real ones measured on width-64 bd_pipeline in Gate 0, so
 * a change that silently rescales or mislabels a delay shows up here.
 */
#include "dali/gui/qt_timing_diagnostics_panel.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QCoreApplication>
#include <QSplitter>

#include <string>

namespace dali {
namespace {

PlacementTimingPathVisualization Path(int id, const std::string &identity,
                                      const std::string &line, double slack,
                                      double fast, double slow) {
  PlacementTimingPathVisualization path;
  path.constraint_id = id;
  path.semantic_identity = identity;
  path.attributed_delay_line = line;
  path.slack_ps = slack;
  path.fast_delay_ps = fast;
  path.slow_delay_ps = slow;
  path.has_geometry = true;
  path.fast_only_component_ids = {1, 2};
  path.slow_only_component_ids = {3, 4};
  path.common_component_ids = {5};
  path.fast_only_edges = {{1, 2, "nf"}};
  path.slow_only_edges = {{3, 4, "ns"}};
  path.common_edges = {{5, 1, "nc"}};
  path.root_component_id = 5;
  path.fast_terminal_component_id = 2;
  path.slow_terminal_component_id = 4;
  return path;
}

PlacementDelayLineTimingVisualization Line(
    const std::string &name,
    std::vector<PlacementTimingPathVisualization> constraints) {
  PlacementDelayLineTimingVisualization line;
  line.delay_line_name = name;
  line.constraints = std::move(constraints);
  if (!line.constraints.empty()) {
    line.worst_constraint_id = line.constraints.front().constraint_id;
    line.worst_slack_ps = line.constraints.front().slack_ps;
  }
  return line;
}

/** The Gate 0 measurements, as the pane would receive them. */
std::vector<PlacementDelayLineTimingVisualization> MeasuredLines() {
  return {
      Line("dl0", {Path(202, "loop__c:Y|dl0_ainv_525_6:Y|g0_562_6:Y", "dl0",
                        129.9000, 1747.04, 1876.94)}),
      Line("dl2", {Path(114, "dl1_a:Y|dl2_a:Y|g2_51_6:Y", "dl2", 106.2820,
                        1462.60, 1568.89),
                   Path(77, "dl1_b:Y|dl2_b:Y|g2_77_6:Y", "dl2", 109.2900,
                        1470.00, 1579.29)}),
      Line("dl5", {Path(59, "dl4_a:Y|dl5_a:Y|s5__63:Y", "dl5", 336.3590,
                        13886.50, 14222.90)}),
      Line("dl7", {Path(128, "dl6_a:Y|dl7_a:Y|h7_563_6:Y", "dl7", 79.4171,
                        1365.69, 1445.11)}),
  };
}

class TimingPanelTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    if (QApplication::instance() != nullptr) return;
    static int argc = 1;
    static char name[] = "qt_timing_diagnostics_panel_test";
    static char *argv[] = {name, nullptr};
    // Deliberately never destroyed. A QApplication torn down during static
    // destruction outlives parts of Qt it depends on and crashes after every
    // test has already passed, which reads as a test failure and is not one.
    new QApplication(argc, argv);
  }
};

TEST_F(TimingPanelTest, StartsWithNoTimingDataAndNoSelection) {
  TimingDiagnosticsPanel panel;
  EXPECT_FALSE(panel.HasTimingData());
  EXPECT_EQ(panel.SelectedPath(), nullptr);
  EXPECT_EQ(panel.SelectedConstraintId(), -1);
}

// Arriving data selects nothing: a highlight the operator did not ask for is
// indistinguishable from one they did. SelectDefault still exists and still
// ranks correctly -- dl7 at +79.4171, not dl0 which happens to be listed first
// -- it is simply no longer automatic.
TEST_F(TimingPanelTest, NewTimingDataSelectsNothing) {
  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {});

  EXPECT_TRUE(panel.HasTimingData());
  EXPECT_TRUE(panel.SelectedLine().empty());
  EXPECT_EQ(panel.SelectedConstraintId(), -1);
  EXPECT_EQ(panel.SelectedPath(), nullptr);
  EXPECT_FALSE(panel.PathControlsEnabled());
}

TEST_F(TimingPanelTest, SelectDefaultStillPicksTheGloballyWorstLine) {
  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {});

  panel.SelectDefault();

  EXPECT_EQ(panel.SelectedLine(), "dl7");
  EXPECT_EQ(panel.SelectedConstraintId(), 128);
  ASSERT_NE(panel.SelectedPath(), nullptr);
  EXPECT_DOUBLE_EQ(panel.SelectedPath()->slack_ps, 79.4171);
  EXPECT_TRUE(panel.PathControlsEnabled());
}

TEST_F(TimingPanelTest, ChildrenAreListedWorstFirstUnderEachLine) {
  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {});
  QStandardItemModel *model = panel.Model();

  // dl2 is the second group and owns two constraints, worst first.
  QStandardItem *dl2 = model->item(1, 0);
  ASSERT_EQ(dl2->text(), "dl2");
  ASSERT_EQ(dl2->rowCount(), 2);
  EXPECT_TRUE(dl2->child(0, 0)->text().startsWith("Worst constraint"));
  EXPECT_TRUE(dl2->child(0, 0)->text().contains("#114"));
  EXPECT_TRUE(dl2->child(1, 0)->text().startsWith("Constraint"));
  EXPECT_TRUE(dl2->child(1, 0)->text().contains("#77"));
  // "Binding" is not user-facing anywhere in the tree.
  EXPECT_FALSE(dl2->child(0, 0)->text().contains("Binding"));
}

TEST_F(TimingPanelTest, SelectingAnotherConstraintUpdatesTheSelection) {
  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {});

  ASSERT_TRUE(panel.SelectByIdentity("dl2", "dl1_b:Y|dl2_b:Y|g2_77_6:Y"));
  EXPECT_EQ(panel.SelectedConstraintId(), 77);
  ASSERT_NE(panel.SelectedPath(), nullptr);
  EXPECT_DOUBLE_EQ(panel.SelectedPath()->slack_ps, 109.29);
}

// The whole point of keeping the identity: a re-elaboration renumbers the
// timer's ids, and the selection must follow the constraint rather than the
// number.
TEST_F(TimingPanelTest, SelectionSurvivesRenumberingByIdentity) {
  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {});
  ASSERT_TRUE(panel.SelectByIdentity("dl2", "dl1_b:Y|dl2_b:Y|g2_77_6:Y"));
  ASSERT_EQ(panel.SelectedConstraintId(), 77);

  std::vector<PlacementDelayLineTimingVisualization> renumbered =
      MeasuredLines();
  for (PlacementTimingPathVisualization &path : renumbered[1].constraints) {
    path.constraint_id += 400;
  }
  panel.SetTimingData("final_placement", renumbered, {}, {}, {});

  EXPECT_EQ(panel.SelectedIdentity(), "dl1_b:Y|dl2_b:Y|g2_77_6:Y");
  EXPECT_EQ(panel.SelectedConstraintId(), 477)
      << "the selection followed the number instead of the constraint";
}

TEST_F(TimingPanelTest, AVanishedIdentityFallsBackToTheLinesWorstConstraint) {
  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {});
  ASSERT_TRUE(panel.SelectByIdentity("dl2", "dl1_b:Y|dl2_b:Y|g2_77_6:Y"));

  std::vector<PlacementDelayLineTimingVisualization> without =
      MeasuredLines();
  without[1].constraints.erase(without[1].constraints.begin() + 1);
  panel.SetTimingData("final_placement", without, {}, {}, {});

  EXPECT_EQ(panel.SelectedLine(), "dl2");
  EXPECT_EQ(panel.SelectedConstraintId(), 114)
      << "the fallback must be this line's current worst constraint";
}

TEST_F(TimingPanelTest, InspectorShowsTheSampleNameAndMeasuredDelays) {
  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {});
  ASSERT_TRUE(panel.SelectByIdentity("dl5", "dl4_a:Y|dl5_a:Y|s5__63:Y"));

  const QString text = panel.InspectorText();
  EXPECT_TRUE(text.contains("final_placement"));
  EXPECT_TRUE(text.contains("13886.50")) << text.toStdString();
  EXPECT_TRUE(text.contains("14222.90")) << text.toStdString();
  EXPECT_TRUE(text.contains("+336.359")) << text.toStdString();
  EXPECT_TRUE(text.contains("#59"));
}

TEST_F(TimingPanelTest, MissingDecisionEvidenceReadsUnavailable) {
  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {});
  panel.SelectDefault();
  EXPECT_TRUE(panel.InspectorText().contains("Unavailable"));

  PlacementSizingDecisionEvidence decision;
  decision.site = "dl7";
  decision.decision_point = "end_of_global_placement";
  decision.boundary_slack_ps = -362.847;
  decision.has_boundary_slack = true;
  decision.current_pairs = 7;
  decision.requested_pairs = 10;
  decision.expected_added_components = 6;
  decision.expected_added_nets = 6;
  // Deliberately no actual growth and no final slack yet: this is the frame
  // published at the request, before the host has applied anything.
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {decision});

  const QString text = panel.InspectorText();
  EXPECT_TRUE(text.contains("end_of_global_placement"));
  EXPECT_TRUE(text.contains("-362.847"));
  EXPECT_TRUE(text.contains("7 -&gt; 10") || text.contains("7 -> 10"));
  EXPECT_TRUE(text.contains("Unavailable"))
      << "growth that has not happened yet must not be shown as a number";
}

TEST_F(TimingPanelTest, CompletedDecisionEvidenceIsShownInFull) {
  PlacementSizingDecisionEvidence decision;
  decision.site = "dl7";
  decision.decision_point = "end_of_global_placement";
  decision.boundary_slack_ps = -362.847;
  decision.has_boundary_slack = true;
  decision.current_pairs = 7;
  decision.requested_pairs = 10;
  decision.measured_response_ps = 462.736;
  decision.has_measured_response = true;
  decision.expected_added_components = 6;
  decision.expected_added_nets = 6;
  decision.actual_added_components = 6;
  decision.actual_added_nets = 6;
  decision.batch_added_components = 66;
  decision.batch_added_nets = 66;
  decision.batch_retired_nets = 0;
  decision.batch_rewired_nets = 8;
  decision.final_pairs = 10;
  decision.final_slack_ps = 79.4171;
  decision.has_final_slack = true;
  decision.closed = true;

  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {decision});
  panel.SelectDefault();

  const QString text = panel.InspectorText();
  EXPECT_TRUE(text.contains("+462.736"));
  EXPECT_TRUE(text.contains("closed"));
  EXPECT_TRUE(text.contains("+79.417"));
}

// --- topology evidence scope ------------------------------------------------
//
// Site-local additions and batch-global rewires describe different things.
// Every rewired net joins two neighbouring sites -- measured: c2 touches dl1
// and dl2 -- so no rewire belongs to one site, and a line reading
// "+6 cells / +6 nets, 8 rewired" invites reading 8 as this site's.

PlacementSizingDecisionEvidence AppliedDecision() {
  PlacementSizingDecisionEvidence decision;
  decision.site = "dl7";
  decision.decision_point = "end_of_global_placement";
  decision.boundary_slack_ps = -362.847;
  decision.has_boundary_slack = true;
  decision.current_pairs = 7;
  decision.requested_pairs = 10;
  decision.measured_response_ps = 462.736;
  decision.has_measured_response = true;
  decision.expected_added_components = 6;
  decision.expected_added_nets = 6;
  decision.actual_added_components = 6;
  decision.actual_added_nets = 6;
  decision.batch_added_components = 66;
  decision.batch_added_nets = 66;
  decision.batch_retired_nets = 0;
  decision.batch_rewired_nets = 8;
  return decision;
}

TEST_F(TimingPanelTest, SiteAndBatchTopologyScopesAreLabelledSeparately) {
  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {},
                      {AppliedDecision()});
  panel.SelectDefault();
  const QString text = panel.InspectorText();

  EXPECT_TRUE(text.contains("Site request"));
  EXPECT_TRUE(text.contains("Batch application"));
  EXPECT_TRUE(text.contains("expected growth"));
  EXPECT_TRUE(text.contains("applied growth"));
  EXPECT_TRUE(text.contains("retired / rewired"));
}

TEST_F(TimingPanelTest, TheBatchRewireCountIsNeverRenderedAsSiteLocal) {
  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {},
                      {AppliedDecision()});
  panel.SelectDefault();
  const QString text = panel.InspectorText();

  // The exact shape that mixed the scopes must not reappear in any form.
  EXPECT_FALSE(text.contains("8 rewired"));
  EXPECT_FALSE(text.contains("+6 cells / +6 nets, 8"));
  EXPECT_FALSE(text.contains("nets, 8 rewired"));
  // The batch numbers appear, in the batch block, as a pair.
  EXPECT_TRUE(text.contains("+66 cells / +66 nets"));
  EXPECT_TRUE(text.contains("0 / 8 nets"));
}

TEST_F(TimingPanelTest, MissingPerSiteAppliedEvidenceIsNotFabricated) {
  PlacementSizingDecisionEvidence requested = AppliedDecision();
  // The frame published at the request: nothing has been applied yet.
  requested.actual_added_components = -1;
  requested.actual_added_nets = -1;
  requested.batch_added_components = -1;
  requested.batch_added_nets = -1;
  requested.batch_retired_nets = -1;
  requested.batch_rewired_nets = -1;

  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {requested});
  panel.SelectDefault();
  const QString text = panel.InspectorText();

  EXPECT_TRUE(text.contains("expected growth"));
  EXPECT_TRUE(text.contains("+6 cells / +6 nets"));
  EXPECT_TRUE(text.contains("Unavailable"));
  EXPECT_FALSE(text.contains("+66"))
      << "batch totals that do not exist yet must not be invented";
}

TEST_F(TimingPanelTest, BatchTotalsAreShownExactlyAsRecorded) {
  PlacementSizingDecisionEvidence decision = AppliedDecision();
  decision.batch_added_components = 66;
  decision.batch_added_nets = 66;
  decision.batch_retired_nets = 0;
  decision.batch_rewired_nets = 8;

  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {decision});
  panel.SelectDefault();
  const QString text = panel.InspectorText();
  EXPECT_TRUE(text.contains("+66 cells / +66 nets"));
  EXPECT_TRUE(text.contains("0 / 8 nets"));
  // And the decision and timing values around them are untouched.
  EXPECT_TRUE(text.contains("final_placement"));
  EXPECT_TRUE(text.contains("-362.847"));
  EXPECT_TRUE(text.contains("+462.736"));
}

TEST_F(TimingPanelTest, UnattributedAndAmbiguousGroupsRemainVisible) {
  TimingDiagnosticsPanel panel;
  PlacementTimingPathVisualization orphan =
      Path(300, "a:Y|b:Y|c:Y", "", -5.0, 10.0, 5.0);
  PlacementTimingPathVisualization shared =
      Path(301, "d:Y|e:Y|f:Y", "", -6.0, 10.0, 4.0);
  shared.ambiguous_attribution = true;
  shared.candidate_delay_lines = {"dl0", "dl1"};

  panel.SetTimingData("final_placement", MeasuredLines(), {orphan}, {shared},
                      {});
  QStandardItemModel *model = panel.Model();
  ASSERT_EQ(model->rowCount(), 6);
  EXPECT_EQ(model->item(4, 0)->text(), "Unattributed");
  EXPECT_EQ(model->item(5, 0)->text(), "Ambiguous");
}

TEST_F(TimingPanelTest, EmptyDiagnosticGroupsAreNotShown) {
  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {});
  QStandardItemModel *model = panel.Model();
  EXPECT_EQ(model->rowCount(), 4);
  for (int row = 0; row < model->rowCount(); ++row) {
    EXPECT_FALSE(model->item(row, 0)->text() == "Unattributed");
    EXPECT_FALSE(model->item(row, 0)->text() == "Ambiguous");
  }
}

// Selection is a rendering operation. The panel's only outward effect is the
// callback that hands the drawn path to the canvas.
TEST_F(TimingPanelTest, SelectionOnlyEmitsTheRenderCallback) {
  TimingDiagnosticsPanel panel;
  int callbacks = 0;
  const PlacementTimingPathVisualization *last = nullptr;
  panel.on_selection = [&](const PlacementTimingPathVisualization *path) {
    ++callbacks;
    last = path;
  };
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {});
  EXPECT_EQ(callbacks, 0) << "arriving data selects nothing, so nothing renders";
  panel.SelectDefault();
  EXPECT_EQ(callbacks, 1);
  ASSERT_NE(last, nullptr);
  EXPECT_EQ(last->constraint_id, 128);

  ASSERT_TRUE(panel.SelectByIdentity("dl0", "loop__c:Y|dl0_ainv_525_6:Y|g0_562_6:Y"));
  EXPECT_EQ(callbacks, 2);
  ASSERT_NE(last, nullptr);
  EXPECT_EQ(last->constraint_id, 202);
  // The path handed over is the one Dali classified, unmodified.
  EXPECT_EQ(last->fast_only_component_ids, (std::vector<int>{1, 2}));
  EXPECT_EQ(last->slow_only_component_ids, (std::vector<int>{3, 4}));
  EXPECT_EQ(last->common_component_ids, (std::vector<int>{5}));
}


// --- rendering-only guarantees ---------------------------------------------
//
// These cover the three behaviours the pane promises beyond selection: that an
// ordinary run without timing data is untouched, that the diagnostics never
// take the canvas's space away entirely, and that fading is a paint change and
// nothing more.

TEST_F(TimingPanelTest, WithoutTimingDataThePanelStaysInertAndSilent) {
  TimingDiagnosticsPanel panel;
  int callbacks = 0;
  panel.on_selection = [&](const PlacementTimingPathVisualization *) {
    ++callbacks;
  };
  // Empty metadata, as an ordinary non-timing placement publishes.
  panel.SetTimingData("", {}, {}, {}, {});
  panel.SelectDefault();

  EXPECT_FALSE(panel.HasTimingData());
  EXPECT_EQ(panel.SelectedPath(), nullptr);
  EXPECT_EQ(panel.Model()->rowCount(), 0);
  EXPECT_EQ(callbacks, 0)
      << "a run with no timing data must not drive the canvas at all";
}

// A splitter that let the diagnostics take the whole width would be a viewer
// that lost its viewer.
TEST_F(TimingPanelTest, SplitterKeepsBothPanesVisible) {
  QWidget host;
  auto *splitter = new QSplitter(Qt::Horizontal, &host);
  auto *canvas_stand_in = new QWidget(splitter);
  auto *panel = new TimingDiagnosticsPanel(splitter);
  splitter->addWidget(canvas_stand_in);
  splitter->addWidget(panel);
  splitter->setChildrenCollapsible(false);
  host.resize(1200, 800);
  splitter->resize(1200, 800);
  host.show();
  QCoreApplication::processEvents();

  splitter->setSizes({900, 300});
  QCoreApplication::processEvents();
  EXPECT_GT(canvas_stand_in->width(), 0);
  EXPECT_GT(panel->width(), 0);

  // Drag the divider hard toward the canvas; neither side may vanish.
  splitter->setSizes({1, 1199});
  QCoreApplication::processEvents();
  EXPECT_GT(canvas_stand_in->width(), 0)
      << "the diagnostics pane collapsed the canvas";
  EXPECT_GT(panel->width(), 0);
  // And they tile rather than overlap.
  EXPECT_LE(canvas_stand_in->geometry().right(), panel->geometry().left() + 1);
}

TEST_F(TimingPanelTest, SelectionSurvivesARepeatedIdenticalSample) {
  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {});
  ASSERT_TRUE(panel.SelectByIdentity("dl0", "loop__c:Y|dl0_ainv_525_6:Y|g0_562_6:Y"));
  const std::string identity = panel.SelectedIdentity();

  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {});
  EXPECT_EQ(panel.SelectedIdentity(), identity)
      << "a repeated sample must not reset the user's selection";
  EXPECT_EQ(panel.SelectedConstraintId(), 202);
}

TEST_F(TimingPanelTest, SelectingByLineAndByIdReachTheSameState) {
  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {});

  ASSERT_TRUE(panel.SelectLineWorst("dl5"));
  const std::string by_line = panel.SelectedIdentity();
  EXPECT_EQ(panel.SelectedConstraintId(), 59);

  ASSERT_TRUE(panel.SelectByIdentity("dl0", "loop__c:Y|dl0_ainv_525_6:Y|g0_562_6:Y"));
  ASSERT_TRUE(panel.SelectConstraintId("dl5", 59));
  EXPECT_EQ(panel.SelectedIdentity(), by_line)
      << "the two capture entry points must land on one state";
}

TEST_F(TimingPanelTest, AnUnknownCaptureSelectionIsRefused) {
  TimingDiagnosticsPanel panel;
  panel.SetTimingData("final_placement", MeasuredLines(), {}, {}, {});
  const std::string before = panel.SelectedIdentity();

  EXPECT_FALSE(panel.SelectLineWorst("dl9"));
  EXPECT_FALSE(panel.SelectConstraintId("dl5", 9999));
  EXPECT_EQ(panel.SelectedIdentity(), before)
      << "a refused capture selection must leave the state alone";
}

} // namespace
} // namespace dali
