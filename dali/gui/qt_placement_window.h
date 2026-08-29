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
 * The live placement window and the widgets it is built from.
 *
 * Internal to `daligui`: not installed, and not part of Dali's public API.
 * It exists so the snapshot sink and the GUI tests reach the one real
 * window, rather than a test building a second one that drifts from it.
 *
 * Moved here unchanged from qt_placement_snapshot_sink.cc.
 */
#ifndef DALI_GUI_QT_PLACEMENT_WINDOW_H_
#define DALI_GUI_QT_PLACEMENT_WINDOW_H_

#include <QColor>
#include <QRectF>
#include <QString>
#include <QWidget>
#include <chrono>
#include <string>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/gui/qt_placement_snapshot_sink.h"
#include "dali/gui/qt_timing_diagnostics_panel.h"

class QCheckBox;
class QLabel;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QPushButton;
class QResizeEvent;
class QSlider;
class QSpinBox;
class QSplitter;
class QTabWidget;
class QWheelEvent;

namespace dali {

enum class SnapshotComponentKind {
  kOrdinary,
  kWellTap,
  kEndCap,
};

struct SnapshotComponent {
  int component_id = -1;
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;
  bool fixed = false;
  ComponentOrient orient = N;
  SnapshotComponentKind kind = SnapshotComponentKind::kOrdinary;
  bool is_delay_line = false;
  bool is_topology_added = false;
};

struct SnapshotWellRect {
  float lx = 0;
  float ly = 0;
  float ux = 0;
  float uy = 0;
  PlacementWellLayer layer = PlacementWellLayer::kPwell;
};

struct SnapshotIoPin {
  float lx = 0;
  float ly = 0;
  float ux = 0;
  float uy = 0;
  bool fixed = false;
};

class PlacementCanvas : public QWidget {
 public:
  explicit PlacementCanvas(QWidget* parent = nullptr);

  /** Draw arrows from where each cell sat when global placement finished. */
  void SetShowDisplacementFromGlobal(bool show);

  /**
   * Prepare a transition and report whether one is worth playing.
   *
   * Builds the frame that will actually be drawn: the previous population, with
   * a target recorded for each ordinary cell that survives into the new
   * snapshot. Cells the new snapshot drops simply stay where they were and
   * vanish at the end; cells it adds are not drawn until the end. Only ordinary
   * cells are matched, and only by component id within that kind -- ids repeat
   * across the well-tap and end-cap collections, so an id alone is not an
   * identity.
   */
  bool BeginTransition();

  /** Show the transition at eased progress `t`, 0 = previous, 1 = target. */
  void SetAnimationProgress(double eased);

  /** Drop back to drawing the target snapshot exactly. */
  void EndAnimation();

  /** The component set to render: the transition's frame, or the snapshot. */
  const std::vector<SnapshotComponent>& Drawn() const;
  const std::vector<SnapshotWellRect>& DrawnWells() const;
  const std::vector<SnapshotIoPin>& DrawnIoPins() const;

  /** Draw arrows from where each cell sat in the previous snapshot. */
  void SetShowDisplacementFromPrevious(bool show);

  void SetSnapshot(Circuit* circuit, const PlacementSnapshotMetadata& metadata);

  /**
   * Render the given world-coordinate rectangle to a PNG.
   *
   * Sets the view to frame [llx, lly]-[urx, ury] in microns, letterboxing to
   * preserve aspect, then grabs the canvas. Used to capture documentation
   * screenshots without a human at the window; see SnapshotCaptureRequests.
   */
  /** Frame a world-coordinate region, without writing anything. */
  void CaptureRegionView(double llx, double lly, double urx, double ury);

  bool CaptureRegion(const QString& path, double llx, double lly, double urx,
                     double ury);

  /** Bounding box the view fits to, in canvas world coordinates. */
  QRectF DesignBounds() const;

  void FitToView();

  bool MovableDotMode() const;

  void SetMovableDotMode(bool enabled);

  void SetShowIoPins(bool show);

  void SetShowDelayLines(bool show);

  void SetShowTopologyAdded(bool show);

  /**
   * Highlight one constraint's witnesses. Rendering only.
   *
   * The sets arrive already classified by Dali; nothing here re-derives a path,
   * asks the timer anything, or touches a coordinate.
   */
  void SetHighlightedPath(const PlacementTimingPathVisualization* path);

  void SetFadeUnrelated(bool fade);
  void SetShowFastPath(bool show);
  void SetShowSlowPath(bool show);
  void SetShowSharedPath(bool show);

  /** Which highlight class a component belongs to, for tests and painting. */
  enum class PathClass { kNone, kFast, kSlow, kShared };
  PathClass ClassOf(int component_id) const;
  bool HasHighlight() const;
  bool FadeUnrelated() const;

