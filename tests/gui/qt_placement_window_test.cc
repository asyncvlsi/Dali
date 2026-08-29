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
/**
 * @file
 * The real placement window, not a stand-in.
 *
 * These drive the QtPlacementWindow the snapshot sink owns, through the same
 * SetSnapshot the sink calls, so what they assert is what a run does. The panel
 * tests beside them cover the tree and the inspector in isolation; these cover
 * what only the assembled window can answer: whether the Timing tab appears at
 * the right moment and only once, whether the splitter can starve either pane,
 * and whether Fade unrelated reaches the canvas through its real signal rather
 * than through a setter the test called itself.
 *
 * The fade is checked in both rendering layers, because it was correct in one
 * and absent from the other: FIXED cells are drawn as rectangles in every view
 * mode, and PLACED cells are drawn by the movable-dot layer that the GUI opens
 * in and that a real design is made of.
 */
#include "dali/gui/qt_placement_window.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QImage>
#include <QMouseEvent>
#include <QPushButton>
#include <QRect>
#include <QSplitter>
#include <QGroupBox>
#include <QLayout>
#include <QTabWidget>
#include <QTimer>

#include <chrono>
#include <cstddef>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "dali/circuit/circuit.h"

namespace dali {
namespace {

/**
 * Twelve cells on a 50x50 grid region, ids 0 through 11.
 *
 * `status` decides which rendering layer the cells land in: FIXED cells are
 * drawn as rectangles in every view mode, PLACED cells are movable and are
 * drawn by the dot layer while Movable dots is on -- which it is by default,
 * and which is therefore the view nearly every real run is looked at in.
 */
std::unique_ptr<Circuit> MakeFixtureCircuit(PlaceStatus status) {
  auto circuit = std::make_unique<Circuit>();
  circuit->SetDatabaseMicrons(1000);
  circuit->SetManufacturingGrid(0.001);
  circuit->AddMetalLayer("m1", 0.1, 0.1, 0.042, 0.2, 0.2, VERTICAL);
  circuit->AddMetalLayer("m2", 0.1, 0.1, 0.042, 0.2, 0.2, HORIZONTAL);
  circuit->SetGridValue(0.2, 0.2);
  circuit->SetRowHeight(0.2);

  Macro *cell = circuit->AddMacro("CELL", 0.8, 1.6);
  circuit->AddMacroPin(cell, "IN", true)->SetOffset(0.1, 0.8);
  circuit->AddMacroPin(cell, "OUT", false)->SetOffset(0.7, 0.8);

  circuit->SetUnitsDistanceMicrons(1000);
  circuit->SetDieArea(0, 0, 10000, 10000);
  circuit->ReserveSpaceForDesignImp(12, 0, 0);
  int index = 0;
  for (const int y : {2, 20, 38}) {
    for (const int x : {2, 14, 26, 38}) {
      circuit->AddComponent("c" + std::to_string(index++), "CELL", x, y, status,
                            N, true);
    }
  }
  return circuit;
}

PlacementTimingPathVisualization Path(int id, const std::string &identity,
                                      double slack) {
  PlacementTimingPathVisualization path;
  path.constraint_id = id;
  path.semantic_identity = identity;
  path.slack_ps = slack;
  path.fast_delay_ps = 1462.601;
  path.slow_delay_ps = 1462.601 + slack;
  path.has_geometry = true;
  path.fast_only_component_ids = {0, 1};
  path.slow_only_component_ids = {2, 3};
  path.common_component_ids = {4};
  path.fast_only_edges = {{4, 0, "n_f0"}, {0, 1, "n_f1"}};
  path.slow_only_edges = {{4, 2, "n_s0"}, {2, 3, "n_s1"}};
  path.common_edges = {{4, 4, "n_root"}};
  path.root_component_id = 4;
  path.fast_terminal_component_id = 1;
  path.slow_terminal_component_id = 3;
  return path;
}

PlacementSnapshotMetadata OrdinaryMetadata() {
  PlacementSnapshotMetadata metadata;
  metadata.id = "global_placement.upper_bound.3";
  metadata.group = "global_placement";
  metadata.subgroup = "upper_bound";
  metadata.iteration = 3;
  return metadata;
}

/** dl7 is globally worst, so it is what a default selection must land on. */
PlacementSnapshotMetadata TimingMetadata() {
  PlacementSnapshotMetadata metadata;
  metadata.id = "final";
  metadata.group = "detailed_placement";
  metadata.timing_sample_stage = "final_placement";

  PlacementDelayLineTimingVisualization dl2;
  dl2.delay_line_name = "dl2";
  dl2.constraints = {Path(114, "dl1_a:Y|dl2_a:Y|g2_51_6:Y", 106.282),
                     Path(77, "dl1_b:Y|dl2_b:Y|g2_77_6:Y", 109.290)};
  dl2.worst_constraint_id = 114;
  dl2.worst_slack_ps = 106.282;

  PlacementDelayLineTimingVisualization dl7;
  dl7.delay_line_name = "dl7";
  dl7.constraints = {Path(128, "dl6_a:Y|dl7_a:Y|h7_563_6:Y", 79.4171)};
  dl7.worst_constraint_id = 128;
  dl7.worst_slack_ps = 79.4171;

  metadata.delay_line_timing = {dl2, dl7};
  return metadata;
}

int TabCount(QTabWidget *tabs, const QString &label) {
  int count = 0;
  for (int index = 0; index < tabs->count(); ++index) {
    if (tabs->tabText(index) == label) ++count;
  }
  return count;
}

int TabIndex(QTabWidget *tabs, const QString &label) {
  for (int index = 0; index < tabs->count(); ++index) {
    if (tabs->tabText(index) == label) return index;
  }
  return -1;
}

std::set<QString> ButtonLabels(QWidget *window) {
  std::set<QString> labels;
  for (QPushButton *button : window->findChildren<QPushButton *>()) {
    labels.insert(button->text());
  }
  return labels;
}

/** A child's rectangle in the window's own coordinates. */
QRect RectInWindow(QWidget *window, QWidget *child) {
  return QRect(child->mapTo(window, QPoint(0, 0)), child->size());
}

/**
 * How many pixels two grabs of the same canvas disagree on.
 *
 * Counted rather than compared, because "the images differ" is also what a
 * single stray pixel of rendering noise would say, and a fade that reached one
 * pixel would be a fade that did not work.
 */
long DifferingPixels(const QImage &a, const QImage &b) {
  if (a.size() != b.size()) return -1;
  long differing = 0;
  for (int y = 0; y < a.height(); ++y) {
    for (int x = 0; x < a.width(); ++x) {
      if (a.pixel(x, y) != b.pixel(x, y)) ++differing;
    }
  }
  return differing;
}

std::map<QRgb, long> Histogram(const QImage &image) {
  std::map<QRgb, long> counts;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      ++counts[image.pixel(x, y)];
    }
  }
  return counts;
}

