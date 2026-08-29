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
 * Implementation of the live placement window.
 *
 * Painting, signal wiring, and the snapshot bookkeeping live here rather than
 * in the header, so the two consumers -- the snapshot sink and the GUI tests --
 * include a declaration instead of the whole widget.
 */
#include "dali/gui/qt_placement_window.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QSplitter>
#include <QStandardItemModel>
#include <QStringList>
#include <QTabWidget>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "dali/common/logging.h"

namespace dali {

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

/*
  The delay-line palette.

  Two shades of one hue rather than two hues: an inserted member is still a
  member of that line, and the eye should read it as the same object grown
  rather than as a different kind of cell. The inserted fill is the existing
  fill scaled to about 55% luminance, which keeps the hue and drops brightness
  from roughly 144 to 79 -- far enough apart to separate at a glance and in a
  pixel test, close enough to read as one family.

  Added cells that are not delay-line members keep the cyan.
*/
const QColor kExistingDelayLineFill(249, 115, 22, 235);

const QColor kExistingDelayLineEdge(154, 52, 18);

const QColor kInsertedDelayLineFill(137, 63, 12, 245);

const QColor kInsertedDelayLineEdge(69, 31, 6);

/*
  The timing-path palette.

  Three classes, three hues, chosen so the shared portion is visibly a mixture
  of the two it is shared between rather than a fourth unrelated colour: a cell
  on both paths reads as purple between the blue fast path and the orange slow
  one. Inserted delay-line cells keep their dark orange, because what a cell is
  outranks which path it is on today.
*/
const QColor kFastPathFill(37, 99, 235, 235);

const QColor kFastPathEdge(23, 55, 140);

const QColor kSlowPathFill(234, 88, 12, 235);

const QColor kSlowPathEdge(124, 45, 8);

const QColor kSharedPathFill(147, 51, 234, 235);

const QColor kSharedPathEdge(76, 20, 128);

const QColor kFadedFill(203, 213, 225, 80);

const QColor kFadedEdge(148, 163, 184, 70);

/*
  A dot is one to two pixels of radius, so it needs more of the ink a cell
  rectangle can spare. Both of these are the neutral slate the faded rectangle
  uses, at the opacity that keeps a dot readable as context: unrelated cells
  stay on the picture and recede, rather than disappearing from it.
*/
const QColor kFadedDotFill(148, 163, 184, 150);

const QColor kPlainDotFill(33, 120, 143, 210);

const QColor kAddedCellFill(6, 182, 212, 245);

const QColor kAddedCellEdge(14, 116, 144);

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

PlacementCanvas::PlacementCanvas(QWidget* parent) : QWidget(parent) {
  setMinimumSize(900, 640);
  setAutoFillBackground(true);
  setMouseTracking(true);
}

void PlacementCanvas::SetShowDisplacementFromGlobal(bool show) {
  show_displacement_from_global_ = show;
  update();
}

bool PlacementCanvas::BeginTransition() {
  animation_draw_.clear();
  animation_target_.clear();
  if (previous_components_.empty()) return false;

  std::unordered_map<int, QPointF> targets;
  targets.reserve(components_.size());
  for (const SnapshotComponent& component : components_) {
    if (component.kind == SnapshotComponentKind::kOrdinary) {
      targets.emplace(component.component_id,
                      QPointF(component.x, component.y));
    }
  }

  bool moves = false;
  animation_draw_.reserve(previous_components_.size());
  animation_target_.reserve(previous_components_.size());
  for (const SnapshotComponent& component : previous_components_) {
    QPointF target(component.x, component.y);
    if (component.kind == SnapshotComponentKind::kOrdinary) {
      auto found = targets.find(component.component_id);
      if (found != targets.end()) {
        target = found->second;
        if (target.x() != component.x || target.y() != component.y) {
          moves = true;
        }
      }
    }
    animation_draw_.push_back(component);
    animation_target_.push_back(target);
  }
  if (!moves) {
    animation_draw_.clear();
    animation_target_.clear();
  }
  return moves;
}

void PlacementCanvas::SetAnimationProgress(double eased) {
  if (animation_draw_.empty()) return;
  for (size_t i = 0; i < animation_draw_.size(); ++i) {
    const SnapshotComponent& from = previous_components_[i];
    animation_draw_[i].x = from.x + eased * (animation_target_[i].x() - from.x);
    animation_draw_[i].y = from.y + eased * (animation_target_[i].y() - from.y);
  }
  animation_active_ = true;
  update();
}

void PlacementCanvas::EndAnimation() {
  animation_active_ = false;
  animation_draw_.clear();
  animation_target_.clear();
  update();
}

const std::vector<SnapshotComponent>& PlacementCanvas::Drawn() const {
  return animation_active_ ? animation_draw_ : components_;
}

const std::vector<SnapshotWellRect>& PlacementCanvas::DrawnWells() const {
  return animation_active_ ? previous_well_rects_ : well_rects_;
}

const std::vector<SnapshotIoPin>& PlacementCanvas::DrawnIoPins() const {
  return animation_active_ ? previous_io_pins_ : io_pins_;
}

void PlacementCanvas::SetShowDisplacementFromPrevious(bool show) {
  show_displacement_from_previous_ = show;
  update();
}

void PlacementCanvas::SetSnapshot(Circuit* circuit,
                                  const PlacementSnapshotMetadata& metadata) {
  // Keep the whole outgoing state, not just where cells were: the live
  // Circuit is the new state, so the snapshot about to be discarded is the
  // only record of the old one. A transition renders this previous
  // population throughout and swaps to the target only at the end. Rendering
  // the target population immediately would make every cell a stage inserts
  // -- 3104 well taps on a mid-size design, plus end caps and fillers --
  // appear at the first frame, which reads as a flash rather than movement.
  previous_components_ = std::move(components_);
  previous_well_rects_ = std::move(well_rects_);
  previous_io_pins_ = std::move(io_pins_);
  animation_active_ = false;
  components_.clear();
  delay_lines_.clear();
  well_rects_.clear();
  io_pins_.clear();
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
  AppendIoPins(circuit);

  delay_lines_ = metadata.delay_lines;
  std::unordered_set<int> delay_line_components;
  for (const PlacementDelayLineVisualization& delay_line : delay_lines_) {
    delay_line_components.insert(delay_line.component_ids.begin(),
                                 delay_line.component_ids.end());
  }
  const std::unordered_set<int> topology_added_components(
      metadata.topology_added_component_ids.begin(),
      metadata.topology_added_component_ids.end());
  for (SnapshotComponent& component : components_) {
    component.is_delay_line =
        delay_line_components.find(component.component_id) !=
        delay_line_components.end();
    component.is_topology_added =
        topology_added_components.find(component.component_id) !=
        topology_added_components.end();
  }

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
  // Scale travels with the frame. A captured picture of a placement is only
  // evidence of which design it is if the design's size is in the picture --
  // otherwise a full run and a one-bit fixture look alike once both are fitted
  // to the same canvas.
  // "design cells", not "components". The exported DEF also carries two
  // synthetic whole-region COVER cells -- `npwells` and `ppnps`, written
  // straight into the DEF text by SaveCircuitWellCoverCell rather than ever
  // existing as components -- so its COMPONENTS count is two higher than
  // anything the circuit contains. The covers are drawn here as the well and
  // implant rectangles instead. Labelling this "cells" invited exactly the
  // 2197-against-2195 comparison that has no defect behind it.
  hpwl_label_ = "HPWL " + FormatHpwl(circuit->WeightedHPWL()) +
                "  design cells " + std::to_string(components_.size()) +
                "  I/O " + std::to_string(io_pins_.size());
  has_snapshot_ = true;
  if (!has_view_) {
    FitToView();
  }
  update();
}

void PlacementCanvas::CaptureRegionView(double llx, double lly, double urx,
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
}

bool PlacementCanvas::CaptureRegion(const QString& path, double llx, double lly,
                                    double urx, double ury) {
  CaptureRegionView(llx, lly, urx, ury);
  return grab().save(path, "PNG");
}

QRectF PlacementCanvas::DesignBounds() const {
  return QRectF(QPointF(view_llx_, view_lly_), QPointF(view_urx_, view_ury_));
}

void PlacementCanvas::FitToView() {
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

bool PlacementCanvas::MovableDotMode() const { return movable_dot_mode_; }

void PlacementCanvas::SetMovableDotMode(bool enabled) {
  movable_dot_mode_ = enabled;
  update();
}

void PlacementCanvas::SetShowIoPins(bool show) {
  show_io_pins_ = show;
  update();
}

void PlacementCanvas::SetShowDelayLines(bool show) {
  show_delay_lines_ = show;
  update();
}

void PlacementCanvas::SetShowTopologyAdded(bool show) {
  show_topology_added_ = show;
  update();
}

void PlacementCanvas::SetHighlightedPath(
    const PlacementTimingPathVisualization* path) {
  fast_only_.clear();
  slow_only_.clear();
  common_.clear();
  highlight_edges_.clear();
  has_highlight_ = path != nullptr;
  if (path != nullptr) {
    fast_only_.insert(path->fast_only_component_ids.begin(),
                      path->fast_only_component_ids.end());
    slow_only_.insert(path->slow_only_component_ids.begin(),
                      path->slow_only_component_ids.end());
    common_.insert(path->common_component_ids.begin(),
                   path->common_component_ids.end());
    auto append = [this](const std::vector<PlacementPathEdge>& edges,
                         PathClass kind) {
      for (const PlacementPathEdge& edge : edges) {
        highlight_edges_.push_back(
            {edge.from_component_id, edge.to_component_id, kind});
      }
    };
    append(path->common_edges, PathClass::kShared);
    append(path->fast_only_edges, PathClass::kFast);
    append(path->slow_only_edges, PathClass::kSlow);
    endpoint_ids_ = {path->root_component_id, path->fast_terminal_component_id,
                     path->slow_terminal_component_id};
  } else {
    endpoint_ids_.clear();
  }
  update();
}

void PlacementCanvas::SetFadeUnrelated(bool fade) {
  fade_unrelated_ = fade;
  update();
}

void PlacementCanvas::SetShowFastPath(bool show) {
  show_fast_path_ = show;
  update();
}

void PlacementCanvas::SetShowSlowPath(bool show) {
  show_slow_path_ = show;
  update();
}

void PlacementCanvas::SetShowSharedPath(bool show) {
  show_shared_path_ = show;
  update();
}

PlacementCanvas::PathClass PlacementCanvas::ClassOf(int component_id) const {
  if (!has_highlight_) return PathClass::kNone;
  if (show_shared_path_ && common_.count(component_id) != 0) {
    return PathClass::kShared;
  }
  if (show_fast_path_ && fast_only_.count(component_id) != 0) {
    return PathClass::kFast;
  }
  if (show_slow_path_ && slow_only_.count(component_id) != 0) {
    return PathClass::kSlow;
  }
  return PathClass::kNone;
}

bool PlacementCanvas::HasHighlight() const { return has_highlight_; }

bool PlacementCanvas::FadeUnrelated() const { return fade_unrelated_; }

QColor PlacementCanvas::DotFillForComponent(int component_id) const {
  for (const SnapshotComponent& component : Drawn()) {
    if (component.kind == SnapshotComponentKind::kOrdinary &&
        component.component_id == component_id) {
      return PaintFor(component).dot_fill;
    }
  }
  return QColor();
}

void PlacementCanvas::paintEvent(QPaintEvent* /*event*/) {
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

  if (show_delay_lines_) {
    DrawDelayLineNets(&painter);
  }
  if (has_highlight_) {
    DrawHighlightEdges(&painter);
  }

  for (const SnapshotComponent& component : Drawn()) {
    if (component.fixed && component.kind == SnapshotComponentKind::kOrdinary) {
      DrawComponent(&painter, component);
    }
  }
  if (movable_dot_mode_) {
    DrawMovableDots(&painter);
  } else {
    for (const SnapshotComponent& component : Drawn()) {
      if (!component.fixed &&
          component.kind == SnapshotComponentKind::kOrdinary) {
        DrawComponent(&painter, component);
      }
    }
  }
  DrawDisplacement(&painter);
  for (const SnapshotComponent& component : Drawn()) {
    if (component.kind != SnapshotComponentKind::kOrdinary) {
      DrawComponent(&painter, component);
    }
  }
  if (show_io_pins_) {
    DrawIoPins(&painter);
  }

  DrawStatusBand(&painter);
}

void PlacementCanvas::resizeEvent(QResizeEvent* /*event*/) {
  if (has_snapshot_ && !has_view_) {
    FitToView();
  }
}

void PlacementCanvas::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) {
    return;
  }
  is_dragging_ = true;
  last_mouse_pos_ = event->pos();
  setCursor(Qt::ClosedHandCursor);
}

