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
#ifndef DALI_COMMON_PLACEMENT_SNAPSHOT_SINK_H_
#define DALI_COMMON_PLACEMENT_SNAPSHOT_SINK_H_

#include <filesystem>
#include <string>

namespace dali {

class Circuit;

/** Run-level metadata shared by placement snapshot consumers. */
struct PlacementSnapshotRunMetadata {
  std::filesystem::path output_dir;
  std::string design_name;
  int database_microns = 0;
  std::string git_commit;
  bool pause_at_every_snapshot = true;
};

/** Checkpoint metadata for one placement state. */
struct PlacementSnapshotMetadata {
  std::string id;
  std::string label;
  std::string group;
  std::string subgroup;
  int iteration = -1;
};

/**
 * Consumer interface for placement checkpoints.
 *
 * Backends may write binary files, stream to a live GUI, pause execution, or do
 * nothing. Placement code should publish checkpoints through this interface
 * instead of depending on a particular visualization implementation.
 */
class PlacementSnapshotSink {
 public:
  virtual ~PlacementSnapshotSink() = default;

  virtual void StartRun(const PlacementSnapshotRunMetadata& metadata) = 0;
  virtual bool IsEnabled() const = 0;
  virtual void PublishSnapshot(Circuit* circuit,
                               const PlacementSnapshotMetadata& metadata) = 0;
  virtual void FinishRun() = 0;
};

}  // namespace dali

#endif  // DALI_COMMON_PLACEMENT_SNAPSHOT_SINK_H_
