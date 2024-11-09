/*******************************************************************************
 *
 * Copyright (c) 2022 Yihang Yang
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
#ifndef DALI_COMMON_ELAPSED_TIME_H_
#define DALI_COMMON_ELAPSED_TIME_H_

#include <chrono>
#include <ctime>

#include "logging.h"

namespace dali {

class ElapsedTime {
 public:
  ElapsedTime() = default;
  void RecordStartTime();
  void RecordEndTime();
  void PrintTimeElapsed(severity lvl = boost::log::trivial::info) const;
  double GetWallTime() const;
  double GetCpuTime() const;

 private:
  std::chrono::time_point<std::chrono::steady_clock> start_wall_time_;
  std::chrono::time_point<std::chrono::steady_clock> end_wall_time_;
  clock_t start_cpu_time_ = 0;
  clock_t end_cpu_time_ = 0;

  // cached time duration
  double wall_time_ = 0;
  double cpu_time_ = 0;
};

}  // namespace dali

#endif  // DALI_COMMON_ELAPSED_TIME_H_
