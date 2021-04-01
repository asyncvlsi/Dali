//
// Created by Yihang Yang on 11/21/20.
//

#include "logging.h"

namespace dali {

namespace logging = boost::log;
namespace keywords = boost::log::keywords;

typedef boost::log::sinks::synchronous_sink<boost::log::sinks::text_file_backend> file_sink_t;
boost::shared_ptr<file_sink_t> g_file_sink = nullptr;

typedef boost::log::sinks::synchronous_sink<boost::log::sinks::basic_text_ostream_backend<char> > console_sink_t;
boost::shared_ptr<console_sink_t> g_console_sink = nullptr;

boost::log::trivial::severity_level StrNumToLoggingLevel(std::string sl_str) {
  int level = -1;
  try {
    level = std::stoi(sl_str);
  } catch (...) {
    std::cout << "Invalid dali verbosity level (0-5)!\n";
    exit(1);
  }

  switch (level) {
    case 0  :return boost::log::trivial::severity_level::fatal;
    case 1  :return boost::log::trivial::severity_level::error;
    case 2  :return boost::log::trivial::severity_level::warning;
    case 3  :return boost::log::trivial::severity_level::info;
    case 4  :return boost::log::trivial::severity_level::debug;
    case 5  :return boost::log::trivial::severity_level::trace;
    default :std::cout << "Invalid dali verbosity level (0-5)!\n";
      exit(1);
  }
}

void InitLogging(boost::log::trivial::severity_level sl) {
  CloseLogging();

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

  g_file_sink = logging::add_file_log
      (
          keywords::file_name = file_name,
          keywords::format = "[%TimeStamp%] [%ThreadID%] [%Severity%] %Message%"
      );
  g_file_sink->locked_backend()->set_auto_newline_mode(logging::sinks::auto_newline_mode::disabled_auto_newline);
  g_file_sink->locked_backend()->auto_flush(false);

  logging::core::get()->set_filter
      (
          logging::trivial::severity >= sl
      );

  logging::add_common_attributes();
  g_console_sink = logging::add_console_log(std::cout, boost::log::keywords::format = "%Message%");
  g_console_sink->locked_backend()->set_auto_newline_mode(logging::sinks::auto_newline_mode::disabled_auto_newline);
  g_console_sink->locked_backend()->auto_flush(false);

// write floating-point values in scientific notation.
//  BOOST_LOG_TRIVIAL(trace) << std::scientific << std::setprecision(4);
}

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