  /**
   * The ink one component's dot is drawn with, or an invalid colour when no
   * snapshot holds that component.
   *
   * Read-only, and here for the tests: asserting that the dot layer applies
   * the same classification as the cell layer is the check that stops the two
   * drifting apart again, and doing it on pixels alone would mean a test that
   * re-derives the palette and passes when both layers are wrong together.
   */
  QColor DotFillForComponent(int component_id) const;

 protected:
  void paintEvent(QPaintEvent* /*event*/) override;

  void resizeEvent(QResizeEvent* /*event*/) override;

  void mousePressEvent(QMouseEvent* event) override;

  void mouseMoveEvent(QMouseEvent* event) override;

  void mouseReleaseEvent(QMouseEvent* event) override;

  void wheelEvent(QWheelEvent* event) override;

 private:
  void DrawStatusBand(QPainter* painter) const;

  /**
   * Centre of every movable, non-I/O component, in circuit order.
   *
   * The component list does not change during placement, so the same index
   * refers to the same cell in every snapshot and displacement can be measured
   * by position in this vector.
   */
  static std::vector<QPointF> CollectMovableCenters(Circuit* circuit);

  /**
   * Draw one arrow per moved cell, from where it sat in `origin` to where it
   * sits now. Sub-pixel moves are skipped so a stage that barely perturbs the
   * placement stays readable. Both references can be shown at once, so each
   * carries its own colour.
   */
  static constexpr double kMinArrowPixels = 0.4;

  void DrawDisplacementFrom(QPainter* painter,
                            const std::vector<QPointF>& origin,
                            const QColor& color) const;

  void DrawDisplacement(QPainter* painter) const;

  QRectF VisiblePlacementArea() const;

  void AppendComponents(Circuit* circuit, std::vector<Component>& components,
                        SnapshotComponentKind kind);

  /** Copy placed I/O pin geometry into the lightweight GUI snapshot. */
  void AppendIoPins(Circuit* circuit);

  QRectF ComponentScreenRect(const SnapshotComponent& component) const;

  QRectF WellScreenRect(const SnapshotWellRect& well_rect) const;

  void DrawWellRects(QPainter* painter) const;

  /**
   * How one component is coloured, for the cell layer and the dot layer both.
   *
   * The two layers used to classify components separately and drifted apart:
   * the dots ignored the selected path and the fade entirely, so on a design
   * whose cells are movable -- which is every ordinary design -- Fade unrelated
   * subdued the well taps and the end caps and left the design itself alone.
   * One policy consulted twice is what stops that recurring.
   *
   * `fill` is for the cell rectangle and `dot_fill` for the dot. They differ
   * only where the geometry demands it: an unclassified cell keeps each
   * layer's own base colour, and a faded dot needs more opacity than a faded
   * rectangle to survive at two pixels across.
   */
  struct ComponentPaint {
    QColor fill;
    QColor dot_fill;
    QColor edge;
    double edge_width = 1.0;
    PathClass path_class = PathClass::kNone;
    bool faded = false;
  };

  ComponentPaint PaintFor(const SnapshotComponent& component) const;

  void DrawComponent(QPainter* painter,
                     const SnapshotComponent& component) const;

  void DrawMovableDots(QPainter* painter) const;

  /** A ring around a witness endpoint, so the two ends of a path are findable.
   */
  void DrawEndpointMarker(QPainter* painter, const QRectF& rect,
                          int component_id) const;

  /** Draw the selected constraint's witness edges between cell centres. */
  void DrawHighlightEdges(QPainter* painter) const;

  /** Draw ordered chain edges behind cells so a detour reads as a net path. */
  void DrawDelayLineNets(QPainter* painter) const;

  /** Draw pins above cells so manual I/O edits remain visible at any zoom. */
  void DrawIoPins(QPainter* painter) const;

  double WorldToScreenX(double x) const;
  double WorldToScreenY(double y) const;
  double ScreenToWorldX(double x) const;
  double ScreenToWorldY(double y) const;