/**
 * Colours whose pixel count differs between two grabs, as (colour, before,
 * after).
 *
 * The canvas paints without antialiasing, so a change that only recolours one
 * class of cell shows up as exactly two entries here: the ink that class had,
 * and the ink it now has. Anything else that moved would add a third.
 */
std::vector<std::tuple<QRgb, long, long>> ColorChanges(const QImage &before,
                                                       const QImage &after) {
  const std::map<QRgb, long> a = Histogram(before);
  const std::map<QRgb, long> b = Histogram(after);
  std::set<QRgb> colors;
  for (const auto &entry : a) colors.insert(entry.first);
  for (const auto &entry : b) colors.insert(entry.first);
  std::vector<std::tuple<QRgb, long, long>> changes;
  for (const QRgb color : colors) {
    const long before_count = a.count(color) ? a.at(color) : 0;
    const long after_count = b.count(color) ? b.at(color) : 0;
    if (before_count != after_count) {
      changes.emplace_back(color, before_count, after_count);
    }
  }
  return changes;
}

class PlacementWindowTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    if (QApplication::instance() == nullptr) {
      static int argc = 1;
      static char name[] = "qt_placement_window_test";
      static char *argv[] = {name, nullptr};
      // Deliberately never destroyed: a QApplication torn down during static
      // destruction crashes after every test has already passed.
      new QApplication(argc, argv);
    }
  }

  void SetUp() override { circuit_ = MakeFixtureCircuit(FIXED); }

  /** Show the window the way the sink shows it, then settle the event queue. */
  void Prepare(QtPlacementWindow *window, int width = 1500, int height = 950) {
    window->resize(width, height);
    window->show();
    QCoreApplication::processEvents();
  }

  std::unique_ptr<Circuit> circuit_;
};

// --- the Timing tab appears only when timing data does ----------------------

TEST_F(PlacementWindowTest, OrdinaryPlacementKeepsTheWindowItAlwaysHad) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), OrdinaryMetadata());
  QCoreApplication::processEvents();

  QTabWidget *tabs = window.DiagnosticsTabs();
  ASSERT_NE(tabs, nullptr);
  EXPECT_EQ(TabCount(tabs, "HPWL"), 1);
  EXPECT_EQ(tabs->count(), 1);
  EXPECT_EQ(TabIndex(tabs, "Timing"), -1)
      << "an ordinary placement run must look exactly as it did";
  EXPECT_EQ(window.TimingTabIndex(), -1);
  EXPECT_FALSE(window.ShowTimingTab());
  EXPECT_FALSE(window.TimingPanel()->HasTimingData());

  ASSERT_NE(window.Canvas(), nullptr);
  EXPECT_TRUE(window.Canvas()->isVisible());
  EXPECT_GT(window.Canvas()->width(), 0);
  EXPECT_GT(window.Canvas()->height(), 0);

  const std::set<QString> buttons = ButtonLabels(&window);
  for (const QString &label : {"Step", "Continue", "Fit", "Save PNG"}) {
    EXPECT_EQ(buttons.count(label), 1u)
        << "missing control: " << label.toStdString();
  }
  ASSERT_NE(window.FadeUnrelatedCheckbox(), nullptr);
  EXPECT_FALSE(window.FadeUnrelatedCheckbox()->isEnabled())
      << "the path toggles have nothing to act on before timing data arrives";
}

