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

#include <fstream>
#include <iomanip>
#include <string>
#include <utility>
#include <vector>

#include "dali/common/git_version.h"
#include "dali/common/logging.h"

namespace dali {
namespace {

std::vector<std::pair<std::string, double>>& Metrics() {
  static std::vector<std::pair<std::string, double>> metrics;
  return metrics;
}

std::string JsonEscape(const std::string& text) {
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

}  // namespace

void ClearPlacementMetrics() { Metrics().clear(); }

void RecordPlacementMetric(const std::string& name, double value) {
  for (auto& [metric_name, metric_value] : Metrics()) {
    if (metric_name == name) {
      metric_value = value;
      return;
    }
  }
  Metrics().emplace_back(name, value);
}

bool WritePlacementMetricsJson(const std::string& file_name, bool completed) {
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
  const auto& metrics = Metrics();
  for (size_t i = 0; i < metrics.size(); ++i) {
    const auto& [name, value] = metrics[i];
    ost << "    \"" << JsonEscape(name) << "\": " << value;
    if (i + 1 < metrics.size()) {
      ost << ",";
    }
    ost << "\n";
  }
  ost << "  }\n";
  ost << "}\n";
  return true;
}

}  // namespace dali
