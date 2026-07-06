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
#include "dali/gui/qt_placement_snapshot_sink.h"

#include <QApplication>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
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
namespace {

struct SnapshotComponent {
  float x = 0;
  float y = 0;
  float width = 0;
  float height = 0;
  bool fixed = false;
};

std::string FormatHpwl(double hpwl) {
  std::ostringstream out;
  out.precision(8);
  out << hpwl;
  return out.str();
}

QString FormatCompactHpwl(double hpwl) {
  if (std::abs(hpwl) >= 1e6) {
    return QString("%1M").arg(hpwl / 1e6, 0, 'f', 2);
  }
  if (std::abs(hpwl) >= 1e3) {
    return QString("%1K").arg(hpwl / 1e3, 0, 'f', 1);
  }
  return QString("%1").arg(hpwl, 0, 'f', 0);
}

}  // namespace

class PlacementCanvas : public QWidget {
 public:
  explicit PlacementCanvas(QWidget* parent = nullptr) : QWidget(parent) {
    setMinimumSize(900, 640);
    setAutoFillBackground(true);
    setMouseTracking(true);
  }

  void SetSnapshot(Circuit* circuit,
                   const PlacementSnapshotMetadata& metadata) {
    components_.clear();
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

    components_.reserve(circuit->Components().size());
    for (Component& component : circuit->Components()) {
      if (component.MacroPtr() == circuit->tech().IoDummyMacroPtr()) {
        continue;
      }
      const double component_lx = component.LLX() * circuit->GridValueX();
      const double component_ly = component.LLY() * circuit->GridValueY();
      const double component_width = component.Width() * circuit->GridValueX();
      const double component_height =
          component.Height() * circuit->GridValueY();
      components_.push_back(
          {static_cast<float>(component_lx), static_cast<float>(component_ly),
           static_cast<float>(component_width),
           static_cast<float>(component_height), component.IsFixed()});
      view_llx_ = std::min(view_llx_, component_lx);
      view_lly_ = std::min(view_lly_, component_ly);
      view_urx_ = std::max(view_urx_, component_lx + component_width);
      view_ury_ = std::max(view_ury_, component_ly + component_height);
    }

    title_ = metadata.id + " | HPWL " + FormatHpwl(circuit->WeightedHPWL());
    has_snapshot_ = true;
    if (!has_view_) {
      FitToView();
    }
    update();
  }

  void FitToView() {
    const double design_width = std::max(view_urx_ - view_llx_, 1.0);
    const double design_height = std::max(view_ury_ - view_lly_, 1.0);
    const double margin = 24.0;
    scale_ = std::min((width() - 2.0 * margin) / design_width,
                      (height() - 2.0 * margin) / design_height);
    scale_ = std::max(scale_, 1e-9);
    pan_x_ = margin - view_llx_ * scale_;
    pan_y_ = height() - margin + view_lly_ * scale_;
    has_view_ = true;
    update();
  }

 protected:
  void paintEvent(QPaintEvent* /*event*/) override {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(248, 249, 251));
    painter.setRenderHint(QPainter::Antialiasing, false);

    const double boundary_width = std::max(boundary_urx_ - boundary_llx_, 1.0);
    const double boundary_height = std::max(boundary_ury_ - boundary_lly_, 1.0);

    painter.setPen(QPen(QColor(40, 48, 60), 1));
    painter.drawRect(QRectF(WorldToScreenX(boundary_llx_),
                            WorldToScreenY(boundary_ury_),
                            boundary_width * scale_, boundary_height * scale_));

    painter.setPen(Qt::NoPen);
    for (const SnapshotComponent& component : components_) {
      painter.setBrush(component.fixed ? QColor(76, 86, 106, 210)
                                       : QColor(136, 192, 208, 190));
      QRectF rect(WorldToScreenX(component.x),
                  WorldToScreenY(component.y + component.height),
                  std::max(component.width * scale_, 0.6),
                  std::max(component.height * scale_, 0.6));
      painter.drawRect(rect);
    }

