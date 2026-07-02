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
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "dali/circuit/enums.h"
#include "dali/common/logging.h"

namespace dali {
namespace {

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
  if (!enabled_) {
    return;
  }

  std::filesystem::create_directories(output_dir_ / "snapshots");
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

    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double max_y = std::numeric_limits<double>::lowest();
    for (NetPin& pin : net.ComponentPins()) {
      min_x = std::min(min_x, pin.AbsX());
      min_y = std::min(min_y, pin.AbsY());
      max_x = std::max(max_x, pin.AbsX());
      max_y = std::max(max_y, pin.AbsY());
    }

    summaries.push_back({&net,
                         net.WeightedHPWLX() * circuit->GridValueX() +
                             net.WeightedHPWLY() * circuit->GridValueY(),
                         ToMicronX(circuit, min_x), ToMicronY(circuit, min_y),
                         ToMicronX(circuit, max_x), ToMicronY(circuit, max_y)});
  }
  return summaries;
}

std::vector<PlacementSnapshotWriter::NetSummary>
PlacementSnapshotWriter::SelectTopNetSummaries(
    std::vector<NetSummary> summaries) const {
  constexpr double kMinDrawableSpan = 1e-9;
  summaries.erase(
      std::remove_if(summaries.begin(), summaries.end(),
                     [](const NetSummary& summary) {
                       return summary.net->PinCnt() > kMaxTopNetPinCount ||
                              ((summary.ux - summary.lx) <= kMinDrawableSpan &&
                               (summary.uy - summary.ly) <= kMinDrawableSpan);
                     }),
      summaries.end());
  std::sort(summaries.begin(), summaries.end(),
            [](const NetSummary& lhs, const NetSummary& rhs) {
              return lhs.weighted_hpwl > rhs.weighted_hpwl;
            });

  size_t keep_count = static_cast<size_t>(
      std::ceil(summaries.size() * kStoredNetPercent / 100.0));
  keep_count = std::min(keep_count, summaries.size());
  summaries.resize(keep_count);
  return summaries;
}

std::vector<PlacementSnapshotWriter::NetSummary>
PlacementSnapshotWriter::SelectBottomNetSummaries(
    std::vector<NetSummary> summaries) const {
  constexpr double kMinDrawableSpan = 1e-9;
  summaries.erase(
      std::remove_if(summaries.begin(), summaries.end(),
                     [](const NetSummary& summary) {
                       return summary.net->PinCnt() > kMaxTopNetPinCount ||
                              ((summary.ux - summary.lx) <= kMinDrawableSpan &&
                               (summary.uy - summary.ly) <= kMinDrawableSpan);
                     }),
      summaries.end());
  std::sort(summaries.begin(), summaries.end(),
            [](const NetSummary& lhs, const NetSummary& rhs) {
              return lhs.weighted_hpwl < rhs.weighted_hpwl;
            });

  size_t keep_count = static_cast<size_t>(
      std::ceil(summaries.size() * kStoredNetPercent / 100.0));
  keep_count = std::min(keep_count, summaries.size());
  summaries.resize(keep_count);
  return summaries;
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
  std::vector<NetSummary> top_net_summaries =
      SelectTopNetSummaries(net_summaries);
  std::vector<NetSummary> bottom_net_summaries =
      SelectBottomNetSummaries(net_summaries);

  WriteMetadata(circuit, record, snapshot_dir);
  WriteComponents(circuit, snapshot_dir);
  WriteNets(circuit, net_summaries, snapshot_dir);
  WriteTopNetPins(circuit, top_net_summaries, snapshot_dir);
  WriteBottomNetPins(circuit, bottom_net_summaries, snapshot_dir);

  records_.push_back(record);
  WriteManifest();
}

void PlacementSnapshotWriter::WriteMetadata(
    Circuit* circuit, const PlacementSnapshotRecord& record,
    const std::filesystem::path& snapshot_dir) const {
  std::ofstream out(snapshot_dir / "metadata.json");
  out << std::setprecision(12);
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
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
  out << "  \"component_count\": " << circuit->Components().size() << ",\n";
  out << "  \"net_count\": " << circuit->Nets().size() << ",\n";
  out << "  \"top_net_policy\": {\"percent\": " << kStoredNetPercent
      << ", \"max_pin_count\": " << kMaxTopNetPinCount << "},\n";
  out << "  \"available_payloads\": {\n";
  out << "    \"components\": \"components.json\",\n";
  out << "    \"nets\": \"nets.json\",\n";
  out << "    \"top_net_pins\": \"top_net_pins.json\",\n";
  out << "    \"bottom_net_pins\": \"bottom_net_pins.json\"\n";
  out << "  }\n";
  out << "}\n";
}

void PlacementSnapshotWriter::WriteComponents(
    Circuit* circuit, const std::filesystem::path& snapshot_dir) const {
  std::ofstream out(snapshot_dir / "components.json");
  out << std::setprecision(12);
  out << "[\n";
  bool first = true;
  for (Component& component : circuit->Components()) {
    if (component.MacroPtr() == circuit->tech().IoDummyMacroPtr()) {
      continue;
    }
    if (!first) {
      out << ",\n";
    }
    first = false;
    out << "  {\"id\": " << component.Id() << ", \"name\": ";
    WriteJsonString(out, component.Name());
    out << ", \"macro\": ";
    WriteJsonString(out, component.MacroPtr()->Name());
    out << ", \"x\": " << ToMicronX(circuit, component.LLX())
        << ", \"y\": " << ToMicronY(circuit, component.LLY())
        << ", \"w\": " << component.Width() * circuit->GridValueX()
        << ", \"h\": " << component.Height() * circuit->GridValueY()
        << ", \"orient\": ";
    WriteJsonString(out, OrientStr(component.Orient()));
    out << ", \"status\": ";
    WriteJsonString(out, component.StatusStr());
    out << "}";
  }
  out << "\n]\n";
}