TEST_F(PlacementWindowTest, TimingTabAppearsOnceWhenTimingDataArrives) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), OrdinaryMetadata());
  QCoreApplication::processEvents();
  QTabWidget *tabs = window.DiagnosticsTabs();
  ASSERT_EQ(TabIndex(tabs, "Timing"), -1);

  window.SetSnapshot(circuit_.get(), TimingMetadata());
  QCoreApplication::processEvents();

  EXPECT_EQ(TabCount(tabs, "Timing"), 1);
  EXPECT_EQ(TabCount(tabs, "HPWL"), 1);
  EXPECT_EQ(tabs->count(), 2);
  EXPECT_EQ(window.TimingTabIndex(), TabIndex(tabs, "Timing"));
  EXPECT_TRUE(window.ShowTimingTab());

  // Nothing is selected until somebody selects it.
  TimingDiagnosticsPanel *panel = window.TimingPanel();
  EXPECT_TRUE(panel->HasTimingData());
  EXPECT_TRUE(panel->SelectedLine().empty());
  EXPECT_TRUE(panel->SelectedIdentity().empty());
  EXPECT_EQ(panel->SelectedConstraintId(), -1);
  EXPECT_EQ(panel->SelectedPath(), nullptr);
  EXPECT_FALSE(window.Canvas()->HasHighlight())
      << "the viewer highlighted a constraint nobody chose";

  // Repeated samples must not stack tabs, and must not lose the selection.
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  QCoreApplication::processEvents();
  EXPECT_EQ(TabCount(tabs, "Timing"), 1);
  EXPECT_EQ(TabCount(tabs, "HPWL"), 1);
  EXPECT_EQ(tabs->count(), 2);
  EXPECT_EQ(panel->SelectedConstraintId(), -1);
}

// --- the splitter cannot starve either side ---------------------------------

TEST_F(PlacementWindowTest, SplitterKeepsBothPanesUsableAtEveryWidth) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  QCoreApplication::processEvents();

  QSplitter *splitter = window.DiagnosticsSplitter();
  ASSERT_NE(splitter, nullptr);
  QWidget *canvas = window.Canvas();
  QWidget *diagnostics = window.DiagnosticsTabs();
  QWidget *toggle = window.FadeUnrelatedCheckbox();

  // The minimums below are read off the widgets, not written down here, so a
  // pane that had no minimum at all would make every check that uses one pass
  // for free. Establish first that there is something to check.
  ASSERT_GT(canvas->minimumWidth(), 0);
  ASSERT_GT(canvas->minimumHeight(), 0);
  ASSERT_GT(diagnostics->minimumSizeHint().width(), 0);

  // Expectations come from the widgets themselves, so this does not encode one
  // machine's pixels. 1000 is narrower than the two minimums can add up to: the
  // window is expected to refuse that width rather than starve a pane.
  for (const int requested : {1000, 1500, 1920}) {
    window.resize(requested, 950);
    QCoreApplication::processEvents();
    const int width = window.width();
    SCOPED_TRACE("requested width " + std::to_string(requested) + ", actual " +
                 std::to_string(width));

    // Zero, not one: a size of zero is what asks a QSplitter to collapse a
    // child, and refusing it is the property this window relies on.
    for (const QList<int> &sizes :
         {QList<int>{0, width}, QList<int>{width, 0},
          QList<int>{width * 3 / 4, width / 4}}) {
      splitter->setSizes(sizes);
      QCoreApplication::processEvents();

      EXPECT_GE(canvas->width(), canvas->minimumWidth());
      EXPECT_GE(canvas->height(), canvas->minimumHeight());
      EXPECT_GE(diagnostics->width(), diagnostics->minimumSizeHint().width());
      EXPECT_GT(diagnostics->height(), 0);
      EXPECT_TRUE(canvas->isVisible());
      EXPECT_TRUE(diagnostics->isVisible());

      // They tile rather than stack: no overlap, both inside the window.
      const QRect canvas_rect = RectInWindow(&window, canvas);
      const QRect diagnostics_rect = RectInWindow(&window, diagnostics);
      EXPECT_FALSE(canvas_rect.intersects(diagnostics_rect))
          << "the diagnostics pane floats over the canvas";
      EXPECT_TRUE(window.rect().contains(canvas_rect));
      EXPECT_TRUE(window.rect().contains(diagnostics_rect));

      // The global controls below stay visible and clear of both panes. The
      // path toggles and the clear-selection button are deliberately not among
      // them any more: they are children of the Timing pane, so they are
      // inside the diagnostics rectangle by design.
      for (QPushButton *button : window.findChildren<QPushButton *>()) {
        if (window.DiagnosticsTabs()->isAncestorOf(button)) continue;
        const QRect button_rect = RectInWindow(&window, button);
        EXPECT_TRUE(button->isVisible()) << button->text().toStdString();
        EXPECT_TRUE(window.rect().contains(button_rect))
            << button->text().toStdString() << " is off the window";
        EXPECT_FALSE(button_rect.intersects(canvas_rect));
        EXPECT_FALSE(button_rect.intersects(diagnostics_rect));
      }
      EXPECT_TRUE(window.DiagnosticsTabs()->isAncestorOf(toggle))
          << "the fade toggle belongs to the Timing pane";
    }
  }
}

// --- fade unrelated is a paint change and nothing else ----------------------

