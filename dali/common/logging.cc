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

namespace dali {

namespace keywords = boost::log::keywords;
namespace sink = boost::log::sinks;

typedef sink::synchronous_sink<sink::text_file_backend> file_sink_t;
boost::shared_ptr<file_sink_t> g_file_sink = nullptr;

typedef sink::synchronous_sink<sink::basic_text_ostream_backend<char>>
    console_sink_t;
boost::shared_ptr<console_sink_t> g_console_sink = nullptr;

severity StrToLoggingLevel(const std::string &sl_str) {
  int level;
  try {
    level = std::stoi(sl_str);
  } catch (...) {
    std::cout << "Invalid dali verbosity level (0-5)!\n";
    exit(1);
  }

  switch (level) {
    case 0  :return severity::fatal;
    case 1  :return severity::error;
    case 2  :return severity::warning;
    case 3  :return severity::info;
    case 4  :return severity::debug;
    case 5  :return severity::trace;
    default :std::cout << "Invalid dali verbosity level (0-5)!\n";
      exit(1);
  }
}

/****
 * @brief Initialize the logging system.
 *
 * @param log_file_name: the name of the log file. This parameter can be an empty
 * string. If it is empty, then the final log file name will be 'daliX.log', where
 * X is a number which makes the log file name different from any other files in
 * the working directory.
 * @param overwrite_log_file: if the log_file_name is not empty, this parameter is
 * ignored; otherwise, if this parameter is true, then the final log file name is
 * always 'dali0.log'.
 * @param severity_level: 0 (fatal) - 5 (trace)
 * @param no_prefix: if true, then the log file only has severity level as prefix
 */
void InitLogging(
    const std::string &log_file_name,
    bool overwrite_log_file,
    severity severity_level,
    bool no_prefix
) {
  CloseLogging();

  // determine the log file name
  std::string file_name = log_file_name;
  if (file_name.empty()) {
    std::string base_name = "dali";
    std::string extension = ".log";
    if (overwrite_log_file) {
      file_name = "dali0.log";
    } else {
      int upper_limits = 2048;
      for (int i = 0; i < upper_limits; ++i) {
        file_name += base_name;
        file_name += std::to_string(i);
        file_name += extension;
        if (!boost::filesystem::exists(file_name)) {
          break;
        }
        file_name.clear();
      }
      if (file_name.empty()) {
        file_name = "dali_out_of_bounds.log";
      }
    }
  }

  // add a file sink
  if (no_prefix) {
    g_file_sink = logging::add_file_log(
        keywords::file_name = file_name,
        keywords::format = "%Message%"
    );

  } else {
    g_file_sink = logging::add_file_log(
        keywords::file_name = file_name,
        keywords::format = "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%"
    );
  }
  g_file_sink->locked_backend()->set_auto_newline_mode(
      sink::auto_newline_mode::disabled_auto_newline
  );
  g_file_sink->locked_backend()->auto_flush(false);

  logging::core::get()->set_filter(
      logging::trivial::severity >= severity_level
  );
  logging::add_common_attributes();

  // add a console sink
  g_console_sink = logging::add_console_log(
      std::cout,
      logging::keywords::format = "%Message%"
  );
  g_console_sink->locked_backend()->set_auto_newline_mode(
      logging::sinks::auto_newline_mode::disabled_auto_newline
  );
  g_console_sink->locked_backend()->auto_flush(false);
}

void CloseLogging() {
  if (g_file_sink != nullptr) {
    logging::core::get()->remove_sink(g_file_sink);
    g_file_sink.reset();
  }
  if (g_console_sink != nullptr) {
    logging::core::get()->remove_sink(g_console_sink);
    g_console_sink.reset();
  }
}

}
