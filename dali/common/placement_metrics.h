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
#ifndef DALI_COMMON_PLACEMENT_METRICS_H_
#define DALI_COMMON_PLACEMENT_METRICS_H_

#include <string>
#include <utility>
#include <vector>

namespace dali {

/** Collects named placement metrics and writes them in Dali's JSON format. */
class PlacementMetrics {
 public:
  void Clear();
  void Record(const std::string& name, double value);
  bool WriteJson(const std::string& file_name, bool completed) const;

 private:
  std::vector<std::pair<std::string, double>> metrics_;
};

/** Clear all placement metrics recorded for the current process. */
void ClearPlacementMetrics();

/** Record or update one named placement metric. */
void RecordPlacementMetric(const std::string& name, double value);

/** Write collected placement metrics as JSON. */
bool WritePlacementMetricsJson(const std::string& file_name, bool completed);

}  // namespace dali

#endif  // DALI_COMMON_PLACEMENT_METRICS_H_