void PlacementCanvas::mouseMoveEvent(QMouseEvent* event) {
  if (!is_dragging_) {
    return;
  }
  const QPoint delta = event->pos() - last_mouse_pos_;
  pan_x_ += delta.x();
  pan_y_ += delta.y();
  last_mouse_pos_ = event->pos();
  update();
}

void PlacementCanvas::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) {
    return;
  }
  is_dragging_ = false;
  setCursor(Qt::ArrowCursor);
}

void PlacementCanvas::wheelEvent(QWheelEvent* event) {
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

void PlacementCanvas::DrawStatusBand(QPainter* painter) const {
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

std::vector<QPointF> PlacementCanvas::CollectMovableCenters(Circuit* circuit) {
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

void PlacementCanvas::DrawDisplacementFrom(QPainter* painter,
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

void PlacementCanvas::DrawDisplacement(QPainter* painter) const {
  if (show_displacement_from_global_) {
    DrawDisplacementFrom(painter, global_centers_, QColor(196, 62, 44));
  }
  if (show_displacement_from_previous_) {
    DrawDisplacementFrom(painter, previous_centers_, QColor(46, 106, 178));
  }
}

QRectF PlacementCanvas::VisiblePlacementArea() const {
  return QRectF(0, 0, width(), std::max(height() - kStatusBandHeight, 0.0));
}

void PlacementCanvas::AppendComponents(Circuit* circuit,
                                       std::vector<Component>& components,
                                       SnapshotComponentKind kind) {
  for (Component& component : components) {
    if (component.MacroPtr() == circuit->tech().IoDummyMacroPtr()) {
      continue;
    }
    const double component_lx = component.LLX() * circuit->GridValueX();
    const double component_ly = component.LLY() * circuit->GridValueY();
    const double component_width = component.Width() * circuit->GridValueX();
    const double component_height = component.Height() * circuit->GridValueY();
    components_.push_back(
        {component.Id(), static_cast<float>(component_lx),
         static_cast<float>(component_ly), static_cast<float>(component_width),
         static_cast<float>(component_height), component.IsFixed(),
         component.Orient(), kind, false});
    view_llx_ = std::min(view_llx_, component_lx);
    view_lly_ = std::min(view_lly_, component_ly);
    view_urx_ = std::max(view_urx_, component_lx + component_width);
    view_ury_ = std::max(view_ury_, component_ly + component_height);
  }
}

void PlacementCanvas::AppendIoPins(Circuit* circuit) {
  for (const IoPin& io_pin : circuit->IoPins()) {
    if (!io_pin.IsPlaced()) {
      continue;
    }
    const double lx = io_pin.LX() * circuit->GridValueX();
    const double ly = io_pin.LY() * circuit->GridValueY();
    const double ux = io_pin.UX() * circuit->GridValueX();
    const double uy = io_pin.UY() * circuit->GridValueY();
    io_pins_.push_back({static_cast<float>(lx), static_cast<float>(ly),
                        static_cast<float>(ux), static_cast<float>(uy),
                        io_pin.Status() == FIXED || io_pin.Status() == COVER});
    view_llx_ = std::min(view_llx_, lx);
    view_lly_ = std::min(view_lly_, ly);
    view_urx_ = std::max(view_urx_, ux);
    view_ury_ = std::max(view_ury_, uy);
  }
}

QRectF PlacementCanvas::ComponentScreenRect(
    const SnapshotComponent& component) const {
  return QRectF(WorldToScreenX(component.x),
                WorldToScreenY(component.y + component.height),
                std::max(component.width * scale_, 0.6),
                std::max(component.height * scale_, 0.6));
}

QRectF PlacementCanvas::WellScreenRect(
    const SnapshotWellRect& well_rect) const {
  return QRectF(WorldToScreenX(well_rect.lx), WorldToScreenY(well_rect.uy),
                std::max((well_rect.ux - well_rect.lx) * scale_, 0.6),
                std::max((well_rect.uy - well_rect.ly) * scale_, 0.6));
}

void PlacementCanvas::DrawWellRects(QPainter* painter) const {
  const QRectF visible_area = VisiblePlacementArea();
  for (const SnapshotWellRect& well_rect : DrawnWells()) {
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

PlacementCanvas::ComponentPaint PlacementCanvas::PaintFor(
    const SnapshotComponent& component) const {
  ComponentPaint paint;

  // A selected timing path outranks every other layer: it is the thing the
  // user just asked to see, and a cell on it that kept its ordinary colour
  // would be a cell the highlight silently lost. An inserted delay-line cell
  // on the slow path is the one exception, and it keeps its dark orange
  // below.
  const PathClass path_class = ClassOf(component.component_id);
  if (path_class != PathClass::kNone &&
      !(component.is_topology_added && component.is_delay_line &&
        path_class == PathClass::kSlow)) {
    paint.path_class = path_class;
    paint.edge_width = 2;
    switch (path_class) {
      case PathClass::kFast:
        paint.fill = kFastPathFill;
        paint.edge = kFastPathEdge;
        break;
      case PathClass::kSlow:
        paint.fill = kSlowPathFill;
        paint.edge = kSlowPathEdge;
        break;
      default:
        paint.fill = kSharedPathFill;
        paint.edge = kSharedPathEdge;
        break;
    }
    paint.dot_fill = paint.fill;
    return paint;
  }
  if (has_highlight_ && fade_unrelated_) {
    paint.faded = true;
    paint.fill = kFadedFill;
    paint.dot_fill = kFadedDotFill;
    paint.edge = kFadedEdge;
    return paint;
  }

  if (component.kind == SnapshotComponentKind::kWellTap) {
    paint.fill = QColor(245, 158, 11, 220);
    paint.edge = QColor(146, 64, 14);
  } else if (component.kind == SnapshotComponentKind::kEndCap) {
    paint.fill = QColor(16, 185, 129, 220);
    paint.edge = QColor(6, 95, 70);
  } else if (component.is_delay_line && show_delay_lines_) {
    // Delay-line membership wins over topology-added, so the Delay lines
    // toggle on its own separates the line a run started with from the cells
    // it grew: same hue, clearly darker for the inserted ones. Colouring an
    // inserted member cyan instead put it in the same class as any other
    // added cell and lost which line it belonged to.
    if (component.is_topology_added) {
      paint.fill = kInsertedDelayLineFill;
      paint.edge = kInsertedDelayLineEdge;
    } else {
      paint.fill = kExistingDelayLineFill;
      paint.edge = kExistingDelayLineEdge;
    }
  } else if (component.is_topology_added && show_topology_added_) {
    paint.fill = kAddedCellFill;
    paint.edge = kAddedCellEdge;
    paint.edge_width = 2;
  } else {
    paint.fill = component.fixed ? QColor(76, 86, 106, 170)
                                 : QColor(136, 192, 208, 190);
    paint.edge = component.fixed ? QColor(35, 42, 55) : QColor(66, 94, 111);
    // A dot is a few pixels across, so it carries its own denser base colour;
    // the rectangle's paler fill would disappear against the canvas ground.
    paint.dot_fill = kPlainDotFill;
    return paint;
  }
  paint.dot_fill = paint.fill;
  return paint;
}

void PlacementCanvas::DrawComponent(QPainter* painter,
                                    const SnapshotComponent& component) const {
  const QRectF rect = ComponentScreenRect(component);
  if (!rect.intersects(VisiblePlacementArea())) {
    return;
  }

  const ComponentPaint paint = PaintFor(component);
  painter->setBrush(paint.fill);
  painter->setPen(QPen(paint.edge, paint.edge_width));
  painter->drawRect(rect);
  if (paint.path_class != PathClass::kNone) {
    DrawEndpointMarker(painter, rect, component.component_id);
    return;
  }
  if (paint.faded) {
    return;
  }

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

void PlacementCanvas::DrawMovableDots(QPainter* painter) const {
  const QRectF visible_area = VisiblePlacementArea();
  for (const SnapshotComponent& component : Drawn()) {
    if (component.fixed) {
      continue;
    }
    const double center_x = WorldToScreenX(component.x + component.width / 2.0);
    const double center_y =
        WorldToScreenY(component.y + component.height / 2.0);
    if (!visible_area.contains(QPointF(center_x, center_y))) {
      continue;
    }
    const ComponentPaint paint = PaintFor(component);
    const double radius = std::clamp(
        std::min(component.width, component.height) * scale_ * 0.4, 1.0, 2.2);
    painter->setPen(Qt::NoPen);
    painter->setBrush(paint.dot_fill);
    painter->drawEllipse(QPointF(center_x, center_y), radius, radius);
    // A dot on the selected path is small enough to lose, so it keeps the
    // endpoint ring the cell rectangles draw rather than relying on colour
    // alone at two pixels across.
    if (paint.path_class != PathClass::kNone) {
      DrawEndpointMarker(
          painter,
          QRectF(center_x - radius, center_y - radius, radius * 2, radius * 2),
          component.component_id);
    }
  }
}

void PlacementCanvas::DrawEndpointMarker(QPainter* painter, const QRectF& rect,
                                         int component_id) const {
  if (std::find(endpoint_ids_.begin(), endpoint_ids_.end(), component_id) ==
      endpoint_ids_.end()) {
    return;
  }
  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(QColor(17, 24, 39), 2));
  const double radius = std::max(rect.width(), rect.height()) * 0.9 + 2.0;
  painter->drawEllipse(rect.center(), radius, radius);
}

void PlacementCanvas::DrawHighlightEdges(QPainter* painter) const {
  if (!has_highlight_) return;
  std::unordered_map<int, QPointF> centers;
  for (const SnapshotComponent& component : Drawn()) {
    centers[component.component_id] =
        QPointF(WorldToScreenX(component.x + component.width / 2.0),
                WorldToScreenY(component.y + component.height / 2.0));
  }
  for (const HighlightEdge& edge : highlight_edges_) {
    if (edge.kind == PathClass::kFast && !show_fast_path_) continue;
    if (edge.kind == PathClass::kSlow && !show_slow_path_) continue;
    if (edge.kind == PathClass::kShared && !show_shared_path_) continue;
    const auto from = centers.find(edge.from);
    const auto to = centers.find(edge.to);
    if (from == centers.end() || to == centers.end()) continue;
    QColor colour = kSharedPathFill;
    if (edge.kind == PathClass::kFast) colour = kFastPathFill;
    if (edge.kind == PathClass::kSlow) colour = kSlowPathFill;
    painter->setPen(QPen(colour, 2));
    painter->drawLine(from->second, to->second);
  }
}

void PlacementCanvas::DrawDelayLineNets(QPainter* painter) const {
  std::unordered_map<int, QPointF> centers;
  for (const SnapshotComponent& component : Drawn()) {
    if (!component.is_delay_line) {
      continue;
    }
    centers.emplace(
        component.component_id,
        QPointF(WorldToScreenX(component.x + component.width / 2.0),
                WorldToScreenY(component.y + component.height / 2.0)));
  }

  const double width = std::clamp(scale_ * 0.7, 1.0, 3.0);
  painter->setBrush(Qt::NoBrush);
  painter->setPen(QPen(QColor(220, 38, 38, 210), width, Qt::DashLine,
                       Qt::RoundCap, Qt::RoundJoin));
  for (const PlacementDelayLineVisualization& delay_line : delay_lines_) {
    for (const auto& edge : delay_line.component_edges) {
      const auto source = centers.find(edge.first);
      const auto target = centers.find(edge.second);
      if (source == centers.end() || target == centers.end()) {
        continue;
      }
      painter->drawLine(source->second, target->second);
    }
  }
}

void PlacementCanvas::DrawIoPins(QPainter* painter) const {
  const QRectF visible_area = VisiblePlacementArea();
  for (const SnapshotIoPin& io_pin : DrawnIoPins()) {
    QRectF rect(WorldToScreenX(io_pin.lx), WorldToScreenY(io_pin.uy),
                std::max((io_pin.ux - io_pin.lx) * scale_, 0.0),
                std::max((io_pin.uy - io_pin.ly) * scale_, 0.0));
    if (rect.width() < 4.0) {
      const double center = rect.center().x();
      rect.setLeft(center - 2.0);
      rect.setRight(center + 2.0);
    }
    if (rect.height() < 4.0) {
      const double center = rect.center().y();
      rect.setTop(center - 2.0);
      rect.setBottom(center + 2.0);
    }
    if (!rect.intersects(visible_area)) {
      continue;
    }
    painter->setBrush(io_pin.fixed ? QColor(219, 39, 119, 225)
                                   : QColor(14, 165, 233, 225));
    painter->setPen(
        QPen(io_pin.fixed ? QColor(131, 24, 67) : QColor(3, 105, 161), 1));
    painter->drawRect(rect);
  }
}

double PlacementCanvas::WorldToScreenX(double x) const {
  return pan_x_ + x * scale_;
}

double PlacementCanvas::WorldToScreenY(double y) const {
  return pan_y_ - y * scale_;
}

double PlacementCanvas::ScreenToWorldX(double x) const {
  return (x - pan_x_) / scale_;
}

double PlacementCanvas::ScreenToWorldY(double y) const {
  return (pan_y_ - y) / scale_;
}

HpwlHistoryPanel::HpwlHistoryPanel(QWidget* parent) : QWidget(parent) {
  setMinimumWidth(330);
  setAutoFillBackground(true);
}

void HpwlHistoryPanel::SetStages(std::vector<PlacementSnapshotStage> stages) {
  stages_ = std::move(stages);
  update();
}

void HpwlHistoryPanel::AddSnapshot(Circuit* circuit,
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

void HpwlHistoryPanel::paintEvent(QPaintEvent* /*event*/) {
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

int HpwlHistoryPanel::NextIteration(const std::vector<HpwlSample>& samples) {
  return samples.empty() ? 0 : samples.back().iteration + 1;
}

bool HpwlHistoryPanel::CollectBounds(const ChartSection& section,
                                     int* min_iteration, int* max_iteration,
                                     double* min_hpwl, double* max_hpwl) {
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

void HpwlHistoryPanel::DrawSection(QPainter* painter, const QRectF& area,
                                   const ChartSection& section) const {
  painter->setPen(QPen(QColor(229, 231, 235), 1));
  painter->setBrush(QColor(255, 255, 255));
  painter->drawRect(area);

  painter->setPen(QColor(31, 41, 55));
  painter->drawText(QPointF(area.left() + 10, area.top() + 20), section.title);

  int min_iteration = 0;
  int max_iteration = 0;
  double min_hpwl = 0;
  double max_hpwl = 0;
  if (!CollectBounds(section, &min_iteration, &max_iteration, &min_hpwl,
                     &max_hpwl)) {
    // Reserved but not yet populated: label the empty slot so the viewer sees
    // the stage is pending rather than an unexplained blank box.
    painter->setPen(QColor(156, 163, 175));
    painter->drawText(QRectF(area.left() + 10, area.top() + 26,
                             area.width() - 20, area.height() - 34),
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

void HpwlHistoryPanel::DrawSeries(QPainter* painter, const QRectF& plot,
                                  const Series& series, int min_iteration,
                                  int max_iteration, double min_hpwl,
                                  double max_hpwl) {
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

ElidingStatusLabel::ElidingStatusLabel(QWidget* parent) : QLabel(parent) {
  setTextFormat(Qt::PlainText);
  setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
  setMinimumWidth(0);
}

void ElidingStatusLabel::SetStatus(const QString& compact,
                                   const QString& detail) {
  compact_status_ = compact;
  full_status_ = detail.isEmpty() ? compact : detail;
  setToolTip(full_status_);
  setAccessibleDescription(full_status_);
  // QLabel::text() stays the compact form so anything reading it sees what is
  // on screen; the whole status is reachable through the tooltip and through
  // FullStatus().
  QLabel::setText(compact_status_);
  update();
}

QSize ElidingStatusLabel::minimumSizeHint() const {
  const QFontMetrics metrics(font());
  return QSize(0, metrics.height() + 4);
}

QSize ElidingStatusLabel::sizeHint() const { return minimumSizeHint(); }

void ElidingStatusLabel::paintEvent(QPaintEvent* /*event*/) {
  QPainter painter(this);
  const QFontMetrics metrics(painter.font());
  const QString shown =
      metrics.elidedText(compact_status_, Qt::ElideRight, width());
  painter.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, shown);
}

QtPlacementWindow::QtPlacementWindow(QWidget* parent) : QWidget(parent) {
  setWindowTitle("Dali Live Placement Debug");
  if (QCoreApplication::instance() != nullptr) {
    QCoreApplication::instance()->installEventFilter(this);
  }

  auto* layout = new QVBoxLayout(this);
  status_label_ = new ElidingStatusLabel(this);
  status_label_->SetStatus(
      "Waiting for first placement snapshot",
      "Waiting for first placement snapshot. Mouse wheel zooms, left-drag "
      "pans.");
  canvas_ = new PlacementCanvas(this);
  pause_checkbox_ = new QCheckBox("Pause at every snapshot", this);
  pause_checkbox_->setChecked(true);
  movable_dot_checkbox_ = new QCheckBox("Movable dots", this);
  io_pin_checkbox_ = new QCheckBox("I/O pins", this);
  displacement_global_checkbox_ = new QCheckBox("Displacement vs global", this);
  displacement_previous_checkbox_ =
      new QCheckBox("Displacement vs previous", this);
  movable_dot_checkbox_->setChecked(true);
  io_pin_checkbox_->setChecked(true);
  const char* show_delay_lines = std::getenv("DALI_GUI_SHOW_DELAY_LINES");
  const bool show_delay_lines_by_default =
      show_delay_lines != nullptr &&
      (QString::fromUtf8(show_delay_lines) == "1" ||
       QString::fromUtf8(show_delay_lines)
               .compare("true", Qt::CaseInsensitive) == 0);
  canvas_->SetShowDelayLines(show_delay_lines_by_default);
  const char* show_topology_added = std::getenv("DALI_GUI_SHOW_TOPOLOGY_ADDED");
  const bool show_topology_added_by_default =
      show_topology_added != nullptr &&
      (QString::fromUtf8(show_topology_added) == "1" ||
       QString::fromUtf8(show_topology_added)
               .compare("true", Qt::CaseInsensitive) == 0);
  canvas_->SetShowTopologyAdded(show_topology_added_by_default);
  animate_checkbox_ = new QCheckBox("Animate transitions", this);
  animate_checkbox_->setChecked(true);
  // Duration in milliseconds, shown inverted so dragging right is faster.
  animation_slider_ = new QSlider(Qt::Horizontal, this);
  animation_slider_->setRange(kMinAnimationMs, kMaxAnimationMs);
  animation_slider_->setValue(kDefaultAnimationMs);
  animation_slider_->setInvertedAppearance(true);
  animation_slider_->setFixedWidth(110);
  animation_slider_->setToolTip("Transition duration: drag right for faster");
  animation_spin_ = new QSpinBox(this);
  animation_spin_->setRange(kMinAnimationMs, kMaxAnimationMs);
  animation_spin_->setValue(kDefaultAnimationMs);
  animation_spin_->setSuffix(" ms");
  animation_spin_->setSingleStep(50);
  animation_spin_->setFixedWidth(90);
  animation_spin_->setToolTip("Transition duration in milliseconds");
  step_button_ = new QPushButton("Step", this);
  continue_button_ = new QPushButton("Continue", this);
  fit_button_ = new QPushButton("Fit", this);
  save_button_ = new QPushButton("Save PNG", this);
  hpwl_panel_ = new HpwlHistoryPanel(this);

  // Three short rows by what a control does, rather than one strip holding
  // every checkbox and button. The old single View row ran wider than the
  // canvas minimum at the capture size, which is what pushed the window out.
  step_button_->setIcon(style()->standardIcon(QStyle::SP_MediaSeekForward));
  step_button_->setToolTip("Advance one snapshot");
  continue_button_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
  continue_button_->setToolTip("Run to the end without pausing");
  fit_button_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
  fit_button_->setToolTip("Fit the whole design in the canvas");
  save_button_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
  save_button_->setToolTip("Save the canvas and the diagnostics pane as PNGs");

  auto* playback_controls = new QHBoxLayout();
  playback_controls->addWidget(MakeGroupLabel("Playback:"));
  playback_controls->addWidget(pause_checkbox_);
  playback_controls->addWidget(step_button_);
  playback_controls->addWidget(continue_button_);
  playback_controls->addStretch();

  auto* view_controls = new QHBoxLayout();
  view_controls->addWidget(MakeGroupLabel("View:"));
  view_controls->addWidget(movable_dot_checkbox_);
  view_controls->addWidget(io_pin_checkbox_);
  view_controls->addWidget(displacement_global_checkbox_);
  view_controls->addWidget(displacement_previous_checkbox_);
  view_controls->addStretch();
  view_controls->addWidget(fit_button_);
  view_controls->addWidget(save_button_);

  auto* animation_controls = new QHBoxLayout();
  animation_controls->addWidget(MakeGroupLabel("Animation:"));
  animation_controls->addWidget(animate_checkbox_);
  animation_controls->addWidget(animation_slider_);
  animation_controls->addWidget(animation_spin_);
  animation_controls->addStretch();

  // A splitter, so the diagnostics pane can be resized and never covers the
  // canvas. The tab widget starts with HPWL alone; the Timing tab is added
  // the first time Dali publishes timing metadata, so an ordinary placement
  // run looks exactly as it did.
  timing_panel_ = new TimingDiagnosticsPanel(this);
  timing_panel_->on_selection =
      [this](const PlacementTimingPathVisualization* path) {
        canvas_->SetHighlightedPath(path);
      };
  // The pane reports plain booleans; the window is what knows there is a
  // canvas. Keeping it this way round is why the pane can own the controls
  // without owning the thing they draw on.
  timing_panel_->on_show_delay_lines = [this](bool on) {
    canvas_->SetShowDelayLines(on);
  };
  timing_panel_->on_show_topology_added = [this](bool on) {
    canvas_->SetShowTopologyAdded(on);
  };
  timing_panel_->DelayLineCheckbox()->setChecked(show_delay_lines_by_default);
  timing_panel_->TopologyAddedCheckbox()->setChecked(
      show_topology_added_by_default);
  timing_panel_->on_fade_unrelated = [this](bool on) {
    canvas_->SetFadeUnrelated(on);
  };
  timing_panel_->on_show_fast_path = [this](bool on) {
    canvas_->SetShowFastPath(on);
  };
  timing_panel_->on_show_slow_path = [this](bool on) {
    canvas_->SetShowSlowPath(on);
  };
  timing_panel_->on_show_shared_path = [this](bool on) {
    canvas_->SetShowSharedPath(on);
  };
  diagnostics_tabs_ = new QTabWidget(this);
  diagnostics_tabs_->addTab(hpwl_panel_, "HPWL");

  diagnostics_splitter_ = new QSplitter(Qt::Horizontal, this);
  diagnostics_splitter_->addWidget(canvas_);
  diagnostics_splitter_->addWidget(diagnostics_tabs_);
  diagnostics_splitter_->setStretchFactor(0, 1);
  diagnostics_splitter_->setStretchFactor(1, 0);
  diagnostics_splitter_->setChildrenCollapsible(false);

  auto* content = new QHBoxLayout();
  content->addWidget(diagnostics_splitter_, 1);

  layout->addWidget(status_label_);
  layout->addLayout(content, 1);
  layout->addLayout(playback_controls);
  layout->addLayout(view_controls);
  layout->addLayout(animation_controls);

  QObject::connect(step_button_, &QPushButton::clicked, this,
                   [this]() { step_requested_ = true; });
  QObject::connect(continue_button_, &QPushButton::clicked, this, [this]() {
    pause_checkbox_->setChecked(false);
    continue_requested_ = true;
  });
  // Slider and box are two views of one value; guard against the round trip.
  QObject::connect(animation_slider_, &QSlider::valueChanged, this,
                   [this](int value) {
                     if (animation_spin_->value() != value) {
                       animation_spin_->setValue(value);
                     }
                   });
  QObject::connect(animation_spin_, QOverload<int>::of(&QSpinBox::valueChanged),
                   this, [this](int value) {
                     if (animation_slider_->value() != value) {
                       animation_slider_->setValue(value);
                     }
                   });
  QObject::connect(fit_button_, &QPushButton::clicked, this,
                   [this]() { canvas_->FitToView(); });
  QObject::connect(
      movable_dot_checkbox_, &QCheckBox::toggled, this,
      [this](bool checked) { canvas_->SetMovableDotMode(checked); });
  QObject::connect(io_pin_checkbox_, &QCheckBox::toggled, this,
                   [this](bool checked) { canvas_->SetShowIoPins(checked); });
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

void QtPlacementWindow::SetPauseAtEverySnapshot(bool pause) {
  pause_checkbox_->setChecked(pause);
}

void QtPlacementWindow::SetStages(std::vector<PlacementSnapshotStage> stages) {
  hpwl_panel_->SetStages(std::move(stages));
}

void QtPlacementWindow::SetSnapshot(Circuit* circuit,
                                    const PlacementSnapshotMetadata& metadata) {
  step_requested_ = false;
  continue_requested_ = false;
  current_snapshot_label_ = QString::fromStdString(metadata.id);
  compact_snapshot_label_ = QString::fromStdString(metadata.id);
  if (metadata.iteration >= 0) {
    compact_snapshot_label_ +=
        QString(" \u2022 iteration %1").arg(metadata.iteration);
  }
  // The group is worth showing only when it says something the id does not;
  // on the final snapshot both are "final", which read as "final . final".
  if (!metadata.group.empty() && metadata.group != metadata.id) {
    compact_snapshot_label_ +=
        QString(" \u2022 %1").arg(QString::fromStdString(metadata.group));
  }
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
  if (metadata.is_delay_line_feedback) {
    current_snapshot_label_ +=
        QString("  line %1  slack %2 ps  separation %3 -> %4")
            .arg(QString::fromStdString(metadata.delay_line_name))
            .arg(metadata.delay_line_binding_slack_ps, 0, 'f', 3)
            .arg(metadata.delay_line_old_separation)
            .arg(metadata.delay_line_new_separation);
  }
  if (!metadata.topology_changes.empty()) {
    QStringList changes;
    for (const PlacementTopologySiteChange& change :
         metadata.topology_changes) {
      QString text = QString("%1 %2 -> %3")
                         .arg(QString::fromStdString(change.site))
                         .arg(change.current_pairs)
                         .arg(change.requested_pairs);
      if (change.has_boundary_slack) {
        text += QString(" (%1 ps)").arg(change.boundary_slack_ps, 0, 'f', 3);
      }
      changes << text;
    }
    current_snapshot_label_ += QString("  topology generation %1  %2")
                                   .arg(metadata.topology_generation)
                                   .arg(changes.join(", "));
    compact_snapshot_label_ +=
        QString(" \u2022 topology generation %1 \u2022 %2 site change%3")
            .arg(metadata.topology_generation)
            .arg(changes.size())
            .arg(changes.size() == 1 ? "" : "s");
  }
  // Compact on screen, complete in the tooltip. Nothing is discarded: the full
  // label, every site and slack, is what the label carries as its detail.
  status_label_->SetStatus(compact_snapshot_label_, current_snapshot_label_);
  canvas_->SetSnapshot(circuit, metadata);
  hpwl_panel_->AddSnapshot(circuit, metadata);
  if (!metadata.delay_line_timing.empty() ||
      !metadata.unattributed_constraints.empty() ||
      !metadata.ambiguous_constraints.empty()) {
    timing_panel_->SetTimingData(
        metadata.timing_sample_stage, metadata.delay_line_timing,
        metadata.unattributed_constraints, metadata.ambiguous_constraints,
        metadata.sizing_decisions);
    EnsureTimingTab();
  }
  if (!has_shown_a_snapshot_) {
    has_shown_a_snapshot_ = true;
    raise();
    activateWindow();
  }
}

void QtPlacementWindow::EnsureTimingTab() {
  if (timing_tab_index_ >= 0) return;
  timing_tab_index_ = diagnostics_tabs_->addTab(timing_panel_, "Timing");
}

bool QtPlacementWindow::ShowTimingTab() {
  if (timing_tab_index_ < 0) return false;
  diagnostics_tabs_->setCurrentIndex(timing_tab_index_);
  return true;
}

QCheckBox* QtPlacementWindow::FadeUnrelatedCheckbox() {
  return timing_panel_->FadeUnrelatedCheckbox();
}

ElidingStatusLabel* QtPlacementWindow::StatusLabel() {
  return status_label_;
}

PlacementCanvas* QtPlacementWindow::Canvas() { return canvas_; }

TimingDiagnosticsPanel* QtPlacementWindow::TimingPanel() {
  return timing_panel_;
}

QTabWidget* QtPlacementWindow::DiagnosticsTabs() { return diagnostics_tabs_; }

QSplitter* QtPlacementWindow::DiagnosticsSplitter() {
  return diagnostics_splitter_;
}

int QtPlacementWindow::TimingTabIndex() const { return timing_tab_index_; }

bool QtPlacementWindow::ShouldPause() const {
  return pause_checkbox_->isChecked();
}

bool QtPlacementWindow::ShouldResume() const {
  return step_requested_ || continue_requested_;
}

void QtPlacementWindow::MarkFinished() {
  const QString finished =
      QString(" | placement finished; close window to exit, or it closes "
              "itself after %1 s untouched")
          .arg(idle_close_seconds_, 0, 'f', 0);
  status_label_->SetStatus(compact_snapshot_label_ + finished,
                           current_snapshot_label_ + finished);
  // The minute is counted from the final frame, not from whatever the operator
  // last did during the run.
  NoteUserActivity();
}

bool QtPlacementWindow::eventFilter(QObject* watched, QEvent* event) {
  switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
    case QEvent::Wheel:
    case QEvent::KeyPress:
    case QEvent::KeyRelease:
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
      NoteUserActivity();
      break;
    default:
      break;
  }
  return QWidget::eventFilter(watched, event);
}

void QtPlacementWindow::NoteUserActivity() {
  last_user_activity_ = std::chrono::steady_clock::now();
}

double QtPlacementWindow::IdleSeconds() const {
  return std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                       last_user_activity_)
      .count();
}

bool QtPlacementWindow::HasGoneIdle() const {
  return IdleSeconds() >= idle_close_seconds_;
}

void QtPlacementWindow::SetIdleCloseSeconds(double seconds) {
  idle_close_seconds_ = seconds;
}

void QtPlacementWindow::WaitUntilClosedOrIdle() {
  while (isVisible()) {
    QApplication::processEvents();
    if (HasGoneIdle()) {
      LOG(info) << "Placement finished and the window went untouched for "
                << idle_close_seconds_ << " s; closing it\n";
      close();
      QApplication::processEvents();
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }
}

void QtPlacementWindow::AnimateToSnapshot() {
  if (canvas_ == nullptr || animate_checkbox_ == nullptr) return;
  if (!animate_checkbox_->isChecked()) return;
  const int duration_ms = animation_spin_->value();
  if (duration_ms <= 0 || !canvas_->BeginTransition()) return;

  const auto start = std::chrono::steady_clock::now();
  for (;;) {
    if (!isVisible()) break;
    const double elapsed = std::chrono::duration<double, std::milli>(
                               std::chrono::steady_clock::now() - start)
                               .count();
    const double t = std::min(1.0, elapsed / duration_ms);
    canvas_->SetAnimationProgress(EaseInOut(t));
    QApplication::processEvents();
    if (t >= 1.0) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }
  canvas_->EndAnimation();
  QApplication::processEvents();
}

double QtPlacementWindow::EaseInOut(double t) {
  if (t < 0.5) return 4.0 * t * t * t;
  const double f = -2.0 * t + 2.0;
  return 1.0 - f * f * f / 2.0;
}

QLabel* QtPlacementWindow::MakeGroupLabel(const QString& text) {
  auto* label = new QLabel(text, this);
  QFont font = label->font();
  font.setBold(true);
  label->setFont(font);
  return label;
}

void QtPlacementWindow::SaveCurrentImages() {
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
  const QString placement_path = UniqueImagePath(dir, base_name + "_placement");
  const QString hpwl_path = UniqueImagePath(dir, base_name + "_hpwl");

  const bool placement_saved = canvas_->grab().save(placement_path, "PNG");
  const bool hpwl_saved = hpwl_panel_->grab().save(hpwl_path, "PNG");
  if (!placement_saved || !hpwl_saved) {
    QMessageBox::warning(this, "Save Dali GUI images",
                         "Failed to save one or more PNG files.");
    return;
  }

  const QString saved = QString("Saved %1 and %2")
                           .arg(QFileInfo(placement_path).fileName())
                           .arg(QFileInfo(hpwl_path).fileName());
  status_label_->SetStatus(saved, saved);
}

}  // namespace dali
