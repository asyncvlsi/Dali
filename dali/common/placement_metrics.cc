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
#include "dali/common/placement_metrics.h"

#include <atomic>
#include <fstream>
#include <iomanip>
#include <string>

#include "dali/circuit/circuit.h"
#include "dali/common/git_version.h"
#include "dali/common/logging.h"

namespace dali {

static PlacementMetrics& GlobalPlacementMetrics() {
  static PlacementMetrics metrics;
  return metrics;
}

static std::atomic<int>& PlacementMetricSuppressionDepth() {
  static std::atomic<int> suppression_depth = 0;
  return suppression_depth;
}

static std::string JsonEscape(const std::string& text) {
  std::string escaped;
  escaped.reserve(text.size());
  for (char ch : text) {
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += ch;
        break;
    }
  }
  return escaped;
}

void PlacementMetrics::Clear() { metrics_.clear(); }

void PlacementMetrics::Record(const std::string& name, double value) {
  for (auto& [metric_name, metric_value] : metrics_) {
    if (metric_name == name) {
      metric_value = value;
      return;
    }
  }
  metrics_.emplace_back(name, value);
}

ScopedPlacementMetricSuppression::ScopedPlacementMetricSuppression() {
  PlacementMetricSuppressionDepth().fetch_add(1, std::memory_order_relaxed);
}

ScopedPlacementMetricSuppression::~ScopedPlacementMetricSuppression() {
  PlacementMetricSuppressionDepth().fetch_sub(1, std::memory_order_relaxed);
}

bool PlacementMetrics::WriteJson(const std::string& file_name,
                                 bool completed) const {
  std::ofstream ost(file_name);
  if (!ost.is_open()) {
    LOG(error) << "Cannot open placement metrics file: " << file_name << "\n";
    return false;
  }

  ost << std::setprecision(12);
  ost << "{\n";
  ost << "  \"completed\": " << (completed ? "true" : "false") << ",\n";
  ost << "  \"git_commit\": \"" << JsonEscape(get_git_version_short())
      << "\",\n";
  ost << "  \"stages\": {\n";
  for (size_t i = 0; i < metrics_.size(); ++i) {
    const auto& [name, value] = metrics_[i];
    ost << "    \"" << JsonEscape(name) << "\": " << value;
    if (i + 1 < metrics_.size()) {
      ost << ",";
    }
    ost << "\n";
  }
  ost << "  }\n";
  ost << "}\n";
  return true;
}

void ClearPlacementMetrics() { GlobalPlacementMetrics().Clear(); }

void RecordPlacementMetric(const std::string& name, double value) {
  if (PlacementMetricSuppressionDepth().load(std::memory_order_relaxed) > 0) {
    return;
  }
  GlobalPlacementMetrics().Record(name, value);
}

WeightedHpwlBreakdown ComputeWeightedHpwlBreakdown(Circuit& circuit) {
  WeightedHpwlBreakdown result;
  for (Net& net : circuit.Nets()) {
    double net_x = net.WeightedHPWLX() * circuit.GridValueX();
    double net_y = net.WeightedHPWLY() * circuit.GridValueY();
    double net_hpwl = net_x + net_y;
    result.x += net_x;
    result.y += net_y;

    size_t fanout = net.PinCnt();
    if (fanout == 2) {
      result.fanout_2 += net_hpwl;
    } else if (fanout == 3) {
      result.fanout_3 += net_hpwl;
    } else if (fanout <= 19) {
      result.fanout_4_to_19 += net_hpwl;
    } else if (fanout <= 39) {
      result.fanout_20_to_39 += net_hpwl;
    } else if (fanout <= 79) {
      result.fanout_40_to_79 += net_hpwl;
    } else if (fanout <= 159) {
      result.fanout_80_to_159 += net_hpwl;
    } else {
      result.fanout_160_plus += net_hpwl;
    }
  }
  return result;
}

void RecordPlacementHpwlMetrics(const std::string& name, Circuit& circuit) {
  WeightedHpwlBreakdown hpwl = ComputeWeightedHpwlBreakdown(circuit);
  RecordPlacementMetric(name, hpwl.Total());
  RecordPlacementMetric(name + ".x", hpwl.x);
  RecordPlacementMetric(name + ".y", hpwl.y);
  RecordPlacementMetric(name + ".fanout.2", hpwl.fanout_2);
  RecordPlacementMetric(name + ".fanout.3", hpwl.fanout_3);
  RecordPlacementMetric(name + ".fanout.4_19", hpwl.fanout_4_to_19);
  RecordPlacementMetric(name + ".fanout.20_39", hpwl.fanout_20_to_39);
  RecordPlacementMetric(name + ".fanout.40_79", hpwl.fanout_40_to_79);
  RecordPlacementMetric(name + ".fanout.80_159", hpwl.fanout_80_to_159);
  RecordPlacementMetric(name + ".fanout.160_plus", hpwl.fanout_160_plus);
}

bool WritePlacementMetricsJson(const std::string& file_name, bool completed) {
  return GlobalPlacementMetrics().WriteJson(file_name, completed);
}

}  // namespace dali