  std::vector<SnapshotComponent> components_;
  std::vector<PlacementDelayLineVisualization> delay_lines_;
  std::vector<QPointF> current_centers_;
  std::vector<QPointF> previous_centers_;
  std::vector<SnapshotComponent> previous_components_;
  std::vector<SnapshotWellRect> previous_well_rects_;
  std::vector<SnapshotIoPin> previous_io_pins_;
  std::vector<SnapshotComponent> animation_draw_;
  std::vector<QPointF> animation_target_;
  bool animation_active_ = false;
  std::vector<QPointF> global_centers_;
  bool show_displacement_from_global_ = false;
  bool show_displacement_from_previous_ = false;
  std::vector<SnapshotWellRect> well_rects_;
  std::vector<SnapshotIoPin> io_pins_;
  std::string snapshot_label_ = "Waiting for first placement snapshot";
  std::string hpwl_label_;
  double boundary_llx_ = 0;
  double boundary_lly_ = 0;
  double boundary_urx_ = 1;
  double boundary_ury_ = 1;
  double view_llx_ = 0;
  double view_lly_ = 0;
  double view_urx_ = 1;
  double view_ury_ = 1;
  double scale_ = 1;
  double pan_x_ = 0;
  double pan_y_ = 0;
  bool has_view_ = false;
  bool has_snapshot_ = false;
  bool is_dragging_ = false;
  bool movable_dot_mode_ = true;
  bool show_io_pins_ = true;
  bool show_delay_lines_ = false;
  bool show_topology_added_ = false;
  struct HighlightEdge {
    int from = -1;
    int to = -1;
    PathClass kind = PathClass::kNone;
  };
  std::unordered_set<int> fast_only_;
  std::unordered_set<int> slow_only_;
  std::unordered_set<int> common_;
  std::vector<HighlightEdge> highlight_edges_;
  std::vector<int> endpoint_ids_;
  bool has_highlight_ = false;
  bool fade_unrelated_ = false;
  bool show_fast_path_ = true;
  bool show_slow_path_ = true;
  bool show_shared_path_ = true;
  QPoint last_mouse_pos_;
};

struct HpwlSample {
  int iteration = 0;
  double hpwl = 0;
};

class HpwlHistoryPanel : public QWidget {
 public:
  explicit HpwlHistoryPanel(QWidget* parent = nullptr);

  /** Declare which stages will run so their chart slots are reserved up front.
   */
  void SetStages(std::vector<PlacementSnapshotStage> stages);

  void AddSnapshot(Circuit* circuit, const PlacementSnapshotMetadata& metadata);

 protected:
  void paintEvent(QPaintEvent* /*event*/) override;

 private:
  struct Series {
    QString name;
    const std::vector<HpwlSample>& samples;
    QColor color;
  };

  struct ChartSection {
    QString title;
    std::vector<Series> series;
  };

  static int NextIteration(const std::vector<HpwlSample>& samples);

  static bool CollectBounds(const ChartSection& section, int* min_iteration,
                            int* max_iteration, double* min_hpwl,
                            double* max_hpwl);

  void DrawSection(QPainter* painter, const QRectF& area,
                   const ChartSection& section) const;

  static void DrawSeries(QPainter* painter, const QRectF& plot,
                         const Series& series, int min_iteration,
                         int max_iteration, double min_hpwl, double max_hpwl);

  std::vector<HpwlSample> global_lower_;
  std::vector<HpwlSample> global_upper_;
  std::vector<HpwlSample> detailed_;
  std::vector<HpwlSample> legalization_;
  std::vector<PlacementSnapshotStage> stages_;
};

/**
 * One-line status that elides instead of widening its window.
 *
 * The snapshot label carries every topology change -- eight sites, their pair
 * transitions and their boundary slacks -- and a QLabel asks for the width its
 * whole text needs. One such snapshot therefore stretched the top-level window
 * and shifted the diagnostics splitter mid-run. This keeps the full text for
 * the tooltip and accessibility and paints only what fits.
 */
class ElidingStatusLabel : public QLabel {
 public:
  explicit ElidingStatusLabel(QWidget* parent = nullptr);

  /** Compact text to show; `detail` is kept whole for tooltip and access. */
  void SetStatus(const QString& compact, const QString& detail);

  /** The complete, unelided status, whatever the widget had room to paint. */
  QString FullStatus() const { return full_status_; }

  QSize minimumSizeHint() const override;
  QSize sizeHint() const override;

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  QString compact_status_;
  QString full_status_;
};

class QtPlacementWindow : public QWidget {
 public:
  explicit QtPlacementWindow(QWidget* parent = nullptr);

  void SetPauseAtEverySnapshot(bool pause);

  void SetStages(std::vector<PlacementSnapshotStage> stages);

  void SetSnapshot(Circuit* circuit, const PlacementSnapshotMetadata& metadata);

  /** Add the Timing tab once, the first time timing data actually arrives. */
  void EnsureTimingTab();

  /**
   * Raise the Timing tab, for a capture that is meant to show it.
   *
   * HPWL is the tab an interactive session opens on, and a window capture of
   * the timing pane that showed the HPWL chart instead would be a frame that
   * proves nothing about what it was asked to show.
   */
  bool ShowTimingTab();