    painter.setPen(QColor(31, 41, 55));
    painter.drawText(QPointF(16, 22), QString::fromStdString(title_));
    painter.drawText(QPointF(16, 42),
                     QString("zoom %1 px/um").arg(scale_, 0, 'g', 4));
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
  double WorldToScreenX(double x) const { return pan_x_ + x * scale_; }
  double WorldToScreenY(double y) const { return pan_y_ - y * scale_; }
  double ScreenToWorldX(double x) const { return (x - pan_x_) / scale_; }
  double ScreenToWorldY(double y) const { return (pan_y_ - y) / scale_; }

  std::vector<SnapshotComponent> components_;
  std::string title_ = "Waiting for first placement snapshot";
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

    std::vector<ChartSection> sections;
    if (!global_lower_.empty() || !global_upper_.empty()) {
      sections.push_back(
          {"Global placement",
           {Series{"lower bound", global_lower_, QColor(37, 99, 235)},
            Series{"upper bound", global_upper_, QColor(220, 38, 38)}}});
    }
    if (!detailed_.empty()) {
      sections.push_back({"Detailed placement",
                          {Series{"HPWL", detailed_, QColor(22, 163, 74)}}});
    }
    if (!legalization_.empty()) {
      sections.push_back(
          {"Legalization",
           {Series{"HPWL", legalization_, QColor(147, 51, 234)}}});
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
    step_button_ = new QPushButton("Step", this);
    continue_button_ = new QPushButton("Continue", this);
    fit_button_ = new QPushButton("Fit", this);
    hpwl_panel_ = new HpwlHistoryPanel(this);

    auto* controls = new QHBoxLayout();
    controls->addWidget(pause_checkbox_);
    controls->addWidget(step_button_);
    controls->addWidget(continue_button_);
    controls->addWidget(fit_button_);
    controls->addStretch();

    auto* content = new QHBoxLayout();
    content->addWidget(canvas_, 1);
    content->addWidget(hpwl_panel_);

    layout->addWidget(status_label_);
    layout->addLayout(content, 1);
    layout->addLayout(controls);

    QObject::connect(step_button_, &QPushButton::clicked, this,
                     [this]() { step_requested_ = true; });
    QObject::connect(continue_button_, &QPushButton::clicked, this, [this]() {
      pause_checkbox_->setChecked(false);
      continue_requested_ = true;
    });
    QObject::connect(fit_button_, &QPushButton::clicked, this,
                     [this]() { canvas_->FitToView(); });
  }

  void SetPauseAtEverySnapshot(bool pause) {
    pause_checkbox_->setChecked(pause);
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
    raise();
    activateWindow();
  }

  bool ShouldPause() const { return pause_checkbox_->isChecked(); }
  bool ShouldResume() const { return step_requested_ || continue_requested_; }
  void MarkFinished() {
    status_label_->setText(current_snapshot_label_ +
                           "  | placement finished; close window to exit");
  }

 private:
  QLabel* status_label_ = nullptr;
  PlacementCanvas* canvas_ = nullptr;
  HpwlHistoryPanel* hpwl_panel_ = nullptr;
  QCheckBox* pause_checkbox_ = nullptr;
  QPushButton* step_button_ = nullptr;
  QPushButton* continue_button_ = nullptr;
  QPushButton* fit_button_ = nullptr;
  QString current_snapshot_label_;
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
  window_->resize(1100, 760);
  window_->show();
  enabled_ = true;
  LOG(info) << "Dali GUI debug mode enabled for design " << metadata.design_name
            << "\n";
}

void QtPlacementSnapshotSink::PublishSnapshot(
    Circuit* circuit, const PlacementSnapshotMetadata& metadata) {
  if (!enabled_ || window_ == nullptr) {
    return;
  }

  window_->SetSnapshot(circuit, metadata);
  QApplication::processEvents();
  if (!window_->ShouldPause()) {
    return;
  }

  while (window_ != nullptr && window_->isVisible() &&
         !window_->ShouldResume()) {
    QApplication::processEvents();
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }
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
