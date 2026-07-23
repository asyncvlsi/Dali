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
 * Qt implementation of the placement snapshot sink.
 *
 * Placement stages publish snapshots through the abstract sink; this one draws
 * them and can pause the run between stages, which is what makes intermediate
 * state inspectable. All Qt knowledge stays behind that interface, so the
 * placer builds without Qt.
 *
 * See [the GUI guide](README.md) for the controls and for the unattended
 * screenshot capture.
 */
#include "dali/gui/qt_placement_snapshot_sink.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/common/logging.h"

namespace dali {

enum class SnapshotComponentKind {
  kOrdinary,
  kWellTap,
  kEndCap,
};

struct SnapshotComponent {
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;
  bool fixed = false;
  ComponentOrient orient = N;
  SnapshotComponentKind kind = SnapshotComponentKind::kOrdinary;
};

struct SnapshotWellRect {
  float lx = 0;
  float ly = 0;
  float ux = 0;
  float uy = 0;
  PlacementWellLayer layer = PlacementWellLayer::kPwell;
};

static std::string FormatHpwl(double hpwl) {
  std::ostringstream out;
  out.precision(8);
  out << hpwl;
  return out.str();
}

static QString FormatCompactHpwl(double hpwl) {
  if (std::abs(hpwl) >= 1e6) {
    return QString("%1M").arg(hpwl / 1e6, 0, 'f', 2);
  }
  if (std::abs(hpwl) >= 1e3) {
    return QString("%1K").arg(hpwl / 1e3, 0, 'f', 1);
  }
  return QString("%1").arg(hpwl, 0, 'f', 0);
}

enum class CellMarkerCorner {
  kLowerLeft,
  kLowerRight,
  kUpperLeft,
  kUpperRight,
};

static CellMarkerCorner LocalLowerLeftCorner(ComponentOrient orient) {
  switch (orient) {
    case N:
    case FE:
      return CellMarkerCorner::kLowerLeft;
    case E:
    case FN:
      return CellMarkerCorner::kLowerRight;
    case W:
    case FS:
      return CellMarkerCorner::kUpperLeft;
    case S:
    case FW:
      return CellMarkerCorner::kUpperRight;
  }
  return CellMarkerCorner::kLowerLeft;
}

static QPolygonF OrientationMarker(const QRectF& rect, ComponentOrient orient,
                                   double size) {
  QPolygonF marker;
  switch (LocalLowerLeftCorner(orient)) {
    case CellMarkerCorner::kLowerLeft:
      marker << rect.bottomLeft() << QPointF(rect.left() + size, rect.bottom())
             << QPointF(rect.left(), rect.bottom() - size);
      break;
    case CellMarkerCorner::kLowerRight:
      marker << rect.bottomRight()
             << QPointF(rect.right() - size, rect.bottom())
             << QPointF(rect.right(), rect.bottom() - size);
      break;
    case CellMarkerCorner::kUpperLeft:
      marker << rect.topLeft() << QPointF(rect.left() + size, rect.top())
             << QPointF(rect.left(), rect.top() + size);
      break;
    case CellMarkerCorner::kUpperRight:
      marker << rect.topRight() << QPointF(rect.right() - size, rect.top())
             << QPointF(rect.right(), rect.top() + size);
      break;
  }
  return marker;
}

constexpr double kCanvasMargin = 24.0;
constexpr double kStatusBandHeight = 48.0;

static QString SanitizeFileStem(QString stem) {
  stem = stem.trimmed();
  for (int i = 0; i < stem.size(); ++i) {
    const QChar ch = stem.at(i);
    if (!ch.isLetterOrNumber() && ch != '_' && ch != '-' && ch != '.') {
      stem[i] = '_';
    }
  }
  while (stem.contains("__")) {
    stem.replace("__", "_");
  }
  stem = stem.left(120);
  return stem.isEmpty() ? QString("dali_snapshot") : stem;
}

static QString UniqueImagePath(const QDir& dir, const QString& file_stem) {
  QString path = dir.filePath(file_stem + ".png");
  int suffix = 1;
  while (QFileInfo::exists(path)) {
    path = dir.filePath(QString("%1_%2.png").arg(file_stem).arg(suffix++));
  }
  return path;
}

class PlacementCanvas : public QWidget {
 public:
  explicit PlacementCanvas(QWidget* parent = nullptr) : QWidget(parent) {
    setMinimumSize(900, 640);
    setAutoFillBackground(true);
    setMouseTracking(true);
  }

  /** Draw arrows from where each cell sat when global placement finished. */
  void SetShowDisplacementFromGlobal(bool show) {
    show_displacement_from_global_ = show;
    update();
  }

  /** Draw arrows from where each cell sat in the previous snapshot. */
  void SetShowDisplacementFromPrevious(bool show) {
    show_displacement_from_previous_ = show;
    update();
  }

