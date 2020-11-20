//
// Created by yihang on 11/20/20.
//

#include "logging.h"

void init_logging_and_formatting(boost::log::trivial::severity_level sl) {
  std::string base_name = "dali";
  std::string extension = ".log";
  std::string file_name;
  for (int i = 0; i < 1024; ++i) {
    file_name = base_name + std::to_string(i) + extension;
    if (!boost::filesystem::exists(file_name)) {
      break;
    }
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