  /**
   * The fade toggle, which now lives in the Timing pane beside the selection
   * it describes. Forwarded so a test still drives the real control.
   */
  QCheckBox* FadeUnrelatedCheckbox();

  /** The status line, so a layout test can measure what it asks for. */
  ElidingStatusLabel* StatusLabel();

  PlacementCanvas* Canvas();
  TimingDiagnosticsPanel* TimingPanel();
  QTabWidget* DiagnosticsTabs();
  QSplitter* DiagnosticsSplitter();
  int TimingTabIndex() const;

  bool ShouldPause() const;
  bool ShouldResume() const;
  void MarkFinished();

  /**
   * Hold the finished window open until the operator closes it, or until it
   * has sat untouched for the idle limit.
   *
   * A finished run used to wait on the window forever. That is right when
   * somebody is at the screen and wrong the rest of the time: an unattended
   * run -- a scripted gate, an overnight flow -- would sit on its final frame
   * indefinitely, holding up everything behind it, and the only way out was a
   * human walking to the machine. Waiting a minute and then closing keeps the
   * frame there for as long as anyone is actually looking, because any mouse
   * move, scroll, click or keystroke restarts the minute.
   */
  void WaitUntilClosedOrIdle();

  /** Seconds since the last mouse, wheel, key or touch event anywhere here. */
  double IdleSeconds() const;

  /** Restart the idle clock, exactly as a user action does. */
  void NoteUserActivity();

  /** True once nobody has touched the window for the idle limit. */
  bool HasGoneIdle() const;

  /** How long a finished window waits for an operator before closing itself. */
  static constexpr double kDefaultIdleCloseSeconds = 60.0;

  /**
   * Change that limit. The default is the shipped behaviour; this exists so
   * the wait can be exercised in a test without spending a minute per run.
   */
  void SetIdleCloseSeconds(double seconds);

  /**
   * Play the transition into the snapshot the canvas is already holding.
   *
   * Runs on the same manually pumped event loop the pause uses, so the window
   * stays responsive and closing it ends the transition immediately. Progress
   * comes from elapsed wall time rather than a frame count, so a slow repaint
   * shortens the animation instead of stretching it. The canvas is returned to
   * drawing the target exactly before this returns, which is what keeps
   * screenshot capture and the paused frame on the real snapshot.
   */
  void AnimateToSnapshot();

 protected:
  /**
   * Watch the whole application for signs of a person.
   *
   * A filter on the application rather than on this widget, because the events
   * that prove somebody is present are delivered to whichever child has the
   * mouse -- the canvas, a checkbox, the constraint tree -- and never reach the
   * window itself.
   */
  bool eventFilter(QObject* watched, QEvent* event) override;

 private:
  static constexpr int kMinAnimationMs = 50;
  static constexpr int kMaxAnimationMs = 2000;
  static constexpr int kDefaultAnimationMs = 1000;

  /** Standard cubic ease-in/ease-out: slow at both ends, fastest mid-move. */
  static double EaseInOut(double t);

  /** Bold row prefix naming what the controls that follow it act on. */
  QLabel* MakeGroupLabel(const QString& text);

  void SaveCurrentImages();

  std::chrono::steady_clock::time_point last_user_activity_ =
      std::chrono::steady_clock::now();
  double idle_close_seconds_ = kDefaultIdleCloseSeconds;

  ElidingStatusLabel* status_label_ = nullptr;
  PlacementCanvas* canvas_ = nullptr;
  HpwlHistoryPanel* hpwl_panel_ = nullptr;
  TimingDiagnosticsPanel* timing_panel_ = nullptr;
  QTabWidget* diagnostics_tabs_ = nullptr;
  QSplitter* diagnostics_splitter_ = nullptr;
  int timing_tab_index_ = -1;
  QCheckBox* pause_checkbox_ = nullptr;
  QCheckBox* movable_dot_checkbox_ = nullptr;
  QCheckBox* io_pin_checkbox_ = nullptr;
  bool has_shown_a_snapshot_ = false;
  QCheckBox* displacement_global_checkbox_ = nullptr;
  QCheckBox* displacement_previous_checkbox_ = nullptr;
  QCheckBox* animate_checkbox_ = nullptr;
  QSlider* animation_slider_ = nullptr;
  QSpinBox* animation_spin_ = nullptr;
  QPushButton* step_button_ = nullptr;
  QPushButton* continue_button_ = nullptr;
  QPushButton* fit_button_ = nullptr;
  QPushButton* save_button_ = nullptr;
  QString current_snapshot_label_;
  QString compact_snapshot_label_;
  QString last_export_dir_;
  bool step_requested_ = false;
  bool continue_requested_ = false;
};

}  // namespace dali

#endif  // DALI_GUI_QT_PLACEMENT_WINDOW_H_