  void SetSnapshot(Circuit* circuit,
                   const PlacementSnapshotMetadata& metadata) {
    components_.clear();
    well_rects_.clear();
    if (circuit == nullptr) {
      update();
      return;
    }

    boundary_llx_ = circuit->RegionLLX() * circuit->GridValueX();
    boundary_lly_ = circuit->RegionLLY() * circuit->GridValueY();
    boundary_urx_ = circuit->RegionURX() * circuit->GridValueX();
    boundary_ury_ = circuit->RegionURY() * circuit->GridValueY();
    view_llx_ = boundary_llx_;
    view_lly_ = boundary_lly_;
    view_urx_ = boundary_urx_;
    view_ury_ = boundary_ury_;

    components_.reserve(
        circuit->Components().size() + circuit->design().WellTaps().size() +
        circuit->design().EndCapComponentCollection().Instances().size());
    AppendComponents(circuit, circuit->Components(),
                     SnapshotComponentKind::kOrdinary);
    AppendComponents(circuit, circuit->design().WellTaps(),
                     SnapshotComponentKind::kWellTap);
    AppendComponents(circuit,
                     circuit->design().EndCapComponentCollection().Instances(),
                     SnapshotComponentKind::kEndCap);

    well_rects_.reserve(metadata.well_rects.size());
    for (const PlacementWellRect& well_rect : metadata.well_rects) {
      if (well_rect.ux <= well_rect.lx || well_rect.uy <= well_rect.ly) {
        continue;
      }
      well_rects_.push_back({well_rect.lx, well_rect.ly, well_rect.ux,
                             well_rect.uy, well_rect.layer});
      view_llx_ = std::min(view_llx_, static_cast<double>(well_rect.lx));
      view_lly_ = std::min(view_lly_, static_cast<double>(well_rect.ly));
      view_urx_ = std::max(view_urx_, static_cast<double>(well_rect.ux));
      view_ury_ = std::max(view_ury_, static_cast<double>(well_rect.uy));
    }

    previous_centers_ = std::move(current_centers_);
    current_centers_ = CollectMovableCenters(circuit);
    // Track the running global-placement state so that once global placement
    // ends this holds its final result, which is the reference the "vs global"
    // overlay measures against. While global placement is still running the
    // reference is the current snapshot itself, so that overlay is empty until
    // legalization starts -- there is no "global placement result" yet.
    if (metadata.group == "global_placement") {
      global_centers_ = current_centers_;
    }

    snapshot_label_ = metadata.id;
    hpwl_label_ = "HPWL " + FormatHpwl(circuit->WeightedHPWL());
    has_snapshot_ = true;
    if (!has_view_) {
      FitToView();
    }
    update();
  }

  /**
   * Render the given world-coordinate rectangle to a PNG.
   *
   * Sets the view to frame [llx, lly]-[urx, ury] in microns, letterboxing to
   * preserve aspect, then grabs the canvas. Used to capture documentation
   * screenshots without a human at the window; see SnapshotCaptureRequests.
   */
  void CaptureRegion(const QString& path, double llx, double lly, double urx,
                     double ury) {
    const double w = std::max(width() - 2.0 * kCanvasMargin, 1.0);
    const double h =
        std::max(height() - kStatusBandHeight - 2.0 * kCanvasMargin, 1.0);
    const double rw = std::max(urx - llx, 1.0);
    const double rh = std::max(ury - lly, 1.0);
    scale_ = std::min(w / rw, h / rh);
    pan_x_ = kCanvasMargin - llx * scale_ + (w - rw * scale_) / 2.0;
    pan_y_ = kCanvasMargin + ury * scale_ + (h - rh * scale_) / 2.0;
    has_view_ = true;
    repaint();
    grab().save(path);
  }

  /** Bounding box the view fits to, in canvas world coordinates. */
  QRectF DesignBounds() const {
    return QRectF(QPointF(view_llx_, view_lly_), QPointF(view_urx_, view_ury_));
  }

  void FitToView() {
    const double design_width = std::max(view_urx_ - view_llx_, 1.0);
    const double design_height = std::max(view_ury_ - view_lly_, 1.0);
    const double drawing_width = std::max(width() - 2.0 * kCanvasMargin, 1.0);
    const double drawing_height =
        std::max(height() - kStatusBandHeight - 2.0 * kCanvasMargin, 1.0);
    scale_ =
        std::min(drawing_width / design_width, drawing_height / design_height);
    scale_ = std::max(scale_, 1e-9);

    const double fitted_width = design_width * scale_;
    const double fitted_height = design_height * scale_;
    const double centered_left =
        kCanvasMargin + std::max(drawing_width - fitted_width, 0.0) / 2.0;
    const double centered_bottom =
        height() - kStatusBandHeight - kCanvasMargin -
        std::max(drawing_height - fitted_height, 0.0) / 2.0;
    pan_x_ = centered_left - view_llx_ * scale_;
    pan_y_ = centered_bottom + view_lly_ * scale_;
    has_view_ = true;
    update();
  }

  bool MovableDotMode() const { return movable_dot_mode_; }

  void SetMovableDotMode(bool enabled) {
    movable_dot_mode_ = enabled;
    update();
  }

 protected:
  void paintEvent(QPaintEvent* /*event*/) override {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(248, 249, 251));
    painter.setRenderHint(QPainter::Antialiasing, false);

    const double boundary_width = std::max(boundary_urx_ - boundary_llx_, 1.0);
    const double boundary_height = std::max(boundary_ury_ - boundary_lly_, 1.0);

    DrawWellRects(&painter);

