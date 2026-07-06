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
#ifndef DALI_COMMON_PLACEMENT_SNAPSHOT_WRITER_H_
#define DALI_COMMON_PLACEMENT_SNAPSHOT_WRITER_H_

#include <filesystem>
#include <string>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/common/placement_snapshot_sink.h"

namespace dali {

/** Metadata describing one emitted visualization snapshot. */
struct PlacementSnapshotRecord {
  int index = 0;
  std::string id;
  std::string label;
  std::string group;
  std::string subgroup;
  int iteration = -1;
  double weighted_hpwl = 0;
  std::string path;
};

/**
 * Writes self-describing placement visualization snapshots.
 *
 * The writer emits a manifest plus numbered snapshot folders:
 *
 *   manifest.json
 *   shared/{components,nets,pins}.bin
 *   snapshots/0/{metadata.json,components.bin,net_metrics.bin}
 *   snapshots/1/{metadata.json,components.bin,net_metrics.bin}
 *
 * Legalized snapshots use integer grid coordinates when all component
 * lower-left locations are grid-aligned. Continuous placement snapshots use
 * float micron coordinates.
 */
class PlacementSnapshotWriter : public PlacementSnapshotSink {
 public:
  void StartRun(const std::filesystem::path& output_dir,
                const std::string& design_name, int database_microns,
                const std::string& git_commit);
  void StartRun(const PlacementSnapshotRunMetadata& metadata) override;

  bool IsEnabled() const override { return enabled_; }

  void WriteSnapshot(Circuit* circuit, const std::string& id,
                     const std::string& label, const std::string& group,
                     const std::string& subgroup = "", int iteration = -1);
  void PublishSnapshot(Circuit* circuit,
                       const PlacementSnapshotMetadata& metadata) override;

  void FinishRun() override;

 private:
  struct NetSummary {
    Net* net = nullptr;
    double weighted_hpwl = 0;
  };

  std::filesystem::path SnapshotPath(int index) const;
  std::vector<NetSummary> BuildNetSummaries(Circuit* circuit) const;

  bool UseGridCoordinates(Circuit* circuit) const;
  void WriteSharedDesign(Circuit* circuit);
  void WriteMetadata(Circuit* circuit, const PlacementSnapshotRecord& record,
                     const std::filesystem::path& snapshot_dir,
                     bool use_grid_coordinates) const;
  void WriteSharedComponents(Circuit* circuit) const;
  void WriteSharedNets(Circuit* circuit) const;
  void WriteComponentLocations(Circuit* circuit,
                               const std::filesystem::path& snapshot_dir,
                               bool use_grid_coordinates) const;
  void WriteNetMetrics(const std::vector<NetSummary>& summaries,
                       const std::filesystem::path& snapshot_dir) const;
  void WriteManifest() const;

  std::filesystem::path output_dir_;
  std::string design_name_;
  int database_microns_ = 0;
  std::string git_commit_;
  std::vector<PlacementSnapshotRecord> records_;
  bool enabled_ = false;
  bool shared_design_written_ = false;
};

}  // namespace dali

#endif  // DALI_COMMON_PLACEMENT_SNAPSHOT_WRITER_H_
