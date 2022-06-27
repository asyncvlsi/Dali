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

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <boost/log/trivial.hpp>
#include <boost/log/utility/formatting_ostream.hpp>

inline boost::log::record_ostream &operator<<(
    boost::log::record_ostream &p, std::vector<double> &v
) {
  p << "[";
  for (size_t i = 0; i < v.size(); ++i) {
    p << v[i];
    if (i != v.size() - 1)
      p << ", ";
  }
  p << "]";
  return p;
}

namespace dali {

typedef boost::log::trivial::severity_level severity;

severity StrToLoggingLevel(const std::string &severity_level_str);

void InitLogging(
    const std::string &log_file_name = "",
    severity severity_level = boost::log::trivial::info,
    bool has_log_prefix = true
);

void CloseLogging();

#define DaliExpects(e, error_message) do{ \
  if(!(e)) { \
    BOOST_LOG_TRIVIAL(fatal) \
      << "\033[0;31m" \
      << "FATAL ERROR:" << "\n" \
      << "    " << error_message << "\n" \
      << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ \
      << "\033[0m" << std::endl; \
      exit(1); \
  } \
} while(0)

#define DaliFatal(error_message) do{ \
  BOOST_LOG_TRIVIAL(fatal) \
      << "\033[0;31m" \
      << "FATAL ERROR:" << "\n" \
      << "    " << error_message << "\n" \
      << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ \
      << "\033[0m" << std::endl; \
      exit(1); \
} while(0)

#define DaliWarns(e, warning_message) do{ \
  if((e)) { \
    BOOST_LOG_TRIVIAL(warning) \
      << "\033[0;34m" \
      << "WARNING:" << "\n" \
      << "    " << warning_message << "\n" \
      << "\033[0m"; \
  } \
} while(0)

#define DaliWarning(warning_message) do{ \
  BOOST_LOG_TRIVIAL(warning) \
      << "\033[0;34m" \
      << "WARNING:" << "\n" \
      << "    " << warning_message << "\n" \
      << "\033[0m"; \
} while(0)

}

#endif //DALI_COMMON_LOGGING_H_