    painter.setPen(QPen(QColor(40, 48, 60), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(QRectF(WorldToScreenX(boundary_llx_),
                            WorldToScreenY(boundary_ury_),
                            boundary_width * scale_, boundary_height * scale_));

    for (const SnapshotComponent& component : components_) {
      if (component.fixed &&
          component.kind == SnapshotComponentKind::kOrdinary) {
        DrawComponent(&painter, component);
      }
    }
    if (movable_dot_mode_) {
      DrawMovableDots(&painter);
    } else {
      for (const SnapshotComponent& component : components_) {
        if (!component.fixed &&
            component.kind == SnapshotComponentKind::kOrdinary) {
          DrawComponent(&painter, component);
        }
      }
    }
    DrawDisplacement(&painter);
    for (const SnapshotComponent& component : components_) {
      if (component.kind != SnapshotComponentKind::kOrdinary) {
        DrawComponent(&painter, component);
      }
    }

    DrawStatusBand(&painter);
  }

  void resizeEvent(QResizeEvent* /*event*/) override {
    if (has_snapshot_ && !has_view_) {
      FitToView();
    }
  }

  void mousePressEvent(QMouseEvent* event) override {
    if (event->button() != Qt::LeftButton) {
      return;
    }
    is_dragging_ = true;
    last_mouse_pos_ = event->pos();
    setCursor(Qt::ClosedHandCursor);
  }

  void mouseMoveEvent(QMouseEvent* event) override {
    if (!is_dragging_) {
      return;
    }
    const QPoint delta = event->pos() - last_mouse_pos_;
    pan_x_ += delta.x();
    pan_y_ += delta.y();
    last_mouse_pos_ = event->pos();
    update();
  }

  void mouseReleaseEvent(QMouseEvent* event) override {
    if (event->button() != Qt::LeftButton) {
      return;
    }
    is_dragging_ = false;
    setCursor(Qt::ArrowCursor);
  }

  void wheelEvent(QWheelEvent* event) override {
    const QPointF pos = event->position();
    const double world_x = ScreenToWorldX(pos.x());
    const double world_y = ScreenToWorldY(pos.y());
    const double zoom_factor =
        std::exp(static_cast<double>(event->angleDelta().y()) / 600.0);
    scale_ = std::clamp(scale_ * zoom_factor, 1e-6, 1e6);
    pan_x_ = pos.x() - world_x * scale_;
    pan_y_ = pos.y() + world_y * scale_;
    update();
  }

 private:
  void DrawStatusBand(QPainter* painter) const {
    const QRectF band(0, height() - kStatusBandHeight, width(),
                      kStatusBandHeight);
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(255, 255, 255, 238));
    painter->drawRect(band);
    painter->setPen(QPen(QColor(229, 231, 235), 1));
    painter->drawLine(QPointF(0, band.top()), QPointF(width(), band.top()));

    painter->setPen(QColor(31, 41, 55));
    painter->drawText(QPointF(16, band.top() + 20),
                      QString::fromStdString(snapshot_label_));
    painter->setPen(QColor(75, 85, 99));
    painter->drawText(QPointF(16, band.top() + 38),
                      QString::fromStdString(hpwl_label_));
    painter->drawText(QRectF(0, band.top() + 26, width() - 16, 18),
                      Qt::AlignRight | Qt::AlignVCenter,
                      QString("zoom %1 px/um").arg(scale_, 0, 'g', 4));
  }

  /**
   * Centre of every movable, non-I/O component, in circuit order.
   *
   * The component list does not change during placement, so the same index
   * refers to the same cell in every snapshot and displacement can be measured
   * by position in this vector.
   */
  static std::vector<QPointF> CollectMovableCenters(Circuit* circuit) {
    std::vector<QPointF> centers;
    centers.reserve(circuit->Components().size());
    for (Component& component : circuit->Components()) {
      if (component.MacroPtr() == circuit->tech().IoDummyMacroPtr()) continue;
      if (component.IsFixed()) continue;
      centers.emplace_back(
          (component.LLX() + component.Width() / 2.0) * circuit->GridValueX(),
          (component.LLY() + component.Height() / 2.0) * circuit->GridValueY());
    }
    return centers;
  }

  /**
   * Draw one arrow per moved cell, from where it sat in `origin` to where it
   * sits now. Sub-pixel moves are skipped so a stage that barely perturbs the
   * placement stays readable. Both references can be shown at once, so each
   * carries its own colour.
   */
  static constexpr double kMinArrowPixels = 0.4;

  void DrawDisplacementFrom(QPainter* painter,
                            const std::vector<QPointF>& origin,
                            const QColor& color) const {
    if (origin.size() != current_centers_.size()) return;

    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(color, 1));
    for (size_t i = 0; i < current_centers_.size(); ++i) {
      const QPointF from(origin[i]);
      const QPointF to(current_centers_[i]);
      const QPointF tail(WorldToScreenX(from.x()), WorldToScreenY(from.y()));
      const QPointF head(WorldToScreenX(to.x()), WorldToScreenY(to.y()));
      const double dx = head.x() - tail.x();
      const double dy = head.y() - tail.y();
      const double length = std::sqrt(dx * dx + dy * dy);
      if (length < kMinArrowPixels) continue;
      painter->drawLine(tail, head);
      const double ux = dx / length;
      const double uy = dy / length;
      const double barb = std::min(4.0, length * 0.35);
      painter->drawLine(head, QPointF(head.x() - barb * (ux + uy * 0.5),
                                      head.y() - barb * (uy - ux * 0.5)));
      painter->drawLine(head, QPointF(head.x() - barb * (ux - uy * 0.5),
                                      head.y() - barb * (uy + ux * 0.5)));
    }
  }

  void DrawDisplacement(QPainter* painter) const {
    if (show_displacement_from_global_) {
      DrawDisplacementFrom(painter, global_centers_, QColor(196, 62, 44));
    }
    if (show_displacement_from_previous_) {
      DrawDisplacementFrom(painter, previous_centers_, QColor(46, 106, 178));
    }
  }