/*
  The view a real run is actually looked at in: movable cells, Movable dots on.

  This is where the toggle used to do nothing. The dot layer classified cells
  by delay-line membership alone, so on any ordinary design -- where the cells
  are movable -- Fade unrelated subdued the well taps and the end caps and left
  the design untouched. The fixed-cell test below covers the rectangle layer;
  this one covers the layer the default view uses.
*/
TEST_F(PlacementWindowTest, FadeUnrelatedSubduesMovableDots) {
  circuit_ = MakeFixtureCircuit(PLACED);
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  QCoreApplication::processEvents();

  TimingDiagnosticsPanel *panel = window.TimingPanel();
  PlacementCanvas *canvas = window.Canvas();
  ASSERT_TRUE(canvas->MovableDotMode())
      << "the default view draws dots; this test is about that view";
  ASSERT_TRUE(panel->SelectByIdentity("dl2", "dl1_a:Y|dl2_a:Y|g2_51_6:Y"));
  QCoreApplication::processEvents();

  // The fixture's path is fast {0,1}, slow {2,3}, shared {4}; 5..11 unrelated.
  ASSERT_EQ(canvas->ClassOf(0), PlacementCanvas::PathClass::kFast);
  ASSERT_EQ(canvas->ClassOf(2), PlacementCanvas::PathClass::kSlow);
  ASSERT_EQ(canvas->ClassOf(4), PlacementCanvas::PathClass::kShared);
  ASSERT_EQ(canvas->ClassOf(7), PlacementCanvas::PathClass::kNone);

  const std::string identity = panel->SelectedIdentity();
  const int constraint_id = panel->SelectedConstraintId();
  const QString inspector = panel->InspectorPlainText();
  const PlacementTimingPathVisualization *before = panel->SelectedPath();
  ASSERT_NE(before, nullptr);
  const std::vector<int> fast = before->fast_only_component_ids;
  const std::vector<int> slow = before->slow_only_component_ids;
  const std::vector<int> shared = before->common_component_ids;
  const QColor fast_ink = canvas->DotFillForComponent(0);
  const QColor slow_ink = canvas->DotFillForComponent(2);
  const QColor shared_ink = canvas->DotFillForComponent(4);
  const QColor unrelated_ink = canvas->DotFillForComponent(7);
  ASSERT_TRUE(fast_ink.isValid() && unrelated_ink.isValid());
  std::vector<PlacementCanvas::PathClass> classes;
  for (int id = 0; id < 12; ++id) classes.push_back(canvas->ClassOf(id));
  const int rows = panel->Model()->rowCount();
  const int tab_count = window.DiagnosticsTabs()->count();

  int selection_callbacks = 0;
  auto window_handler = panel->on_selection;
  panel->on_selection = [&](const PlacementTimingPathVisualization *path) {
    ++selection_callbacks;
    if (window_handler) window_handler(path);
  };

  QCheckBox *fade = window.FadeUnrelatedCheckbox();
  ASSERT_FALSE(canvas->FadeUnrelated());
  const QImage unfaded = canvas->grab().toImage();

  fade->setChecked(true);
  QCoreApplication::processEvents();
  const QImage faded = canvas->grab().toImage();

  // 1. The picture really moved.
  EXPECT_GT(DifferingPixels(unfaded, faded), 100)
      << "fading unrelated movable cells barely touched the picture";

  // 2. What moved is the unrelated dots' ink, and only that. Exactly two
  // colours change count: the ink those dots had, and the ink they now have.
  const auto changes = ColorChanges(unfaded, faded);
  ASSERT_EQ(changes.size(), 2u)
      << "fading recoloured something other than the unrelated dots";
  const long lost = std::get<1>(changes[0]) - std::get<2>(changes[0]);
  const long gained = std::get<2>(changes[1]) - std::get<1>(changes[1]);
  EXPECT_GT(lost, 0);
  EXPECT_EQ(lost, gained) << "the unrelated dots changed colour, not size";

  // 3. The dot layer subdued the unrelated cells and left the path alone.
  EXPECT_NE(canvas->DotFillForComponent(7), unrelated_ink)
      << "an unrelated movable dot kept its ordinary ink";
  EXPECT_EQ(canvas->DotFillForComponent(0), fast_ink) << "fast dot lost blue";
  EXPECT_EQ(canvas->DotFillForComponent(2), slow_ink) << "slow dot lost orange";
  EXPECT_EQ(canvas->DotFillForComponent(4), shared_ink)
      << "shared dot lost purple";
  // Subdued, not hidden: still ink on the canvas, just less of it.
  EXPECT_GT(canvas->DotFillForComponent(7).alpha(), 0);

  // 4-5. Selection, geometry and inspector are untouched.
  const PlacementTimingPathVisualization *after = panel->SelectedPath();
  ASSERT_NE(after, nullptr);
  EXPECT_EQ(panel->SelectedIdentity(), identity);
  EXPECT_EQ(panel->SelectedConstraintId(), constraint_id);
  EXPECT_EQ(panel->InspectorPlainText(), inspector);
  EXPECT_EQ(after->fast_only_component_ids, fast);
  EXPECT_EQ(after->slow_only_component_ids, slow);
  EXPECT_EQ(after->common_component_ids, shared);
  for (int id = 0; id < 12; ++id) {
    EXPECT_EQ(canvas->ClassOf(id), classes[id])
        << "component " << id << " changed path class";
  }

  // 6. Off again, exactly the picture it started from.
  fade->setChecked(false);
  QCoreApplication::processEvents();
  EXPECT_FALSE(canvas->FadeUnrelated());
  EXPECT_EQ(DifferingPixels(canvas->grab().toImage(), unfaded), 0);
  EXPECT_EQ(canvas->DotFillForComponent(7), unrelated_ink);

  // 7. Nothing downstream ran.
  EXPECT_EQ(selection_callbacks, 0)
      << "a rendering toggle must not re-select anything";
  EXPECT_EQ(panel->Model()->rowCount(), rows);
  EXPECT_EQ(window.DiagnosticsTabs()->count(), tab_count);
  panel->on_selection = window_handler;
}

