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
 *   snapshots/0/{metadata,components,nets,top_net_pins,bottom_net_pins}.json
 *   snapshots/1/{metadata,components,nets,top_net_pins,bottom_net_pins}.json
 *
 * Coordinates are written in microns so visualizers do not need to know Dali's
 * internal grid values.
 */
class PlacementSnapshotWriter {
 public:
  void StartRun(const std::filesystem::path& output_dir,
                const std::string& design_name, int database_microns,
                const std::string& git_commit);

  bool IsEnabled() const { return enabled_; }

  void WriteSnapshot(Circuit* circuit, const std::string& id,
                     const std::string& label, const std::string& group,
                     const std::string& subgroup = "", int iteration = -1);

  void FinishRun();

 private:
  struct NetSummary {
    Net* net = nullptr;
    double weighted_hpwl = 0;
    double lx = 0;
    double ly = 0;
    double ux = 0;
    double uy = 0;
  };

  static constexpr int kMaxTopNetPinCount = 3;
  static constexpr double kStoredNetPercent = 100.0;

  std::filesystem::path SnapshotPath(int index) const;
  std::vector<NetSummary> BuildNetSummaries(Circuit* circuit) const;
  std::vector<NetSummary> SelectTopNetSummaries(
      std::vector<NetSummary> summaries) const;
  std::vector<NetSummary> SelectBottomNetSummaries(
      std::vector<NetSummary> summaries) const;

  void WriteMetadata(Circuit* circuit, const PlacementSnapshotRecord& record,
                     const std::filesystem::path& snapshot_dir) const;
  void WriteComponents(Circuit* circuit,
                       const std::filesystem::path& snapshot_dir) const;
  void WriteNets(Circuit* circuit, const std::vector<NetSummary>& summaries,
                 const std::filesystem::path& snapshot_dir) const;
  void WriteTopNetPins(Circuit* circuit,
                       const std::vector<NetSummary>& summaries,
                       const std::filesystem::path& snapshot_dir) const;
  void WriteBottomNetPins(Circuit* circuit,
                          const std::vector<NetSummary>& summaries,
                          const std::filesystem::path& snapshot_dir) const;
  void WriteManifest() const;

  std::filesystem::path output_dir_;
  std::string design_name_;
  int database_microns_ = 0;
  std::string git_commit_;
  std::vector<PlacementSnapshotRecord> records_;
  bool enabled_ = false;
};

}  // namespace dali

#endif  // DALI_COMMON_PLACEMENT_SNAPSHOT_WRITER_H_
