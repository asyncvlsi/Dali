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

namespace {

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream input(path);
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

TEST(PlacementMetricsTest, WritesCompletedJsonWithUpdatedStageValues) {
  const std::filesystem::path metrics_file =
      std::filesystem::temp_directory_path() /
      "dali_placement_metrics_test.json";
  std::filesystem::remove(metrics_file);

  dali::ClearPlacementMetrics();
  dali::RecordPlacementMetric("input", 10.0);
  dali::RecordPlacementMetric("global_placement", 20.0);
  dali::RecordPlacementMetric("global_placement", 21.5);
  dali::RecordPlacementMetric("stage\"with\\escapes", 30.0);

  ASSERT_TRUE(dali::WritePlacementMetricsJson(metrics_file.string(), true));

  const std::string json = ReadFile(metrics_file);
  EXPECT_NE(json.find("\"completed\": true"), std::string::npos);
  EXPECT_NE(json.find("\"git_commit\""), std::string::npos);
  EXPECT_NE(json.find("\"input\": 10"), std::string::npos);
  EXPECT_NE(json.find("\"global_placement\": 21.5"), std::string::npos);
  EXPECT_EQ(json.find("\"global_placement\": 20"), std::string::npos);
  EXPECT_NE(json.find("\"stage\\\"with\\\\escapes\": 30"), std::string::npos);

  std::filesystem::remove(metrics_file);
}

TEST(PlacementMetricsTest, ClearRemovesOldStageValues) {
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

}  // namespace