TEST_F(PlacementWindowTest, FadeUnrelatedChangesRenderingOnly) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  QCoreApplication::processEvents();

  TimingDiagnosticsPanel *panel = window.TimingPanel();
  PlacementCanvas *canvas = window.Canvas();
  ASSERT_TRUE(panel->SelectByIdentity("dl2", "dl1_a:Y|dl2_a:Y|g2_51_6:Y"));
  QCoreApplication::processEvents();

  // Everything that must survive the toggle, recorded first.
  const std::string identity = panel->SelectedIdentity();
  const std::string line = panel->SelectedLine();
  const int constraint_id = panel->SelectedConstraintId();
  const QString inspector = panel->InspectorPlainText();
  const PlacementTimingPathVisualization *before = panel->SelectedPath();
  ASSERT_NE(before, nullptr);
  const std::vector<int> fast = before->fast_only_component_ids;
  const std::vector<int> slow = before->slow_only_component_ids;
  const std::vector<int> shared = before->common_component_ids;
  const std::size_t fast_edges = before->fast_only_edges.size();
  const std::size_t slow_edges = before->slow_only_edges.size();
  const int rows = panel->Model()->rowCount();
  const int tab_count = window.DiagnosticsTabs()->count();
  std::vector<PlacementCanvas::PathClass> classes;
  for (int id = 0; id < 12; ++id) classes.push_back(canvas->ClassOf(id));

  // A re-selection would run the window's own handler, so count it rather than
  // replace it: replacing it would hide the very wiring under test.
  int selection_callbacks = 0;
  auto window_handler = panel->on_selection;
  panel->on_selection = [&](const PlacementTimingPathVisualization *path) {
    ++selection_callbacks;
    if (window_handler) window_handler(path);
  };

  QCheckBox *fade = window.FadeUnrelatedCheckbox();
  ASSERT_FALSE(fade->isChecked());
  ASSERT_FALSE(canvas->FadeUnrelated());
  const QImage unfaded = canvas->grab().toImage();

  // Through the real checkbox: this is the path a user takes, and the only one
  // that proves the signal is connected.
  fade->setChecked(true);
  QCoreApplication::processEvents();
  const QImage faded = canvas->grab().toImage();

  EXPECT_TRUE(canvas->FadeUnrelated())
      << "the checkbox never reached the canvas";
  EXPECT_GT(DifferingPixels(unfaded, faded), 100)
      << "fading unrelated cells barely touched the picture";

  // The selection, its geometry, and the panel around it are untouched.
  EXPECT_EQ(panel->SelectedIdentity(), identity);
  EXPECT_EQ(panel->SelectedLine(), line);
  EXPECT_EQ(panel->SelectedConstraintId(), constraint_id);
  EXPECT_EQ(panel->InspectorPlainText(), inspector);
  EXPECT_EQ(panel->Model()->rowCount(), rows);
  EXPECT_EQ(window.DiagnosticsTabs()->count(), tab_count);
  const PlacementTimingPathVisualization *after = panel->SelectedPath();
  ASSERT_NE(after, nullptr);
  EXPECT_EQ(after->fast_only_component_ids, fast);
  EXPECT_EQ(after->slow_only_component_ids, slow);
  EXPECT_EQ(after->common_component_ids, shared);
  EXPECT_EQ(after->fast_only_edges.size(), fast_edges);
  EXPECT_EQ(after->slow_only_edges.size(), slow_edges);
  for (int id = 0; id < 12; ++id) {
    EXPECT_EQ(canvas->ClassOf(id), classes[id])
        << "component " << id << " changed path class";
  }
  EXPECT_EQ(selection_callbacks, 0)
      << "a rendering toggle must not re-select anything";

  // Off again, deterministically back to the original picture.
  fade->setChecked(false);
  QCoreApplication::processEvents();
  EXPECT_FALSE(canvas->FadeUnrelated());
  EXPECT_EQ(DifferingPixels(canvas->grab().toImage(), unfaded), 0)
      << "the picture did not come back to what it was";
  EXPECT_EQ(panel->SelectedIdentity(), identity);
  EXPECT_EQ(panel->SelectedConstraintId(), constraint_id);
  EXPECT_EQ(selection_callbacks, 0);

  panel->on_selection = window_handler;
}

// --- a finished window does not hold a machine hostage ----------------------