  QRectF VisiblePlacementArea() const {
    return QRectF(0, 0, width(), std::max(height() - kStatusBandHeight, 0.0));
  }

  void AppendComponents(Circuit* circuit, std::vector<Component>& components,
                        SnapshotComponentKind kind) {
    for (Component& component : components) {
      if (component.MacroPtr() == circuit->tech().IoDummyMacroPtr()) {
        continue;
      }
      const double component_lx = component.LLX() * circuit->GridValueX();
      const double component_ly = component.LLY() * circuit->GridValueY();
      const double component_width = component.Width() * circuit->GridValueX();
      const double component_height =
          component.Height() * circuit->GridValueY();
      components_.push_back({static_cast<float>(component_lx),
                             static_cast<float>(component_ly),
                             static_cast<float>(component_width),
                             static_cast<float>(component_height),
                             component.IsFixed(), component.Orient(), kind});
      view_llx_ = std::min(view_llx_, component_lx);
      view_lly_ = std::min(view_lly_, component_ly);
      view_urx_ = std::max(view_urx_, component_lx + component_width);
      view_ury_ = std::max(view_ury_, component_ly + component_height);
    }
  }

  QRectF ComponentScreenRect(const SnapshotComponent& component) const {
    return QRectF(WorldToScreenX(component.x),
                  WorldToScreenY(component.y + component.height),
                  std::max(component.width * scale_, 0.6),
                  std::max(component.height * scale_, 0.6));
  }

  QRectF WellScreenRect(const SnapshotWellRect& well_rect) const {
    return QRectF(WorldToScreenX(well_rect.lx), WorldToScreenY(well_rect.uy),
                  std::max((well_rect.ux - well_rect.lx) * scale_, 0.6),
                  std::max((well_rect.uy - well_rect.ly) * scale_, 0.6));
  }

  void DrawWellRects(QPainter* painter) const {
    const QRectF visible_area = VisiblePlacementArea();
    for (const SnapshotWellRect& well_rect : well_rects_) {
      const QRectF rect = WellScreenRect(well_rect);
      if (!rect.intersects(visible_area)) {
        continue;
      }

      switch (well_rect.layer) {
        case PlacementWellLayer::kPwell:
          painter->setBrush(QColor(255, 238, 245, 24));
          painter->setPen(QPen(QColor(219, 39, 119, 28), 1));
          break;
        case PlacementWellLayer::kNwell:
          painter->setBrush(QColor(238, 247, 255, 24));
          painter->setPen(QPen(QColor(37, 99, 235, 28), 1));
          break;
        case PlacementWellLayer::kPplus:
          painter->setBrush(QColor(251, 207, 232, 70));
          painter->setPen(QPen(QColor(190, 24, 93, 80), 1));
          break;
        case PlacementWellLayer::kNplus:
          painter->setBrush(QColor(191, 219, 254, 70));
          painter->setPen(QPen(QColor(29, 78, 216, 80), 1));
          break;
      }
      painter->drawRect(rect);
    }
  }

  void DrawComponent(QPainter* painter,
                     const SnapshotComponent& component) const {
    const QRectF rect = ComponentScreenRect(component);
    if (!rect.intersects(VisiblePlacementArea())) {
      return;
    }

    if (component.kind == SnapshotComponentKind::kWellTap) {
      painter->setBrush(QColor(245, 158, 11, 220));
      painter->setPen(QPen(QColor(146, 64, 14), 1));
    } else if (component.kind == SnapshotComponentKind::kEndCap) {
      painter->setBrush(QColor(16, 185, 129, 220));
      painter->setPen(QPen(QColor(6, 95, 70), 1));
    } else {
      painter->setBrush(component.fixed ? QColor(76, 86, 106, 170)
                                        : QColor(136, 192, 208, 190));
      painter->setPen(
          QPen(component.fixed ? QColor(35, 42, 55) : QColor(66, 94, 111), 1));
    }
    painter->drawRect(rect);

    const double marker_size = std::min(
        std::clamp(std::min(rect.width(), rect.height()) * 0.35, 3.0, 10.0),
        std::min(rect.width(), rect.height()));
    if (marker_size >= 2.0) {
      painter->setPen(Qt::NoPen);
      painter->setBrush(QColor(17, 24, 39, 230));
      painter->drawPolygon(
          OrientationMarker(rect, component.orient, marker_size));
    }
  }

  void DrawMovableDots(QPainter* painter) const {
    const QRectF visible_area = VisiblePlacementArea();
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(33, 120, 143, 210));
    for (const SnapshotComponent& component : components_) {
      if (component.fixed) {
        continue;
      }
      const double center_x =
          WorldToScreenX(component.x + component.width / 2.0);
      const double center_y =
          WorldToScreenY(component.y + component.height / 2.0);
      if (!visible_area.contains(QPointF(center_x, center_y))) {
        continue;
      }
      const double radius = std::clamp(
          std::min(component.width, component.height) * scale_ * 0.4, 1.0, 2.2);
      painter->drawEllipse(QPointF(center_x, center_y), radius, radius);
    }
  }

  double WorldToScreenX(double x) const { return pan_x_ + x * scale_; }
  double WorldToScreenY(double y) const { return pan_y_ - y * scale_; }
  double ScreenToWorldX(double x) const { return (x - pan_x_) / scale_; }
  double ScreenToWorldY(double y) const { return (pan_y_ - y) / scale_; }

  std::vector<SnapshotComponent> components_;
  std::vector<QPointF> current_centers_;
  std::vector<QPointF> previous_centers_;
  std::vector<QPointF> global_centers_;
  bool show_displacement_from_global_ = false;
  bool show_displacement_from_previous_ = false;
  std::vector<SnapshotWellRect> well_rects_;
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
  QPoint last_mouse_pos_;
};

