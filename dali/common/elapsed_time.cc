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

#include "elapsed_time.h"

#include "logging.h"

namespace dali {

void ElapsedTime::RecordStartTime() {
  start_wall_time_ = std::chrono::steady_clock::now();
  start_cpu_time_ = std::clock();
}

void ElapsedTime::RecordEndTime() {
  end_wall_time_ = std::chrono::steady_clock::now();
  end_cpu_time_ = std::clock();
}

void ElapsedTime::PrintTimeElapsed() {
  std::chrono::duration<double> wall_time = end_wall_time_ - start_wall_time_;
  double cpu_time =
      static_cast<double>(end_cpu_time_ - start_cpu_time_) / CLOCKS_PER_SEC;
  BOOST_LOG_TRIVIAL(info)
    << "wall time: " << wall_time.count()
    << "s, cpu time: " << cpu_time << "s\n";
}

} // dali