/*
  A finished run used to wait on its window forever, so an unattended run sat
  on its final frame until somebody walked to the machine. These cover the way
  out: the window closes itself once nobody has touched it for the idle limit,
  and any sign of a person restarts the clock.
*/
TEST_F(PlacementWindowTest, IdleClockRestartsOnUserInput) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  QCoreApplication::processEvents();

  EXPECT_DOUBLE_EQ(QtPlacementWindow::kDefaultIdleCloseSeconds, 60.0)
      << "the shipped wait is one minute";

  window.NoteUserActivity();
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  const double idled = window.IdleSeconds();
  EXPECT_GT(idled, 0.05) << "the idle clock is not running";

  // A real event, delivered the way Qt delivers one, so this exercises the
  // application-wide filter rather than the bookkeeping behind it.
  QMouseEvent move(QEvent::MouseMove, QPointF(10, 10), QPointF(10, 10),
                   Qt::NoButton, Qt::NoButton, Qt::NoModifier);
  QCoreApplication::sendEvent(window.Canvas(), &move);
  EXPECT_LT(window.IdleSeconds(), idled)
      << "a mouse move over the canvas did not restart the idle clock";

  // An event that is not a person must not count.
  window.NoteUserActivity();
  std::this_thread::sleep_for(std::chrono::milliseconds(80));
  const double before_paint = window.IdleSeconds();
  QEvent paint(QEvent::UpdateRequest);
  QCoreApplication::sendEvent(window.Canvas(), &paint);
  EXPECT_GE(window.IdleSeconds(), before_paint)
      << "a repaint was mistaken for a user";
}

TEST_F(PlacementWindowTest, FinishedWindowClosesItselfWhenNobodyIsThere) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  window.SetIdleCloseSeconds(0.15);
  window.MarkFinished();
  ASSERT_TRUE(window.isVisible());

  const auto started = std::chrono::steady_clock::now();
  window.WaitUntilClosedOrIdle();
  const double waited =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
          .count();

  EXPECT_FALSE(window.isVisible()) << "the window is still holding the run";
  EXPECT_GE(waited, 0.15) << "it gave up before the limit it was given";
  EXPECT_LT(waited, 5.0) << "it did not give up at all";
}

TEST_F(PlacementWindowTest, SomebodyAtTheScreenKeepsTheWindowOpen) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  window.SetIdleCloseSeconds(0.15);
  window.MarkFinished();

  // Somebody moving the mouse every 30 ms for the first 600 ms -- four times
  // the idle limit -- then leaving. The window must outlast the fidgeting and
  // close on its own afterwards, not at the first 150 ms.
  const auto started = std::chrono::steady_clock::now();
  QTimer activity;
  activity.setInterval(30);
  QObject::connect(&activity, &QTimer::timeout, &window, [&]() {
    if (std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                      started)
            .count() < 0.6) {
      QMouseEvent move(QEvent::MouseMove, QPointF(20, 20), QPointF(20, 20),
                       Qt::NoButton, Qt::NoButton, Qt::NoModifier);
      QCoreApplication::sendEvent(window.Canvas(), &move);
    } else {
      activity.stop();
    }
  });
  activity.start();

  window.WaitUntilClosedOrIdle();
  const double waited =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started)
          .count();

  EXPECT_FALSE(window.isVisible());
  EXPECT_GE(waited, 0.6)
      << "the window closed while somebody was still using it";
  EXPECT_LT(waited, 5.0);
}


// --- selection is something a person does -----------------------------------

/*
  The viewer used to select the globally worst constraint the moment timing
  data arrived. That put a conclusion on the screen the run had not reached,
  and left no way to tell an automatic highlight apart from one an operator
  chose. These cover the replacement: the tab fills, nothing is selected, and
  the path toggles stay dead until a selection exists.
*/
TEST_F(PlacementWindowTest, TimingDataSelectsNothing) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  QCoreApplication::processEvents();
  TimingDiagnosticsPanel *panel = window.TimingPanel();

  EXPECT_TRUE(panel->HasTimingData());
  EXPECT_GT(panel->Model()->rowCount(), 0) << "the tab did not populate";
  EXPECT_EQ(panel->SelectedPath(), nullptr);
  EXPECT_TRUE(panel->SelectedIdentity().empty());
  EXPECT_EQ(panel->InspectorPlainText().trimmed(), "No constraint selected");
  EXPECT_FALSE(window.Canvas()->HasHighlight());
  EXPECT_FALSE(panel->PathControlsEnabled());
  for (QCheckBox *box : {panel->FadeUnrelatedCheckbox(), panel->FastPathCheckbox(),
                         panel->SlowPathCheckbox(), panel->SharedPathCheckbox()}) {
    EXPECT_FALSE(box->isEnabled()) << box->text().toStdString();
  }
  EXPECT_FALSE(panel->ClearSelectionButton()->isEnabled());
}

TEST_F(PlacementWindowTest, SelectingEnablesControlsAndHighlights) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  QCoreApplication::processEvents();
  TimingDiagnosticsPanel *panel = window.TimingPanel();

  ASSERT_TRUE(panel->SelectByIdentity("dl2", "dl1_a:Y|dl2_a:Y|g2_51_6:Y"));
  QCoreApplication::processEvents();

  EXPECT_EQ(panel->SelectedConstraintId(), 114);
  EXPECT_TRUE(window.Canvas()->HasHighlight());
  EXPECT_TRUE(panel->PathControlsEnabled());
  EXPECT_TRUE(panel->ClearSelectionButton()->isEnabled());
}