struct HpwlSample {
  int iteration = 0;
  double hpwl = 0;
};

class HpwlHistoryPanel : public QWidget {
 public:
  explicit HpwlHistoryPanel(QWidget* parent = nullptr) : QWidget(parent) {
    setMinimumWidth(330);
    setAutoFillBackground(true);
  }

  /** Declare which stages will run so their chart slots are reserved up front. */
  void SetStages(std::vector<PlacementSnapshotStage> stages) {
    stages_ = std::move(stages);
    update();
  }

  void AddSnapshot(Circuit* circuit,
                   const PlacementSnapshotMetadata& metadata) {
    if (circuit == nullptr) {
      return;
    }
    const double hpwl = circuit->WeightedHPWL();
    if (metadata.group == "global_placement") {
      if (metadata.subgroup == "lower_bound") {
        global_lower_.push_back({metadata.iteration, hpwl});
      } else if (metadata.subgroup == "upper_bound") {
        global_upper_.push_back({metadata.iteration, hpwl});
      }
    } else if (metadata.group == "detailed_placement") {
      detailed_.push_back({NextIteration(detailed_), hpwl});
    } else if (metadata.group == "legalization") {
      legalization_.push_back({NextIteration(legalization_), hpwl});
    }
    update();
  }

 protected:
  void paintEvent(QPaintEvent* /*event*/) override {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(253, 253, 252));
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QColor(31, 41, 55));
    painter.drawText(QPointF(16, 24), "HPWL over time");

    // Reserve one chart slot per stage the run declared it will execute, in
    // execution order, so each stage's curve fills into its own fixed slot as
    // snapshots arrive instead of charts popping in and reordering. The stage
    // list is configured by Dali from its options (see ExpectedSnapshotStages).
    std::vector<ChartSection> sections;
    for (const PlacementSnapshotStage& stage : stages_) {
      const QString title = QString::fromStdString(stage.title);
      if (stage.group == "global_placement") {
        sections.push_back(
            {title,
             {Series{"lower bound", global_lower_, QColor(37, 99, 235)},
              Series{"upper bound", global_upper_, QColor(220, 38, 38)}}});
      } else if (stage.group == "legalization") {
        sections.push_back(
            {title, {Series{"HPWL", legalization_, QColor(147, 51, 234)}}});
      } else if (stage.group == "detailed_placement") {
        sections.push_back(
            {title, {Series{"HPWL", detailed_, QColor(22, 163, 74)}}});
      }
    }
    if (sections.empty()) {
      painter.setPen(QColor(107, 114, 128));
      painter.drawText(QRectF(16, 48, width() - 32, 80), Qt::TextWordWrap,
                       "Charts appear as placement snapshots arrive.");
      return;
    }

    const int top = 42;
    const int gap = 12;
    const int chart_height = std::max(
        110,
        (height() - top - gap * (static_cast<int>(sections.size()) - 1) - 14) /
            static_cast<int>(sections.size()));
    int y = top;
    for (const ChartSection& section : sections) {
      DrawSection(&painter, QRectF(12, y, width() - 24, chart_height), section);
      y += chart_height + gap;
    }
  }

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

  static int NextIteration(const std::vector<HpwlSample>& samples) {
    return samples.empty() ? 0 : samples.back().iteration + 1;
  }

  static bool CollectBounds(const ChartSection& section, int* min_iteration,
                            int* max_iteration, double* min_hpwl,
                            double* max_hpwl) {
    bool found = false;
    for (const Series& series : section.series) {
      for (const HpwlSample& sample : series.samples) {
        if (!found) {
          *min_iteration = sample.iteration;
          *max_iteration = sample.iteration;
          *min_hpwl = sample.hpwl;
          *max_hpwl = sample.hpwl;
          found = true;
        } else {
          *min_iteration = std::min(*min_iteration, sample.iteration);
          *max_iteration = std::max(*max_iteration, sample.iteration);
          *min_hpwl = std::min(*min_hpwl, sample.hpwl);
          *max_hpwl = std::max(*max_hpwl, sample.hpwl);
        }
      }
    }
    return found;
  }

  void DrawSection(QPainter* painter, const QRectF& area,
                   const ChartSection& section) const {
    painter->setPen(QPen(QColor(229, 231, 235), 1));
    painter->setBrush(QColor(255, 255, 255));
    painter->drawRect(area);

    painter->setPen(QColor(31, 41, 55));
    painter->drawText(QPointF(area.left() + 10, area.top() + 20),
                      section.title);

    int min_iteration = 0;
    int max_iteration = 0;
    double min_hpwl = 0;
    double max_hpwl = 0;
    if (!CollectBounds(section, &min_iteration, &max_iteration, &min_hpwl,
                       &max_hpwl)) {
      // Reserved but not yet populated: label the empty slot so the viewer sees
      // the stage is pending rather than an unexplained blank box.
      painter->setPen(QColor(156, 163, 175));
      painter->drawText(
          QRectF(area.left() + 10, area.top() + 26, area.width() - 20,
                 area.height() - 34),
          Qt::AlignCenter, "Waiting for snapshots…");
      return;
    }
    if (min_iteration == max_iteration) {
      max_iteration = min_iteration + 1;
    }
    if (std::abs(max_hpwl - min_hpwl) < 1e-9) {
      max_hpwl = min_hpwl + 1.0;
    }

    const QRectF plot(area.left() + 46, area.top() + 34, area.width() - 62,
                      area.height() - 58);
    painter->setPen(QPen(QColor(229, 231, 235), 1));
    painter->drawLine(QPointF(plot.left(), plot.bottom()),
                      QPointF(plot.right(), plot.bottom()));
    painter->drawLine(QPointF(plot.left(), plot.top()),
                      QPointF(plot.left(), plot.bottom()));

    painter->setPen(QColor(107, 114, 128));
    painter->drawText(QPointF(area.left() + 8, plot.top() + 4),
                      FormatCompactHpwl(max_hpwl));
    painter->drawText(QPointF(area.left() + 8, plot.bottom()),
                      FormatCompactHpwl(min_hpwl));
    painter->drawText(QPointF(plot.left(), area.bottom() - 8),
                      QString::number(min_iteration));
    painter->drawText(QPointF(plot.right() - 24, area.bottom() - 8),
                      QString::number(max_iteration));

    int legend_x = static_cast<int>(area.left() + 10);
    const int legend_y = static_cast<int>(area.bottom() - 12);
    for (const Series& series : section.series) {
      if (series.samples.empty()) continue;
      painter->setPen(QPen(series.color, 2));
      painter->drawLine(QPointF(legend_x, legend_y - 4),
                        QPointF(legend_x + 16, legend_y - 4));
      painter->setPen(QColor(75, 85, 99));
      painter->drawText(QPointF(legend_x + 20, legend_y), series.name);
      legend_x += 105;
    }

    for (const Series& series : section.series) {
      DrawSeries(painter, plot, series, min_iteration, max_iteration, min_hpwl,
                 max_hpwl);
    }
  }

  static void DrawSeries(QPainter* painter, const QRectF& plot,
                         const Series& series, int min_iteration,
                         int max_iteration, double min_hpwl, double max_hpwl) {
    if (series.samples.empty()) {
      return;
    }

    auto map_point = [&](const HpwlSample& sample) {
      const double x_ratio =
          static_cast<double>(sample.iteration - min_iteration) /
          static_cast<double>(max_iteration - min_iteration);
      const double y_ratio = (sample.hpwl - min_hpwl) / (max_hpwl - min_hpwl);
      return QPointF(plot.left() + x_ratio * plot.width(),
                     plot.bottom() - y_ratio * plot.height());
    };

    painter->setPen(QPen(series.color, 2));
    if (series.samples.size() == 1) {
      QPointF point = map_point(series.samples.front());
      painter->setBrush(series.color);
      painter->drawEllipse(point, 3.0, 3.0);
      return;
    }

    QPolygonF polyline;
    for (const HpwlSample& sample : series.samples) {
      polyline << map_point(sample);
    }
    painter->drawPolyline(polyline);
  }

  std::vector<HpwlSample> global_lower_;
  std::vector<HpwlSample> global_upper_;
  std::vector<HpwlSample> detailed_;
  std::vector<HpwlSample> legalization_;
  std::vector<PlacementSnapshotStage> stages_;
};