void PlacementSnapshotWriter::WriteNets(
    Circuit* /*circuit*/, const std::vector<NetSummary>& summaries,
    const std::filesystem::path& snapshot_dir) const {
  std::ofstream out(snapshot_dir / "nets.json");
  out << std::setprecision(12);
  out << "[\n";
  for (size_t i = 0; i < summaries.size(); ++i) {
    const NetSummary& summary = summaries[i];
    if (i > 0) {
      out << ",\n";
    }
    out << "  {\"id\": " << summary.net->Id() << ", \"name\": ";
    WriteJsonString(out, summary.net->Name());
    out << ", \"weighted_hpwl\": " << summary.weighted_hpwl
        << ", \"pin_count\": " << summary.net->PinCnt()
        << ", \"bbox\": {\"lx\": " << summary.lx << ", \"ly\": " << summary.ly
        << ", \"ux\": " << summary.ux << ", \"uy\": " << summary.uy << "}}";
  }
  out << "\n]\n";
}

void PlacementSnapshotWriter::WriteTopNetPins(
    Circuit* circuit, const std::vector<NetSummary>& summaries,
    const std::filesystem::path& snapshot_dir) const {
  std::ofstream out(snapshot_dir / "top_net_pins.json");
  out << std::setprecision(12);
  out << "[\n";
  for (size_t i = 0; i < summaries.size(); ++i) {
    const NetSummary& summary = summaries[i];
    if (i > 0) {
      out << ",\n";
    }
    out << "  {\"id\": " << summary.net->Id() << ", \"name\": ";
    WriteJsonString(out, summary.net->Name());
    out << ", \"weighted_hpwl\": " << summary.weighted_hpwl
        << ", \"pin_count\": " << summary.net->PinCnt()
        << ", \"bbox\": {\"lx\": " << summary.lx << ", \"ly\": " << summary.ly
        << ", \"ux\": " << summary.ux << ", \"uy\": " << summary.uy
        << "}, \"pins\": [";
    for (size_t pin_id = 0; pin_id < summary.net->ComponentPins().size();
         ++pin_id) {
      NetPin& pin = summary.net->ComponentPins()[pin_id];
      if (pin_id > 0) {
        out << ", ";
      }
      out << "{\"component_id\": " << pin.ComponentId() << ", \"component\": ";
      WriteJsonString(out, pin.ComponentName());
      out << ", \"pin\": ";
      WriteJsonString(out, pin.PinName());
      out << ", \"x\": " << ToMicronX(circuit, pin.AbsX())
          << ", \"y\": " << ToMicronY(circuit, pin.AbsY()) << "}";
    }
    out << "]}";
  }
  out << "\n]\n";
}

void PlacementSnapshotWriter::WriteBottomNetPins(
    Circuit* circuit, const std::vector<NetSummary>& summaries,
    const std::filesystem::path& snapshot_dir) const {
  std::ofstream out(snapshot_dir / "bottom_net_pins.json");
  out << std::setprecision(12);
  out << "[\n";
  for (size_t i = 0; i < summaries.size(); ++i) {
    const NetSummary& summary = summaries[i];
    if (i > 0) {
      out << ",\n";
    }
    out << "  {\"id\": " << summary.net->Id() << ", \"name\": ";
    WriteJsonString(out, summary.net->Name());
    out << ", \"weighted_hpwl\": " << summary.weighted_hpwl
        << ", \"pin_count\": " << summary.net->PinCnt()
        << ", \"bbox\": {\"lx\": " << summary.lx << ", \"ly\": " << summary.ly
        << ", \"ux\": " << summary.ux << ", \"uy\": " << summary.uy
        << "}, \"pins\": [";
    for (size_t pin_id = 0; pin_id < summary.net->ComponentPins().size();
         ++pin_id) {
      NetPin& pin = summary.net->ComponentPins()[pin_id];
      if (pin_id > 0) {
        out << ", ";
      }
      out << "{\"component_id\": " << pin.ComponentId() << ", \"component\": ";
      WriteJsonString(out, pin.ComponentName());
      out << ", \"pin\": ";
      WriteJsonString(out, pin.PinName());
      out << ", \"x\": " << ToMicronX(circuit, pin.AbsX())
          << ", \"y\": " << ToMicronY(circuit, pin.AbsY()) << "}";
    }
    out << "]}";
  }
  out << "\n]\n";
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
  out << "  \"schema_version\": 1,\n";
  out << "  \"design_name\": ";
  WriteJsonString(out, design_name_);
  out << ",\n  \"database_units_per_micron\": " << database_microns_ << ",\n";
  out << "  \"created_by\": \"Dali\",\n";
  out << "  \"git_commit\": ";
  WriteJsonString(out, git_commit_);
  out << ",\n  \"snapshot_count\": " << records_.size() << ",\n";
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
