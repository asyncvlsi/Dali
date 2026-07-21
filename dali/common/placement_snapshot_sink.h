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
#include <vector>

namespace dali {

class Circuit;

/** N/P-well layer carried by a placement snapshot. */
enum class PlacementWellLayer {
  kPwell,
  kNwell,
  kPplus,
  kNplus,
};

/** One well or implant rectangle in micron coordinates for visualization. */
struct PlacementWellRect {
  float lx = 0;
  float ly = 0;
  float ux = 0;
  float uy = 0;
  PlacementWellLayer layer = PlacementWellLayer::kPwell;
};

/** One placement stage expected to produce snapshots this run. */
struct PlacementSnapshotStage {
  std::string group;  // snapshot group id, e.g. "global_placement"
  std::string title;  // human-readable chart title, e.g. "Global placement"
};

/** Run-level metadata shared by placement snapshot consumers. */
struct PlacementSnapshotRunMetadata {
  std::filesystem::path output_dir;
  std::string design_name;
  int database_microns = 0;
  std::string git_commit;
  bool pause_at_every_snapshot = true;
  /** Stages that will run this configuration, in execution order. Consumers
   *  (e.g. the live GUI) can reserve a chart slot per stage up front. */
  std::vector<PlacementSnapshotStage> stages;
};

/** Checkpoint metadata for one placement state. */
struct PlacementSnapshotMetadata {
  std::string id;
  std::string label;
  std::string group;
  std::string subgroup;
  int iteration = -1;
  std::vector<PlacementWellRect> well_rects;
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
  /** Process pending UI/backend events without publishing a new snapshot. */
  virtual void FlushEvents() {}
  virtual void FinishRun() = 0;
};

}  // namespace dali

#endif  // DALI_COMMON_PLACEMENT_SNAPSHOT_SINK_H_