class QtPlacementWindow : public QWidget {
 public:
  explicit QtPlacementWindow(QWidget* parent = nullptr) : QWidget(parent) {
    setWindowTitle("Dali Live Placement Debug");

    auto* layout = new QVBoxLayout(this);
    status_label_ = new QLabel("Waiting for first placement snapshot", this);
    status_label_->setText(
        "Waiting for first placement snapshot. Mouse wheel zooms, left-drag "
        "pans.");
    canvas_ = new PlacementCanvas(this);
    pause_checkbox_ = new QCheckBox("Pause at every snapshot", this);
    pause_checkbox_->setChecked(true);
    movable_dot_checkbox_ = new QCheckBox("Movable dots", this);
    displacement_global_checkbox_ =
        new QCheckBox("Displacement vs global", this);
    displacement_previous_checkbox_ =
        new QCheckBox("Displacement vs previous", this);
    movable_dot_checkbox_->setChecked(true);
    step_button_ = new QPushButton("Step", this);
    continue_button_ = new QPushButton("Continue", this);
    fit_button_ = new QPushButton("Fit", this);
    save_button_ = new QPushButton("Save PNG", this);
    hpwl_panel_ = new HpwlHistoryPanel(this);

    auto* run_controls = new QHBoxLayout();
    run_controls->addWidget(MakeGroupLabel("Run:"));
    run_controls->addWidget(pause_checkbox_);
    run_controls->addWidget(step_button_);
    run_controls->addWidget(continue_button_);
    run_controls->addStretch();
    run_controls->addWidget(save_button_);

    auto* view_controls = new QHBoxLayout();
    view_controls->addWidget(MakeGroupLabel("View:"));
    view_controls->addWidget(movable_dot_checkbox_);
    view_controls->addWidget(displacement_global_checkbox_);
    view_controls->addWidget(displacement_previous_checkbox_);
    view_controls->addStretch();
    view_controls->addWidget(fit_button_);

    auto* content = new QHBoxLayout();
    content->addWidget(canvas_, 1);
    content->addWidget(hpwl_panel_);

    layout->addWidget(status_label_);
    layout->addLayout(content, 1);
    layout->addLayout(run_controls);
    layout->addLayout(view_controls);

    QObject::connect(step_button_, &QPushButton::clicked, this,
                     [this]() { step_requested_ = true; });
    QObject::connect(continue_button_, &QPushButton::clicked, this, [this]() {
      pause_checkbox_->setChecked(false);
      continue_requested_ = true;
    });
    QObject::connect(fit_button_, &QPushButton::clicked, this,
                     [this]() { canvas_->FitToView(); });
    QObject::connect(
        movable_dot_checkbox_, &QCheckBox::toggled, this,
        [this](bool checked) { canvas_->SetMovableDotMode(checked); });
    QObject::connect(displacement_global_checkbox_, &QCheckBox::toggled, this,
                     [this](bool checked) {
                       canvas_->SetShowDisplacementFromGlobal(checked);
                     });
    QObject::connect(displacement_previous_checkbox_, &QCheckBox::toggled, this,
                     [this](bool checked) {
                       canvas_->SetShowDisplacementFromPrevious(checked);
                     });
    QObject::connect(save_button_, &QPushButton::clicked, this,
                     [this]() { SaveCurrentImages(); });
  }

