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
#include "dali/common/placement_snapshot_writer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>

#include "dali/circuit/enums.h"
#include "dali/common/logging.h"

namespace dali {
namespace {

constexpr uint32_t kBinarySchemaVersion = 1;
constexpr uint32_t kInvalidIndex = std::numeric_limits<uint32_t>::max();
constexpr size_t kDrawableNetMaxPinCount = 3;

struct BinaryHeader {
  char magic[8];
  uint32_t schema_version;
  uint32_t record_size;
  uint64_t record_count;
};

struct SharedComponentRecord {
  uint32_t id;
  float width_um;
  float height_um;
  uint8_t initial_status;
  uint8_t reserved[3];
};

struct SharedNetRecord {
  uint32_t id;
  uint32_t first_pin;
  uint32_t pin_count;
  uint32_t drawable_pin_count;
};

struct SharedPinRecord {
  uint32_t component_id;
  float offset_um[16];
};

struct ComponentGridRecord {
  uint32_t id;
  int32_t x_grid;
  int32_t y_grid;
  uint8_t orient;
  uint8_t status;
  uint8_t reserved[2];
};

struct ComponentContinuousRecord {
  uint32_t id;
  float x_um;
  float y_um;
  uint8_t orient;
  uint8_t status;
  uint8_t reserved[2];
};

struct NetMetricRecord {
  uint32_t id;
  float weighted_hpwl_um;
};

static_assert(sizeof(BinaryHeader) == 24);
static_assert(sizeof(SharedComponentRecord) == 16);
static_assert(sizeof(SharedNetRecord) == 16);
static_assert(sizeof(SharedPinRecord) == 68);
static_assert(sizeof(ComponentGridRecord) == 16);
static_assert(sizeof(ComponentContinuousRecord) == 16);
static_assert(sizeof(NetMetricRecord) == 8);

std::string JsonEscape(const std::string& text) {
  std::ostringstream escaped;
  for (char ch : text) {
    switch (ch) {
      case '"':
        escaped << "\\\"";
        break;
      case '\\':
        escaped << "\\\\";
        break;
      case '\b':
        escaped << "\\b";
        break;
      case '\f':
        escaped << "\\f";
        break;
      case '\n':
        escaped << "\\n";
        break;
      case '\r':
        escaped << "\\r";
        break;
      case '\t':
        escaped << "\\t";
        break;
      default:
        escaped << ch;
        break;
    }
  }
  return escaped.str();
}

void WriteJsonString(std::ostream& out, const std::string& text) {
  out << '"' << JsonEscape(text) << '"';
}

double ToMicronX(Circuit* circuit, double x) {
  return x * circuit->GridValueX();
}

double ToMicronY(Circuit* circuit, double y) {
  return y * circuit->GridValueY();
}

bool NetHasComponentPins(Net& net) { return !net.ComponentPins().empty(); }

bool IsVisualizationComponent(Circuit* circuit, Component& component) {
  return component.MacroPtr() != circuit->tech().IoDummyMacroPtr();
}

bool IsDrawableNet(Net& net) {
  return NetHasComponentPins(net) &&
         net.ComponentPins().size() <= kDrawableNetMaxPinCount;
}

template <typename Record>
void WriteBinaryTable(const std::filesystem::path& path, const char (&magic)[8],
                      const std::vector<Record>& records) {
  std::ofstream out(path, std::ios::binary);
  BinaryHeader header{};
  std::copy(std::begin(magic), std::end(magic), std::begin(header.magic));
  header.schema_version = kBinarySchemaVersion;
  header.record_size = sizeof(Record);
  header.record_count = records.size();
  out.write(reinterpret_cast<const char*>(&header), sizeof(header));
  if (!records.empty()) {
    out.write(reinterpret_cast<const char*>(records.data()),
              static_cast<std::streamsize>(records.size() * sizeof(Record)));
  }
}

}  // namespace

void PlacementSnapshotWriter::StartRun(const std::filesystem::path& output_dir,
                                       const std::string& design_name,
                                       int database_microns,
                                       const std::string& git_commit) {
  output_dir_ = output_dir;
  design_name_ = design_name;
  database_microns_ = database_microns;
  git_commit_ = git_commit;
  records_.clear();
  enabled_ = !output_dir_.empty();
  shared_design_written_ = false;
  if (!enabled_) {
    return;
  }

  std::filesystem::create_directories(output_dir_ / "snapshots");
  std::filesystem::create_directories(output_dir_ / "shared");
}

void PlacementSnapshotWriter::StartRun(
    const PlacementSnapshotRunMetadata& metadata) {
  StartRun(metadata.output_dir, metadata.design_name, metadata.database_microns,
           metadata.git_commit);
}

std::filesystem::path PlacementSnapshotWriter::SnapshotPath(int index) const {
  return output_dir_ / "snapshots" / std::to_string(index);
}

std::vector<PlacementSnapshotWriter::NetSummary>
PlacementSnapshotWriter::BuildNetSummaries(Circuit* circuit) const {
  std::vector<NetSummary> summaries;
  summaries.reserve(circuit->Nets().size());
  for (Net& net : circuit->Nets()) {
    if (!NetHasComponentPins(net)) {
      continue;
    }

    summaries.push_back(
        {&net, net.WeightedHPWLX() * circuit->GridValueX() +
                   net.WeightedHPWLY() * circuit->GridValueY()});
  }
  return summaries;
}

bool PlacementSnapshotWriter::UseGridCoordinates(Circuit* circuit) const {
  constexpr double kGridTolerance = 1e-6;
  for (Component& component : circuit->Components()) {
    if (!IsVisualizationComponent(circuit, component)) {
      continue;
    }
    if (std::abs(component.LLX() - std::round(component.LLX())) >
            kGridTolerance ||
        std::abs(component.LLY() - std::round(component.LLY())) >
            kGridTolerance) {
      return false;
    }
  }
  return true;
}

void PlacementSnapshotWriter::WriteSnapshot(
    Circuit* circuit, const std::string& id, const std::string& label,
    const std::string& group, const std::string& subgroup, int iteration) {
  if (!enabled_) {
    return;
  }

  int index = static_cast<int>(records_.size());
  std::filesystem::path snapshot_dir = SnapshotPath(index);
  std::filesystem::create_directories(snapshot_dir);
  WriteSharedDesign(circuit);

  PlacementSnapshotRecord record;
  record.index = index;
  record.id = id;
  record.label = label;
  record.group = group;
  record.subgroup = subgroup;
  record.iteration = iteration;
  record.weighted_hpwl = circuit->WeightedHPWL();
  record.path = "snapshots/" + std::to_string(index);

  std::vector<NetSummary> net_summaries = BuildNetSummaries(circuit);
  bool use_grid_coordinates = UseGridCoordinates(circuit);

  WriteMetadata(circuit, record, snapshot_dir, use_grid_coordinates);
  WriteComponentLocations(circuit, snapshot_dir, use_grid_coordinates);
  WriteNetMetrics(net_summaries, snapshot_dir);

  records_.push_back(record);
  WriteManifest();
}

void PlacementSnapshotWriter::PublishSnapshot(
    Circuit* circuit, const PlacementSnapshotMetadata& metadata) {
  WriteSnapshot(circuit, metadata.id, metadata.label, metadata.group,
                metadata.subgroup, metadata.iteration);
}

void PlacementSnapshotWriter::WriteSharedDesign(Circuit* circuit) {
  if (shared_design_written_) return;
  WriteSharedComponents(circuit);
  WriteSharedNets(circuit);
  shared_design_written_ = true;
}

void PlacementSnapshotWriter::WriteMetadata(
    Circuit* circuit, const PlacementSnapshotRecord& record,
    const std::filesystem::path& snapshot_dir,
    bool use_grid_coordinates) const {
  std::ofstream out(snapshot_dir / "metadata.json");
  out << std::setprecision(12);
  out << "{\n";
  out << "  \"schema_version\": 2,\n";
  out << "  \"encoding\": \"binary-le\",\n";
  out << "  \"index\": " << record.index << ",\n";
  out << "  \"id\": ";
  WriteJsonString(out, record.id);
  out << ",\n  \"label\": ";
  WriteJsonString(out, record.label);
  out << ",\n  \"group\": ";
  WriteJsonString(out, record.group);
  out << ",\n  \"subgroup\": ";
  WriteJsonString(out, record.subgroup);
  out << ",\n  \"iteration\": " << record.iteration << ",\n";
  out << "  \"weighted_hpwl\": " << record.weighted_hpwl << ",\n";
  out << "  \"die_area\": {\"lx\": " << ToMicronX(circuit, circuit->RegionLLX())
      << ", \"ly\": " << ToMicronY(circuit, circuit->RegionLLY())
      << ", \"ux\": " << ToMicronX(circuit, circuit->RegionURX())
      << ", \"uy\": " << ToMicronY(circuit, circuit->RegionURY()) << "},\n";
  out << "  \"grid\": {\"x\": " << circuit->GridValueX()
      << ", \"y\": " << circuit->GridValueY() << "},\n";
  out << "  \"component_count\": " << circuit->Components().size() << ",\n";
  out << "  \"net_count\": " << circuit->Nets().size() << ",\n";
  out << "  \"net_draw_policy\": {\"max_pin_count\": 3},\n";
  out << "  \"coordinate_type\": ";
  WriteJsonString(out, use_grid_coordinates ? "grid_int" : "continuous_um");
  out << ",\n";
  out << "  \"available_payloads\": {\n";
  out << "    \"components\": \"components.bin\",\n";
  out << "    \"net_metrics\": \"net_metrics.bin\"\n";
  out << "  }\n";
  out << "}\n";
}

void PlacementSnapshotWriter::WriteSharedComponents(Circuit* circuit) const {
  std::vector<SharedComponentRecord> records;
  records.reserve(circuit->Components().size());
  for (Component& component : circuit->Components()) {
    if (!IsVisualizationComponent(circuit, component)) {
      continue;
    }
    records.push_back(
        {static_cast<uint32_t>(component.Id()),
         static_cast<float>(component.Width() * circuit->GridValueX()),
         static_cast<float>(component.Height() * circuit->GridValueY()),
         static_cast<uint8_t>(component.Status()),
         {0, 0, 0}});
  }
  WriteBinaryTable(output_dir_ / "shared" / "components.bin", "DALICMP",
                   records);
}

void PlacementSnapshotWriter::WriteSharedNets(Circuit* circuit) const {
  constexpr ComponentOrient kOrientations[] = {N, S, W, E, FN, FS, FW, FE};
  std::vector<SharedNetRecord> net_records;
  std::vector<SharedPinRecord> pin_records;
  net_records.reserve(circuit->Nets().size());
  for (Net& net : circuit->Nets()) {
    if (!NetHasComponentPins(net)) {
      continue;
    }
    uint32_t first_pin = kInvalidIndex;
    uint32_t drawable_pin_count = 0;
    if (IsDrawableNet(net)) {
      first_pin = static_cast<uint32_t>(pin_records.size());
      drawable_pin_count = static_cast<uint32_t>(net.ComponentPins().size());
      for (NetPin& pin : net.ComponentPins()) {
        SharedPinRecord pin_record{};
        pin_record.component_id = static_cast<uint32_t>(pin.ComponentId());
        for (size_t orient_id = 0; orient_id < std::size(kOrientations);
             ++orient_id) {
          ComponentOrient orient = kOrientations[orient_id];
          pin_record.offset_um[2 * orient_id] = static_cast<float>(
              ToMicronX(circuit, pin.PinPtr()->OffsetX(orient)));
          pin_record.offset_um[2 * orient_id + 1] = static_cast<float>(
              ToMicronY(circuit, pin.PinPtr()->OffsetY(orient)));
        }
        pin_records.push_back(pin_record);
      }
    }
    net_records.push_back({static_cast<uint32_t>(net.Id()), first_pin,
                           static_cast<uint32_t>(net.ComponentPins().size()),
                           drawable_pin_count});
  }
  WriteBinaryTable(output_dir_ / "shared" / "nets.bin", "DALINET", net_records);
  WriteBinaryTable(output_dir_ / "shared" / "pins.bin", "DALIPIN", pin_records);
}

void PlacementSnapshotWriter::WriteComponentLocations(
    Circuit* circuit, const std::filesystem::path& snapshot_dir,
    bool use_grid_coordinates) const {
  if (use_grid_coordinates) {
    std::vector<ComponentGridRecord> records;
    records.reserve(circuit->Components().size());
    for (Component& component : circuit->Components()) {
      if (!IsVisualizationComponent(circuit, component)) {
        continue;
      }
      records.push_back({static_cast<uint32_t>(component.Id()),
                         static_cast<int32_t>(std::llround(component.LLX())),
                         static_cast<int32_t>(std::llround(component.LLY())),
                         static_cast<uint8_t>(component.Orient()),
                         static_cast<uint8_t>(component.Status()),
                         {0, 0}});
    }
    WriteBinaryTable(snapshot_dir / "components.bin", "DALICLG", records);
    return;
  }

  std::vector<ComponentContinuousRecord> records;
  records.reserve(circuit->Components().size());
  for (Component& component : circuit->Components()) {
    if (!IsVisualizationComponent(circuit, component)) {
      continue;
    }
    records.push_back({static_cast<uint32_t>(component.Id()),
                       static_cast<float>(ToMicronX(circuit, component.LLX())),
                       static_cast<float>(ToMicronY(circuit, component.LLY())),
                       static_cast<uint8_t>(component.Orient()),
                       static_cast<uint8_t>(component.Status()),
                       {0, 0}});
  }
  WriteBinaryTable(snapshot_dir / "components.bin", "DALICLC", records);
}

void PlacementSnapshotWriter::WriteNetMetrics(
    const std::vector<NetSummary>& summaries,
    const std::filesystem::path& snapshot_dir) const {
  std::vector<NetMetricRecord> records;
  records.reserve(summaries.size());
  for (const NetSummary& summary : summaries) {
    records.push_back({static_cast<uint32_t>(summary.net->Id()),
                       static_cast<float>(summary.weighted_hpwl)});
  }
  WriteBinaryTable(snapshot_dir / "net_metrics.bin", "DALINMT", records);
}

void PlacementSnapshotWriter::FinishRun() {
  if (!enabled_) {
    return;
  }
  WriteManifest();
}

void PlacementSnapshotWriter::WriteManifest() const {
  std::ofstream out(output_dir_ / "manifest.json");
  out << std::setprecision(12);
  out << "{\n";
  out << "  \"schema_version\": 2,\n";
  out << "  \"encoding\": \"binary-le\",\n";
  out << "  \"design_name\": ";
  WriteJsonString(out, design_name_);
  out << ",\n  \"database_units_per_micron\": " << database_microns_ << ",\n";
  out << "  \"created_by\": \"Dali\",\n";
  out << "  \"git_commit\": ";
  WriteJsonString(out, git_commit_);
  out << ",\n  \"shared_payloads\": {\n";
  out << "    \"components\": \"shared/components.bin\",\n";
  out << "    \"nets\": \"shared/nets.bin\",\n";
  out << "    \"pins\": \"shared/pins.bin\"\n";
  out << "  },\n";
  out << "  \"snapshot_count\": " << records_.size() << ",\n";
  out << "  \"snapshots\": [\n";
  for (size_t i = 0; i < records_.size(); ++i) {
    const PlacementSnapshotRecord& record = records_[i];
    if (i > 0) {
      out << ",\n";
    }
    out << "    {\"index\": " << record.index << ", \"path\": ";
    WriteJsonString(out, record.path);
    out << ", \"id\": ";
    WriteJsonString(out, record.id);
    out << ", \"label\": ";
    WriteJsonString(out, record.label);
    out << ", \"group\": ";
    WriteJsonString(out, record.group);
    out << ", \"subgroup\": ";
    WriteJsonString(out, record.subgroup);
    out << ", \"iteration\": " << record.iteration
        << ", \"weighted_hpwl\": " << record.weighted_hpwl << "}";
  }
  out << "\n  ]\n";
  out << "}\n";
}

}  // namespace dali
