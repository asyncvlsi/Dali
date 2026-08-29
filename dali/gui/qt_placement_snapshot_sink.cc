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
#include "dali/gui/qt_placement_window.h"

#include "dali/gui/qt_placement_snapshot_sink.h"

#include "dali/gui/qt_timing_diagnostics_panel.h"

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
#include <QSlider>
#include <QCoreApplication>
#include <QFile>
#include <QSplitter>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTreeView>
#include <QHeaderView>
#include <QSpinBox>
#include <QStringList>
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
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/common/logging.h"


namespace dali {

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
      // Split at the first colon only. The region and its suffixes may contain
      // further colons -- `select=dl2:#114` does -- and a plain split() that
      // demanded exactly two parts dropped those requests without a word,
      // producing a capture set quietly missing the frames that were asked for.
      const int separator = parts[i].indexOf(':');
      if (separator < 0) {
        LOG(error) << "Ignoring GUI capture request '" << parts[i].toStdString()
                   << "': expected <stem>@<snapshot id>:<region>\n";
        continue;
      }
      const QStringList key_and_region = {parts[i].left(separator),
                                          parts[i].mid(separator + 1)};
      const QStringList stem_and_id = key_and_region[0].split('@');
      if (stem_and_id.size() != 2) {
        LOG(error) << "Ignoring GUI capture request '" << parts[i].toStdString()
                   << "': expected <stem>@<snapshot id>\n";
        continue;
      }
      // Suffixes are order-independent, so a request reads the way it is meant
      // rather than in whatever order the parser happened to strip them.
      QStringList suffixes = key_and_region[1].split('+');
      const QString region = suffixes.takeFirst();
      Request request;
      request.id = stem_and_id[1];
      request.stem = stem_and_id[0];
      bool understood = true;
      for (const QString& suffix : suffixes) {
        if (suffix == "cells") {
          request.draw_cells = true;
        } else if (suffix == "window") {
          request.target = CaptureTarget::kWindow;
        } else if (suffix == "timing") {
          request.target = CaptureTarget::kTimingPane;
        } else if (suffix == "fade") {
          request.fade_unrelated = true;
        } else if (suffix.startsWith("select=")) {
          // `select=<line>:worst` or `select=<line>:#<id>`.
          const QString value = suffix.mid(QString("select=").size());
          const int colon = value.indexOf(':');
          if (colon < 0) { understood = false; break; }
          request.select_line = value.left(colon).toStdString();
          const QString which = value.mid(colon + 1);
          if (which == "worst") {
            request.select_worst = true;
          } else if (which.startsWith('#')) {
            bool ok = false;
            request.select_constraint_id = which.mid(1).toInt(&ok);
            if (!ok) { understood = false; break; }
          } else {
            understood = false;
            break;
          }
        } else {
          understood = false;
          break;
        }
      }
      // A request nobody can satisfy is dropped loudly rather than captured
      // wrongly: a silently ignored suffix would produce a frame that looks
      // like the one asked for and is not.
      if (!understood) {
        LOG(error) << "Ignoring GUI capture request '"
                   << parts[i].toStdString() << "': unrecognised option\n";
        continue;
      }
      if (region == "fit") {
        request.region = QRectF(0.0, 0.0, 1.0, 1.0);
        result.requests_.push_back(std::move(request));
        continue;
      }
      const QStringList bounds = region.split(',');
      if (bounds.size() != 4) {
        LOG(error) << "Ignoring GUI capture request '" << parts[i].toStdString()
                   << "': region must be `fit` or four fractions\n";
        continue;
      }
      request.region =
          QRectF(QPointF(bounds[0].toDouble(), bounds[1].toDouble()),
                 QPointF(bounds[2].toDouble(), bounds[3].toDouble()));
      result.requests_.push_back(std::move(request));
    }
    return result;
  }

  /**
   * Write `<dir>/<stem>.png` for every request naming this snapshot.
   *
   * Nothing touches the filesystem until a request is known to match, so an
   * ordinary interactive run -- which sets no requests and hence no directory
   * -- stays silent instead of reporting a directory it was never asked to
   * create, once per snapshot.
   */
  void CaptureIfRequested(QtPlacementWindow* window,
                          const std::string& snapshot_id) const {
    if (window == nullptr) return;
    PlacementCanvas* canvas = window->Canvas();
    if (canvas == nullptr) return;
    const QString id = QString::fromStdString(snapshot_id);
    const bool any_requested =
        std::any_of(requests_.begin(), requests_.end(),
                    [&id](const Request& request) { return request.id == id; });
    if (!any_requested) return;
    if (!QDir().mkpath(directory_)) {
      LOG(error) << "Cannot create GUI snapshot directory "
                 << directory_.toStdString() << "\n";
      return;
    }
    for (const Request& request : requests_) {
      if (request.id != id) continue;
      const QString path = directory_ + "/" + request.stem + ".png";
      const QRectF design = canvas->DesignBounds();
      const QRectF f = request.region;
      const double llx = design.left() + f.left() * design.width();
      const double lly = design.top() + f.top() * design.height();
      const double urx = design.left() + f.right() * design.width();
      const double ury = design.top() + f.bottom() * design.height();
      // Selection first, so the frame shows the state that was asked for. It
      // goes through the panel's ordinary Select path, the same one a click
      // uses, and touches nothing but the view.
      if (!request.select_line.empty() && window->TimingPanel() != nullptr) {
        const bool selected =
            request.select_worst
                ? window->TimingPanel()->SelectLineWorst(request.select_line)
                : window->TimingPanel()->SelectConstraintId(
                      request.select_line, request.select_constraint_id);
        if (!selected) {
          LOG(error) << "GUI capture " << request.stem.toStdString()
                     << ": no such selection for site '" << request.select_line
                     << "'\n";
          continue;
        }
      }
      if (request.target != CaptureTarget::kCanvas) {
        window->ShowTimingTab();
      }
      const bool previous_dot_mode = canvas->MovableDotMode();
      if (request.draw_cells) canvas->SetMovableDotMode(false);
      // Through the checkbox rather than the canvas setter, so a fade frame is
      // evidence about the control a user actually has.
      QCheckBox* fade = window->FadeUnrelatedCheckbox();
      const bool previous_fade = fade != nullptr && fade->isChecked();
      if (fade != nullptr && request.fade_unrelated != previous_fade) {
        fade->setChecked(request.fade_unrelated);
      }
      bool captured = false;
      if (request.target == CaptureTarget::kCanvas) {
        captured = canvas->CaptureRegion(path, llx, lly, urx, ury);
      } else {
        // The canvas is framed first so a window capture shows the same region
        // a canvas capture of the same request would.
        canvas->CaptureRegionView(llx, lly, urx, ury);
        QWidget* target = request.target == CaptureTarget::kWindow
                              ? static_cast<QWidget*>(window)
                              : static_cast<QWidget*>(window->TimingPanel());
        if (target != nullptr) {
          QCoreApplication::processEvents();
          captured = target->grab().save(path, "PNG");
        }
      }
      canvas->SetMovableDotMode(previous_dot_mode);
      if (fade != nullptr && fade->isChecked() != previous_fade) {
        fade->setChecked(previous_fade);
      }
      if (captured && request.target != CaptureTarget::kCanvas &&
          window->TimingPanel() != nullptr) {
        // What the pane said, beside what it looked like.
        const QString text_path = directory_ + "/" + request.stem + ".inspector.txt";
        QFile file(text_path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
          file.write(window->TimingPanel()->InspectorPlainText().toUtf8());
        }
      }
      if (captured) {
        LOG(info) << "Captured GUI snapshot " << path.toStdString() << "\n";
      } else {
        LOG(error) << "Failed to capture GUI snapshot " << path.toStdString()
                   << "\n";
      }
    }
  }

 private:
  enum class CaptureTarget { kCanvas, kWindow, kTimingPane };
  struct Request {
    QString id;
    QString stem;
    QRectF region;
    bool draw_cells = false;
    CaptureTarget target = CaptureTarget::kCanvas;
    std::string select_line;
    bool select_worst = false;
    int select_constraint_id = -1;
    bool fade_unrelated = false;
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

  // No event processing between adopting the snapshot and starting the
  // transition. SetSnapshot schedules a repaint of the target, so pumping
  // events here would paint the target, and the transition's first frame would
  // then snap back to the previous positions before playing forward -- a
  // visible flash on every snapshot, worst where cells move furthest. The
  // transition paints its own first frame at progress zero; when it is disabled
  // it returns immediately and the pump below paints the target as before.
  window_->AnimateToSnapshot();
  QApplication::processEvents();

  SnapshotCaptureRequests::FromEnvironment().CaptureIfRequested(
      window_.get(), metadata.id);

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
  if (std::getenv("DALI_GUI_CAPTURE") != nullptr) {
    window_->close();
    QApplication::processEvents();
    return;
  }
  window_->WaitUntilClosedOrIdle();
}

}  // namespace dali