  void SetPauseAtEverySnapshot(bool pause) {
    pause_checkbox_->setChecked(pause);
  }

  void SetStages(std::vector<PlacementSnapshotStage> stages) {
    hpwl_panel_->SetStages(std::move(stages));
  }

  void SetSnapshot(Circuit* circuit,
                   const PlacementSnapshotMetadata& metadata) {
    step_requested_ = false;
    continue_requested_ = false;
    current_snapshot_label_ = QString::fromStdString(metadata.id);
    if (metadata.iteration >= 0) {
      current_snapshot_label_ +=
          QString("  iteration %1").arg(metadata.iteration);
    }
    if (!metadata.group.empty()) {
      current_snapshot_label_ +=
          QString("  group %1").arg(QString::fromStdString(metadata.group));
    }
    if (!metadata.subgroup.empty()) {
      current_snapshot_label_ +=
          QString("  %1").arg(QString::fromStdString(metadata.subgroup));
    }
    status_label_->setText(current_snapshot_label_);
    canvas_->SetSnapshot(circuit, metadata);
    hpwl_panel_->AddSnapshot(circuit, metadata);
    if (!has_shown_a_snapshot_) {
      has_shown_a_snapshot_ = true;
      raise();
      activateWindow();
    }
  }

  PlacementCanvas* Canvas() { return canvas_; }

  bool ShouldPause() const { return pause_checkbox_->isChecked(); }
  bool ShouldResume() const { return step_requested_ || continue_requested_; }
  void MarkFinished() {
    status_label_->setText(current_snapshot_label_ +
                           "  | placement finished; close window to exit");
  }

 private:
  /** Bold row prefix naming what the controls that follow it act on. */
  QLabel* MakeGroupLabel(const QString& text) {
    auto* label = new QLabel(text, this);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
  }

  void SaveCurrentImages() {
    const QString start_dir =
        last_export_dir_.isEmpty() ? QDir::currentPath() : last_export_dir_;
    const QString dir_path = QFileDialog::getExistingDirectory(
        this, "Save Dali GUI images", start_dir);
    if (dir_path.isEmpty()) {
      return;
    }

    last_export_dir_ = dir_path;
    QDir dir(dir_path);
    const QString base_name = SanitizeFileStem(current_snapshot_label_.isEmpty()
                                                   ? QString("dali_snapshot")
                                                   : current_snapshot_label_);
    const QString placement_path =
        UniqueImagePath(dir, base_name + "_placement");
    const QString hpwl_path = UniqueImagePath(dir, base_name + "_hpwl");

    const bool placement_saved = canvas_->grab().save(placement_path, "PNG");
    const bool hpwl_saved = hpwl_panel_->grab().save(hpwl_path, "PNG");
    if (!placement_saved || !hpwl_saved) {
      QMessageBox::warning(this, "Save Dali GUI images",
                           "Failed to save one or more PNG files.");
      return;
    }

    status_label_->setText(QString("Saved %1 and %2")
                               .arg(QFileInfo(placement_path).fileName())
                               .arg(QFileInfo(hpwl_path).fileName()));
  }

  QLabel* status_label_ = nullptr;
  PlacementCanvas* canvas_ = nullptr;
  HpwlHistoryPanel* hpwl_panel_ = nullptr;
  QCheckBox* pause_checkbox_ = nullptr;
  QCheckBox* movable_dot_checkbox_ = nullptr;
  bool has_shown_a_snapshot_ = false;
  QCheckBox* displacement_global_checkbox_ = nullptr;
  QCheckBox* displacement_previous_checkbox_ = nullptr;
  QPushButton* step_button_ = nullptr;
  QPushButton* continue_button_ = nullptr;
  QPushButton* fit_button_ = nullptr;
  QPushButton* save_button_ = nullptr;
  QString current_snapshot_label_;
  QString last_export_dir_;
  bool step_requested_ = false;
  bool continue_requested_ = false;
};

QtPlacementSnapshotSink::QtPlacementSnapshotSink() = default;

QtPlacementSnapshotSink::~QtPlacementSnapshotSink() = default;

