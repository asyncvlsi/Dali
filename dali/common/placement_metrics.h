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

class Circuit;

/** Weighted HPWL split by axis and net fanout, in physical micron units. */
struct WeightedHpwlBreakdown {
  double x = 0.0;
  double y = 0.0;
  double fanout_2 = 0.0;
  double fanout_3 = 0.0;
  double fanout_4_to_19 = 0.0;
  double fanout_20_to_39 = 0.0;
  double fanout_40_to_79 = 0.0;
  double fanout_80_to_159 = 0.0;
  double fanout_160_plus = 0.0;

  /** Return total weighted HPWL across both axes. */
  double Total() const { return x + y; }
};

/** Collects named placement metrics and writes them in Dali's JSON format. */
class PlacementMetrics {
 public:
  void Clear();
  void Record(const std::string& name, double value);
  bool WriteJson(const std::string& file_name, bool completed) const;

 private:
  std::vector<std::pair<std::string, double>> metrics_;
};

/** Temporarily suppress metrics recorded through the process-wide wrappers. */
class ScopedPlacementMetricSuppression {
 public:
  ScopedPlacementMetricSuppression();
  ~ScopedPlacementMetricSuppression();

  ScopedPlacementMetricSuppression(const ScopedPlacementMetricSuppression&) =
      delete;
  ScopedPlacementMetricSuppression& operator=(
      const ScopedPlacementMetricSuppression&) = delete;
};

/** Clear all placement metrics recorded for the current process. */
void ClearPlacementMetrics();

/** Record or update one named placement metric. */
void RecordPlacementMetric(const std::string& name, double value);

/** Compute weighted HPWL attribution for the circuit's current placement. */
WeightedHpwlBreakdown ComputeWeightedHpwlBreakdown(Circuit& circuit);

/** Record total, axis, and fanout HPWL metrics under one stage name. */
void RecordPlacementHpwlMetrics(const std::string& name, Circuit& circuit);

/** Write collected placement metrics as JSON. */
bool WritePlacementMetricsJson(const std::string& file_name, bool completed);

}  // namespace dali

#endif  // DALI_COMMON_PLACEMENT_METRICS_H_
