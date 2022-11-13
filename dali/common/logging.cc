/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
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
#include "logging.h"

#include <filesystem>

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

namespace dali {

namespace keywords = boost::log::keywords;
namespace sink = boost::log::sinks;

typedef sink::synchronous_sink<sink::text_file_backend> file_sink_t;
boost::shared_ptr<file_sink_t> g_file_sink = nullptr;

typedef sink::synchronous_sink<sink::basic_text_ostream_backend<char>>
    console_sink_t;
boost::shared_ptr<console_sink_t> g_console_sink = nullptr;

severity IntToLoggingLevel(int level) {
  switch (level) {
    case 0  : { return severity::fatal; }
    case 1  : { return severity::error; }
    case 2  : { return severity::warning; }
    case 3  : { return severity::info; }
    case 4  : { return severity::debug; }
    case 5  : { return severity::trace; }
    default : {
      std::cout << "Invalid dali verbosity level (0-5)!\n";
      exit(1);
    }
  }
}

/****
 * @brief Convert a string number (0-5) to the corresponding boost logging level
 *
 * @param severity_level_str: severity level string
 * @return boost severity level
 */
severity StrToLoggingLevel(const std::string &severity_level_str) {
  int level;
  try {
    level = std::stoi(severity_level_str);
  } catch (...) {
    std::cout << "Invalid dali verbosity level (0-5)!\n";
    exit(1);
  }

  return IntToLoggingLevel(level);
}

/****
 * @brief Initialize the logging system. The log will be directed to the
 * console and a log file.
 *
 * @param log_file_name: the name of the log file. This parameter can be an empty
 * string. If it is empty, then the final log file name will be 'dali_X.log',
 * where X is the first number which makes the log file name different from any
 * other files in the working directory.
 * @param severity_level: 0 (fatal) - 5 (trace)
 * @param disable_log_prefix: if false, then the log file will have time_stamp,
 * thread_id, and severity level as the prefix.
 */
void InitLogging(
    const std::string &log_file_name,
    severity severity_level,
    bool disable_log_prefix
) {
  // in case the logging system has been initialized, we need to close it first
  CloseLogging();

  // determine the log file name
  std::string file_name = log_file_name;
  if (file_name.empty()) {
    std::string base_name = "dali";
    std::string extension = ".log";
    int upper_limits = 2048;
    for (int i = 0; i < upper_limits; ++i) {
      file_name += base_name;
      file_name += std::to_string(i);
      file_name += extension;
      if (!std::filesystem::exists(file_name)) {
        break;
      }
      file_name.clear();
    }
    if (file_name.empty()) {
      file_name = "dali_out_of_bounds.log";
    }
  }

  // add a file sink
  if (disable_log_prefix) {
    g_file_sink = boost::log::add_file_log(
        keywords::file_name = file_name,
        keywords::format = "%Message%"
    );
  } else {
    g_file_sink = boost::log::add_file_log(
        keywords::file_name = file_name,
        keywords::format = "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%"
    );
  }
  g_file_sink->locked_backend()->set_auto_newline_mode(
      sink::auto_newline_mode::disabled_auto_newline
  );
  g_file_sink->locked_backend()->auto_flush(false);

  // add a console sink
  g_console_sink = boost::log::add_console_log(
      std::cout,
      boost::log::keywords::format = "%Message%"
  );
  g_console_sink->locked_backend()->set_auto_newline_mode(
      sink::auto_newline_mode::disabled_auto_newline
  );
  g_console_sink->locked_backend()->auto_flush(false);

  // register attributes with the log core
  boost::log::core::get()->set_filter(
      boost::log::trivial::severity >= severity_level
  );
  boost::log::add_common_attributes();
}

/****
 * @brief Close the logging system by removing and resetting the file sink
 * and console sink. Nothing will happen if the logging system has not been
 * initialized.
 */
void CloseLogging() {
  if (g_file_sink != nullptr) {
    boost::log::core::get()->remove_sink(g_file_sink);
    g_file_sink.reset();
  }
  if (g_console_sink != nullptr) {
    boost::log::core::get()->remove_sink(g_console_sink);
    g_console_sink.reset();
  }
}

}
