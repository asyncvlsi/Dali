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
#ifndef DALI_COMMON_LOGGING_H_
#define DALI_COMMON_LOGGING_H_

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

inline std::ostream& operator<<(std::ostream& os,
                                const std::vector<double>& v) {
  os << "[";
  for (size_t i = 0; i < v.size(); ++i) {
    os << v[i];
    if (i != v.size() - 1) os << ", ";
  }
  os << "]";
  return os;
}

namespace dali {

enum class severity {
  trace = 0,
  debug = 1,
  info = 2,
  warning = 3,
  error = 4,
  fatal = 5,
};

/** Return the printable name for a logging severity. */
const char* SeverityName(severity level);

/**
 * Builds a log record with stream syntax and flushes it when destroyed.
 *
 * This class exists so call sites can use simple stream logging syntax.
 */
class LogMessage {
 public:
  explicit LogMessage(severity level);
  LogMessage(const LogMessage&) = delete;
  LogMessage& operator=(const LogMessage&) = delete;
  LogMessage(LogMessage&&) = default;
  ~LogMessage();

  std::ostream& Stream();

 private:
  severity level_;
  std::ostringstream stream_;
};

/** Convert Dali verbosity level 0-5 to a logging severity. */
severity IntToLoggingLevel(int level);

/** Convert a Dali verbosity string 0-5 to a logging severity. */
severity StrToLoggingLevel(const std::string& severity_level_str);

/**
 * Initialize console and file logging.
 *
 * If log_file_name is empty, a unique dali<N>.log file is created in the
 * current working directory.
 */
void InitLogging(const std::string& log_file_name = "",
                 severity severity_level = severity::info,
                 bool disable_log_prefix = false);

/** Remove active logging sinks and release their resources. */
void CloseLogging();

#define LOG(level) ::dali::LogMessage(::dali::severity::level).Stream()

#define DaliExpects(e, error_message)                                      \
  do {                                                                     \
    if (!(e)) {                                                            \
      LOG(fatal) << "\033[0;31m"                                           \
                 << "FATAL ERROR:" << "\n"                                 \
                 << "    " << error_message << "\n"                        \
                 << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ \
                 << "\033[0m" << std::endl;                                \
      exit(1);                                                             \
    }                                                                      \
  } while (0)

#define DaliFatal(error_message)                                         \
  do {                                                                   \
    LOG(fatal) << "\033[0;31m"                                           \
               << "FATAL ERROR:" << "\n"                                 \
               << "    " << error_message << "\n"                        \
               << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ \
               << "\033[0m" << std::endl;                                \
    exit(1);                                                             \
  } while (0)

#define DaliWarns(e, warning_message)                   \
  do {                                                  \
    if ((e)) {                                          \
      LOG(warning) << "\033[0;34m"                      \
                   << "WARNING:" << "\n"                \
                   << "    " << warning_message << "\n" \
                   << "\033[0m";                        \
    }                                                   \
  } while (0)

#define DaliWarning(warning_message)                  \
  do {                                                \
    LOG(warning) << "\033[0;34m"                      \
                 << "WARNING:" << "\n"                \
                 << "    " << warning_message << "\n" \
                 << "\033[0m";                        \
  } while (0)

}  // namespace dali

#endif  // DALI_COMMON_LOGGING_H_
