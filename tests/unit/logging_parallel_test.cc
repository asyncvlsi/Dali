/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "dali/common/logging.h"

namespace {

constexpr int kThreadCount = 8;
constexpr int kMessagesPerThread = 200;

std::string ExpectedRecord(int thread_id, int message_id) {
  std::ostringstream record;
  record << "BEGIN tid=" << thread_id << " msg=" << message_id << " END";
  return record.str();
}

}  // namespace

TEST(LoggingTest, WritesCompleteRecordsFromParallelThreads) {
  const std::filesystem::path log_file =
      std::filesystem::temp_directory_path() / "dali_parallel_logging_test.log";
  std::filesystem::remove(log_file);

  dali::InitLogging(log_file.string(), dali::severity::trace, true);

  std::vector<std::thread> threads;
  threads.reserve(kThreadCount);
  for (int thread_id = 0; thread_id < kThreadCount; ++thread_id) {
    threads.emplace_back([thread_id]() {
      for (int message_id = 0; message_id < kMessagesPerThread; ++message_id) {
        LOG(info) << ExpectedRecord(thread_id, message_id) << "\n";
      }
    });
  }

  for (std::thread& thread : threads) {
    thread.join();
  }

  dali::CloseLogging();

  std::ifstream input(log_file);
  ASSERT_TRUE(input.is_open());

  std::map<std::string, int> record_counts;
  std::string line;
  int line_count = 0;
  while (std::getline(input, line)) {
    ++line_count;
    ++record_counts[line];
  }

  EXPECT_EQ(line_count, kThreadCount * kMessagesPerThread);

  for (int thread_id = 0; thread_id < kThreadCount; ++thread_id) {
    for (int message_id = 0; message_id < kMessagesPerThread; ++message_id) {
      const std::string expected = ExpectedRecord(thread_id, message_id);
      EXPECT_EQ(record_counts[expected], 1) << expected;
    }
  }

  std::filesystem::remove(log_file);
}
