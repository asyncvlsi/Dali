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
#ifndef DALI_GUI_QT_PLACEMENT_SNAPSHOT_SINK_H_
#define DALI_GUI_QT_PLACEMENT_SNAPSHOT_SINK_H_

#include <memory>
#include <vector>

#include "dali/common/placement_snapshot_sink.h"

class QApplication;
class QLabel;
class QPushButton;
class QCheckBox;
class QWidget;

namespace dali {

class QtPlacementWindow;

/**
 * Minimal live Qt checkpoint viewer.
 *
 * This sink is compiled only when Dali is configured with Qt support. It keeps
 * one placement snapshot in memory, updates the window at each checkpoint, and
 * can pause Dali execution so the user can inspect the intermediate state.
 */
class QtPlacementSnapshotSink : public PlacementSnapshotSink {
 public:
  QtPlacementSnapshotSink();
  ~QtPlacementSnapshotSink() override;

  void StartRun(const PlacementSnapshotRunMetadata& metadata) override;
  bool IsEnabled() const override { return enabled_; }
  void PublishSnapshot(Circuit* circuit,
                       const PlacementSnapshotMetadata& metadata) override;
  void FlushEvents() override;
  void FinishRun() override;

 private:
  std::unique_ptr<QApplication> owned_application_;
  std::unique_ptr<QtPlacementWindow> window_;
  bool enabled_ = false;
};

}  // namespace dali

#endif  // DALI_GUI_QT_PLACEMENT_SNAPSHOT_SINK_H_
