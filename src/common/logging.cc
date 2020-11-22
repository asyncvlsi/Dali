//
// Created by yihang on 11/21/20.
//

#include "logging.h"

namespace dali {

namespace logging = boost::log;
namespace keywords = boost::log::keywords;

void init_logging(boost::log::trivial::severity_level sl) {
  std::string base_name = "dali";
  std::string extension = ".log";
  std::string file_name;

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

  auto file_sink = logging::add_file_log
      (
          keywords::file_name = file_name,
          keywords::format = "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%"
      );
  file_sink->locked_backend()->set_auto_newline_mode(logging::sinks::auto_newline_mode::disabled_auto_newline);
  file_sink->locked_backend()->auto_flush(false);

  logging::core::get()->set_filter
      (
          logging::trivial::severity >= sl
      );

  logging::add_common_attributes();
  auto console_sink = logging::add_console_log(std::cout, boost::log::keywords::format = "%Message%");
  console_sink->locked_backend()->set_auto_newline_mode(logging::sinks::auto_newline_mode::disabled_auto_newline);
  console_sink->locked_backend()->auto_flush(false);

// write floating-point values in scientific notation.
  BOOST_LOG_TRIVIAL(info) << std::scientific << std::setprecision(4);
}

}