void QtPlacementSnapshotSink::StartRun(
    const PlacementSnapshotRunMetadata& metadata) {
  if (QApplication::instance() == nullptr) {
    static int argc = 1;
    static char app_name[] = "dali";
    static char* argv[] = {app_name, nullptr};
    owned_application_ = std::make_unique<QApplication>(argc, argv);
  }

  window_ = std::make_unique<QtPlacementWindow>();
  window_->SetPauseAtEverySnapshot(metadata.pause_at_every_snapshot);
  window_->SetStages(metadata.stages);
  int window_width = 1100;
  int window_height = 760;
  if (const char* size = std::getenv("DALI_GUI_CAPTURE_SIZE")) {
    const QStringList wh = QString(size).split('x');
    if (wh.size() == 2) {
      window_width = wh[0].toInt();
      window_height = wh[1].toInt();
    }
  }
  window_->resize(window_width, window_height);
  window_->show();
  enabled_ = true;
  LOG(info) << "Dali GUI debug mode enabled for design " << metadata.design_name
            << "\n";
}

/**
 * Screenshot requests read from the DALI_GUI_CAPTURE environment variable.
 *
 * Format is `<dir>;<stem>@<snapshot id>:<region>;...`, where `<stem>.png` is
 * the file written -- so one snapshot can be captured at several zoom levels --
 * and `<region>` is one of:
 *
 *   - `fit`, the whole design, framed as the GUI frames it on open
 *   - `<fx0>,<fy0>,<fx1>,<fy1>`, a sub-rectangle given as fractions of the
 *     design bounding box, so the same request works on any design
 *
 * Appending `+cells` to a region draws movable cells as rectangles instead of
 * the dots the GUI uses by default, which is what makes individual cells
 * legible in a zoomed capture. `DALI_GUI_CAPTURE_SIZE=<w>x<h>` sets the
 * window size, and so the image resolution. When DALI_GUI_CAPTURE is unset --
 * the normal case -- no requests exist and the GUI behaves exactly as if this
 * feature were absent.
 *
 * It exists so documentation screenshots can be regenerated reproducibly rather
 * than captured by hand, and works headlessly under QT_QPA_PLATFORM=offscreen,
 * so a run needs no display and no operator at the window.
 */
class SnapshotCaptureRequests {
 public:
  static SnapshotCaptureRequests FromEnvironment() {
    SnapshotCaptureRequests result;
    const char* spec = std::getenv("DALI_GUI_CAPTURE");
    if (spec == nullptr) return result;

    const QStringList parts = QString(spec).split(';', Qt::SkipEmptyParts);
    if (parts.isEmpty()) return result;
    result.directory_ = parts.value(0);
    for (int i = 1; i < parts.size(); ++i) {
      const QStringList key_and_region = parts[i].split(':');
      if (key_and_region.size() != 2) continue;
      const QStringList stem_and_id = key_and_region[0].split('@');
      if (stem_and_id.size() != 2) continue;
      QString region = key_and_region[1];
      const bool draw_cells = region.endsWith("+cells");
      if (draw_cells) region.chop(QString("+cells").size());
      if (region == "fit") {
        result.requests_.push_back({stem_and_id[1], stem_and_id[0],
                                    QRectF(0.0, 0.0, 1.0, 1.0), draw_cells});
        continue;
      }
      const QStringList bounds = region.split(',');
      if (bounds.size() != 4) continue;
      result.requests_.push_back(
          {stem_and_id[1], stem_and_id[0],
           QRectF(QPointF(bounds[0].toDouble(), bounds[1].toDouble()),
                  QPointF(bounds[2].toDouble(), bounds[3].toDouble())),
           draw_cells});
    }
    return result;
  }

  /** Write `<dir>/<stem>.png` for every request naming this snapshot. */
  void CaptureIfRequested(PlacementCanvas* canvas,
                          const std::string& snapshot_id) const {
    if (canvas == nullptr) return;
    const QString id = QString::fromStdString(snapshot_id);
    for (const Request& request : requests_) {
      if (request.id != id) continue;
      const QString path = directory_ + "/" + request.stem + ".png";
      const QRectF design = canvas->DesignBounds();
      const QRectF f = request.region;
      const double llx = design.left() + f.left() * design.width();
      const double lly = design.top() + f.top() * design.height();
      const double urx = design.left() + f.right() * design.width();
      const double ury = design.top() + f.bottom() * design.height();
      const bool previous_dot_mode = canvas->MovableDotMode();
      if (request.draw_cells) canvas->SetMovableDotMode(false);
      canvas->CaptureRegion(path, llx, lly, urx, ury);
      canvas->SetMovableDotMode(previous_dot_mode);
      LOG(info) << "Captured GUI snapshot " << path.toStdString() << "\n";
    }
  }

 private:
  struct Request {
    QString id;
    QString stem;
    QRectF region;
    bool draw_cells = false;
  };
  QString directory_;
  std::vector<Request> requests_;
};

void QtPlacementSnapshotSink::PublishSnapshot(
    Circuit* circuit, const PlacementSnapshotMetadata& metadata) {
  if (!enabled_ || window_ == nullptr) {
    return;
  }

  window_->SetSnapshot(circuit, metadata);
  QApplication::processEvents();

  SnapshotCaptureRequests::FromEnvironment().CaptureIfRequested(
      window_->Canvas(), metadata.id);

  if (!window_->ShouldPause()) {
    return;
  }

  while (window_ != nullptr && window_->isVisible() &&
         !window_->ShouldResume()) {
    QApplication::processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }
}

void QtPlacementSnapshotSink::FlushEvents() {
  if (!enabled_) {
    return;
  }
  QApplication::processEvents();
}

void QtPlacementSnapshotSink::FinishRun() {
  if (!enabled_ || window_ == nullptr) {
    return;
  }
  window_->MarkFinished();
  QApplication::processEvents();
  while (window_ != nullptr && window_->isVisible()) {
    QApplication::processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }
}

}  // namespace dali
