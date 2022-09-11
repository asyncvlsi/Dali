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

namespace dali {

void ElapsedTime::RecordStartTime() {
  start_wall_time_ = std::chrono::steady_clock::now();
  start_cpu_time_ = std::clock();
}

void ElapsedTime::RecordEndTime() {
  end_wall_time_ = std::chrono::steady_clock::now();
  end_cpu_time_ = std::clock();
  std::chrono::duration<double> wall_time = end_wall_time_ - start_wall_time_;
  wall_time_ = wall_time.count();
  cpu_time_ =
      static_cast<double>(end_cpu_time_ - start_cpu_time_) / CLOCKS_PER_SEC;
}

void ElapsedTime::PrintTimeElapsed(severity lvl) const {
  std::string buffer(1024, '\0');
  int32_t written_length = sprintf(
      &buffer[0], "(wall time: %fs, cpu time: %f)\n", wall_time_, cpu_time_
  );
  buffer.resize(written_length);
  switch (lvl) {
    case boost::log::trivial::trace : {
      BOOST_LOG_TRIVIAL(trace) << buffer;
      break;
    }
    case boost::log::trivial::debug : {
      BOOST_LOG_TRIVIAL(debug) << buffer;
      break;
    }
    case boost::log::trivial::info : {
      BOOST_LOG_TRIVIAL(info) << buffer;
      break;
    }
    case boost::log::trivial::warning : {
      BOOST_LOG_TRIVIAL(warning) << buffer;
      break;
    }
    case boost::log::trivial::error : {
      BOOST_LOG_TRIVIAL(error) << buffer;
      break;
    }
    case boost::log::trivial::fatal : {
      BOOST_LOG_TRIVIAL(fatal) << buffer;
      break;
    }
    default : {
      DaliFatal("Unknown Boost severity level");
    }
  }

}

double ElapsedTime::GetWallTime() const {
  return wall_time_;
}

double ElapsedTime::GetCpuTime() const {
  return cpu_time_;
}

} // dali
