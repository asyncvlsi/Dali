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

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "dali/circuit/circuit.h"

static std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

TEST(PlacementMetricsTest, WritesCompletedJsonWithUpdatedStageValues) {
  const std::filesystem::path metrics_file =
      std::filesystem::temp_directory_path() /
      "dali_placement_metrics_test.json";
  std::filesystem::remove(metrics_file);

  dali::PlacementMetrics metrics;
  metrics.Record("input", 10.0);
  metrics.Record("global_placement", 20.0);
  metrics.Record("global_placement", 21.5);
  metrics.Record("stage\"with\\escapes", 30.0);

  ASSERT_TRUE(metrics.WriteJson(metrics_file.string(), true));

  const std::string json = ReadFile(metrics_file);
  EXPECT_NE(json.find("\"completed\": true"), std::string::npos);
  EXPECT_NE(json.find("\"git_commit\""), std::string::npos);
  EXPECT_NE(json.find("\"input\": 10"), std::string::npos);
  EXPECT_NE(json.find("\"global_placement\": 21.5"), std::string::npos);
  EXPECT_EQ(json.find("\"global_placement\": 20"), std::string::npos);
  EXPECT_NE(json.find("\"stage\\\"with\\\\escapes\": 30"), std::string::npos);

  std::filesystem::remove(metrics_file);
}

TEST(PlacementMetricsTest, GlobalWrapperClearRemovesOldStageValues) {
  const std::filesystem::path metrics_file =
      std::filesystem::temp_directory_path() /
      "dali_placement_metrics_clear_test.json";
  std::filesystem::remove(metrics_file);

  dali::ClearPlacementMetrics();
  dali::RecordPlacementMetric("old", 1.0);
  dali::ClearPlacementMetrics();
  dali::RecordPlacementMetric("new", 2.0);

  ASSERT_TRUE(dali::WritePlacementMetricsJson(metrics_file.string(), false));

  const std::string json = ReadFile(metrics_file);
  EXPECT_NE(json.find("\"completed\": false"), std::string::npos);
  EXPECT_EQ(json.find("\"old\""), std::string::npos);
  EXPECT_NE(json.find("\"new\": 2"), std::string::npos);

  std::filesystem::remove(metrics_file);
}

TEST(PlacementMetricsTest, ScopedSuppressionHidesDiscardedTrialMetrics) {
  const std::filesystem::path metrics_file =
      std::filesystem::temp_directory_path() /
      "dali_placement_metrics_suppression_test.json";
  std::filesystem::remove(metrics_file);

  dali::ClearPlacementMetrics();
  dali::RecordPlacementMetric("before_preview", 1.0);
  {
    dali::ScopedPlacementMetricSuppression suppress_preview_metrics;
    dali::RecordPlacementMetric("discarded_preview", 2.0);
  }
  dali::RecordPlacementMetric("selected_flow", 3.0);

  ASSERT_TRUE(dali::WritePlacementMetricsJson(metrics_file.string(), true));
  const std::string json = ReadFile(metrics_file);
  EXPECT_NE(json.find("\"before_preview\": 1"), std::string::npos);
  EXPECT_EQ(json.find("\"discarded_preview\""), std::string::npos);
  EXPECT_NE(json.find("\"selected_flow\": 3"), std::string::npos);

  std::filesystem::remove(metrics_file);
}

TEST(PlacementMetricsTest, AttributesWeightedHpwlByAxisAndFanout) {
  dali::Circuit circuit;
  circuit.SetManufacturingGrid(0.05);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(0.5, 0.25);
  circuit.ReserveSpaceForDesignImp(3, 0, 2);
  circuit.AddMacro("cell", 1, 1);
  dali::Macro* macro = circuit.GetMacroPtr("cell");
  circuit.AddMacroPin(macro, "p", true)->SetOffset(0, 0);
  circuit.AddComponent("u0", "cell", 0, 0);
  circuit.AddComponent("u1", "cell", 10, 20);
  circuit.AddComponent("u2", "cell", 20, 40);

  circuit.AddNet("two_pin", 2);
  circuit.AddComponentPinToNet("u0", "p", "two_pin");
  circuit.AddComponentPinToNet("u1", "p", "two_pin");
  circuit.AddNet("three_pin", 3);
  circuit.AddComponentPinToNet("u0", "p", "three_pin");
  circuit.AddComponentPinToNet("u1", "p", "three_pin");
  circuit.AddComponentPinToNet("u2", "p", "three_pin");

  dali::WeightedHpwlBreakdown hpwl =
      dali::ComputeWeightedHpwlBreakdown(circuit);

  EXPECT_DOUBLE_EQ(hpwl.x, 15.0);
  EXPECT_DOUBLE_EQ(hpwl.y, 15.0);
  EXPECT_DOUBLE_EQ(hpwl.fanout_2, 10.0);
  EXPECT_DOUBLE_EQ(hpwl.fanout_3, 20.0);
  EXPECT_DOUBLE_EQ(hpwl.Total(), 30.0);
}