TEST_F(PlacementWindowTest, ClearSelectionRemovesHighlightAndDisables) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  TimingDiagnosticsPanel *panel = window.TimingPanel();
  ASSERT_TRUE(panel->SelectByIdentity("dl2", "dl1_a:Y|dl2_a:Y|g2_51_6:Y"));
  QCoreApplication::processEvents();
  ASSERT_TRUE(window.Canvas()->HasHighlight());

  // Through the real button, so the wiring is what is exercised.
  panel->ClearSelectionButton()->click();
  QCoreApplication::processEvents();

  EXPECT_EQ(panel->SelectedPath(), nullptr);
  EXPECT_TRUE(panel->SelectedIdentity().empty());
  EXPECT_FALSE(window.Canvas()->HasHighlight())
      << "clearing left the canvas highlighted";
  EXPECT_EQ(panel->InspectorPlainText().trimmed(), "No constraint selected");
  EXPECT_FALSE(panel->PathControlsEnabled());
  EXPECT_EQ(panel->Tree()->currentIndex(), QModelIndex());
}

TEST_F(PlacementWindowTest, ExplicitCaptureSelectionsUseTheClickPath) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  TimingDiagnosticsPanel *panel = window.TimingPanel();

  ASSERT_TRUE(panel->SelectLineWorst("dl0") == false)
      << "dl0 is not in this fixture; the call must decline";
  ASSERT_TRUE(panel->SelectLineWorst("dl7"));
  QCoreApplication::processEvents();
  EXPECT_EQ(panel->SelectedConstraintId(), 128);
  EXPECT_TRUE(window.Canvas()->HasHighlight());
  EXPECT_TRUE(panel->PathControlsEnabled());

  ASSERT_TRUE(panel->SelectConstraintId("dl2", 114));
  QCoreApplication::processEvents();
  EXPECT_EQ(panel->SelectedConstraintId(), 114);
  EXPECT_EQ(panel->SelectedIdentity(), "dl1_a:Y|dl2_a:Y|g2_51_6:Y");
  EXPECT_TRUE(window.Canvas()->HasHighlight());
}

TEST_F(PlacementWindowTest, SelectionSurvivesRenumberingByIdentity) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  TimingDiagnosticsPanel *panel = window.TimingPanel();
  ASSERT_TRUE(panel->SelectByIdentity("dl2", "dl1_a:Y|dl2_a:Y|g2_51_6:Y"));

  // Same identity, different numeric id, as ACT re-elaboration produces.
  PlacementSnapshotMetadata renumbered = TimingMetadata();
  renumbered.delay_line_timing[0].constraints[0].constraint_id = 901;
  renumbered.delay_line_timing[0].worst_constraint_id = 901;
  window.SetSnapshot(circuit_.get(), renumbered);
  QCoreApplication::processEvents();

  EXPECT_EQ(panel->SelectedIdentity(), "dl1_a:Y|dl2_a:Y|g2_51_6:Y");
  EXPECT_EQ(panel->SelectedConstraintId(), 901);
}

TEST_F(PlacementWindowTest, MissingIdentityFallsBackWithinItsLine) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  TimingDiagnosticsPanel *panel = window.TimingPanel();
  ASSERT_TRUE(panel->SelectByIdentity("dl2", "dl1_b:Y|dl2_b:Y|g2_77_6:Y"));

  PlacementSnapshotMetadata without = TimingMetadata();
  without.delay_line_timing[0].constraints.erase(
      without.delay_line_timing[0].constraints.begin() + 1);
  window.SetSnapshot(circuit_.get(), without);
  QCoreApplication::processEvents();

  EXPECT_EQ(panel->SelectedLine(), "dl2") << "the line should have been kept";
  EXPECT_EQ(panel->SelectedConstraintId(), 114)
      << "fall back to this line's worst, not to whatever holds the old number";
  EXPECT_TRUE(panel->PathControlsEnabled());
}

TEST_F(PlacementWindowTest, MissingLineClearsSelection) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  TimingDiagnosticsPanel *panel = window.TimingPanel();
  ASSERT_TRUE(panel->SelectByIdentity("dl2", "dl1_a:Y|dl2_a:Y|g2_51_6:Y"));

  PlacementSnapshotMetadata without = TimingMetadata();
  without.delay_line_timing.erase(without.delay_line_timing.begin());
  window.SetSnapshot(circuit_.get(), without);
  QCoreApplication::processEvents();

  EXPECT_EQ(panel->SelectedPath(), nullptr);
  EXPECT_TRUE(panel->SelectedLine().empty());
  EXPECT_FALSE(window.Canvas()->HasHighlight());
  EXPECT_FALSE(panel->PathControlsEnabled());
}

// --- layout -----------------------------------------------------------------

/** A snapshot whose full status names all eight sites and their slacks. */
PlacementSnapshotMetadata LongTopologyMetadata() {
  PlacementSnapshotMetadata metadata = TimingMetadata();
  metadata.topology_generation = 1;
  for (int site = 0; site < 8; ++site) {
    PlacementTopologySiteChange change;
    change.site = "dl" + std::to_string(site);
    change.current_pairs = 7;
    change.requested_pairs = 13;
    change.has_boundary_slack = true;
    change.boundary_slack_ps = -768.577123;
    metadata.topology_changes.push_back(change);
  }
  return metadata;
}

TEST_F(PlacementWindowTest, LongStatusDoesNotResizeTheWindowOrMoveTheSplitter) {
  QtPlacementWindow window;
  Prepare(&window, 1400, 900);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  QCoreApplication::processEvents();
  const int width_before = window.width();
  const QList<int> splitter_before = window.DiagnosticsSplitter()->sizes();
  const int hint_before = window.StatusLabel()->minimumSizeHint().width();

  window.SetSnapshot(circuit_.get(), LongTopologyMetadata());
  QCoreApplication::processEvents();

  EXPECT_EQ(window.width(), width_before)
      << "a long status changed the window width";
  EXPECT_EQ(window.DiagnosticsSplitter()->sizes(), splitter_before)
      << "a long status moved the splitter";
  EXPECT_EQ(window.StatusLabel()->minimumSizeHint().width(), hint_before);
  EXPECT_EQ(window.StatusLabel()->minimumSizeHint().width(), 0)
      << "the status still asks for room proportional to its text";
}

TEST_F(PlacementWindowTest, StatusIsCompactButKeepsEveryDetail) {
  QtPlacementWindow window;
  Prepare(&window, 1400, 900);
  window.SetSnapshot(circuit_.get(), LongTopologyMetadata());
  QCoreApplication::processEvents();

  const QString shown = window.StatusLabel()->text();
  const QString full = window.StatusLabel()->FullStatus();

  EXPECT_LT(shown.size(), full.size()) << "nothing was compacted";
  EXPECT_TRUE(shown.contains("topology generation 1"));
  EXPECT_TRUE(shown.contains("8 site changes"));
  EXPECT_FALSE(shown.contains("-768.577"))
      << "the compact line is listing individual slacks";
  EXPECT_FALSE(shown.contains('\n')) << "the status must stay one line";
  // Nothing is discarded: every site and its slack survives in the detail.
  for (int site = 0; site < 8; ++site) {
    EXPECT_TRUE(full.contains(QString("dl%1 7 -> 13").arg(site)))
        << "site " << site << " is missing from the full status";
  }
  EXPECT_TRUE(full.contains("-768.577"));
  EXPECT_EQ(window.StatusLabel()->toolTip(), full);
  EXPECT_EQ(window.StatusLabel()->accessibleDescription(), full);
}

TEST_F(PlacementWindowTest, ControlsAreGroupedRatherThanOneLongRow) {
  QtPlacementWindow window;
  Prepare(&window, 1400, 900);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  QCoreApplication::processEvents();

  // Count the global control rows: horizontal layouts holding a bold group
  // label. There must be more than one, and none may hold everything.
  int rows = 0;
  int widest_row = 0;
  for (QHBoxLayout *row : window.findChildren<QHBoxLayout *>()) {
    if (window.DiagnosticsTabs()->isAncestorOf(row->parentWidget())) continue;
    int controls = 0;
    for (int index = 0; index < row->count(); ++index) {
      if (row->itemAt(index)->widget() != nullptr) ++controls;
    }
    if (controls > 0) {
      ++rows;
      widest_row = std::max(widest_row, controls);
    }
  }
  EXPECT_GE(rows, 3) << "the global controls are not split into groups";

  const int global_controls =
      static_cast<int>(window.findChildren<QCheckBox *>().size() +
                       window.findChildren<QPushButton *>().size());
  EXPECT_LT(widest_row, global_controls)
      << "one row still holds every checkbox and button";
}

TEST_F(PlacementWindowTest, TimingControlsBelongToTheTimingPane) {
  QtPlacementWindow window;
  Prepare(&window);
  window.SetSnapshot(circuit_.get(), TimingMetadata());
  QCoreApplication::processEvents();
  TimingDiagnosticsPanel *panel = window.TimingPanel();

  for (QWidget *control :
       {static_cast<QWidget *>(panel->FadeUnrelatedCheckbox()),
        static_cast<QWidget *>(panel->FastPathCheckbox()),
        static_cast<QWidget *>(panel->SlowPathCheckbox()),
        static_cast<QWidget *>(panel->SharedPathCheckbox()),
        static_cast<QWidget *>(panel->ClearSelectionButton()),
        static_cast<QWidget *>(panel->DelayLineCheckbox()),
        static_cast<QWidget *>(panel->TopologyAddedCheckbox())}) {
    EXPECT_TRUE(panel->isAncestorOf(control))
        << "a timing control is not a child of the Timing pane";
  }
  // The layer toggles describe the design, not a selection, so unlike the path
  // toggles they stay usable with nothing selected.
  EXPECT_FALSE(panel->PathControlsEnabled());
  EXPECT_TRUE(panel->DelayLineCheckbox()->isEnabled());
  EXPECT_TRUE(panel->TopologyAddedCheckbox()->isEnabled());

  // And they still reach the canvas through the pane's own signal.
  const bool before = window.Canvas()->MovableDotMode();
  panel->DelayLineCheckbox()->setChecked(!panel->DelayLineCheckbox()->isChecked());
  QCoreApplication::processEvents();
  EXPECT_EQ(window.Canvas()->MovableDotMode(), before)
      << "toggling a layer disturbed an unrelated view mode";
  EXPECT_EQ(window.FadeUnrelatedCheckbox(), panel->FadeUnrelatedCheckbox())
      << "the window accessor must forward to the pane's own control";
}

} // namespace
} // namespace dali
