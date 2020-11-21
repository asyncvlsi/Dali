//
// Created by Yihang Yang on 11/20/20.
//

#ifndef DALI_SRC_COMMON_LOGGING_H_
#define DALI_SRC_COMMON_LOGGING_H_

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/formatting_ostream.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

inline boost::log::record_ostream &operator<<(boost::log::record_ostream &p, std::vector<double> &v) {
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

void init_logging_and_formatting(boost::log::trivial::severity_level sl = boost::log::trivial::info);

inline void Assert(bool e, const std::string &error_message) {
  if (!e) {
    BOOST_LOG_TRIVIAL(fatal) << "\033[0;31m" << "FATAL ERROR:" << "\n"
                             << "    " << error_message << "\033[0m" << std::endl;
    exit(1);
  }
}

inline void Warning(bool e, const std::string &warning_message) {
  if (e) {
    BOOST_LOG_TRIVIAL(warning) << "WARNING:" << "\n"
                               << "    " << warning_message << std::endl;
  }
}

inline void PrintSoftwareStatement() {
  BOOST_LOG_TRIVIAL(info) << "\n"
                          << "+----------------------------------------------+\n"
                          << "|                                              |\n"
                          << "|     Dali: gridded cell placement flow        |\n"
                          << "|                                              |\n"
                          << "|     Department of Electrical Engineering     |\n"
                          << "|     Yale University                          |\n"
                          << "|                                              |\n"
                          << "|     Developed by                             |\n"
                          << "|     Yihang Yang, Rajit Manohar               |\n"
                          << "|                                              |\n"
                          << "|     This program is for academic use and     |\n"
                          << "|     testing only                             |\n"
                          << "|     THERE IS NO WARRANTY                     |\n"
                          << "|                                              |\n"
                          << "|     build time: " << __DATE__ << " " << __TIME__ << "         |\n"
                          << "|                                              |\n"
                          << "+----------------------------------------------+\n\n";
}

inline void ReportDaliUsage() {
  BOOST_LOG_TRIVIAL(info)   << "\033[0;36m"
                            << "Usage: dali\n"
                            << "  -lef        <file.lef>\n"
                            << "  -def        <file.def>\n"
                            << "  -cell       <file.cell> (optional, if provided, iterative well placement flow will be triggered)\n"
                            << "  -o          <output_name>.def (optional, default output file name dali_out.def)\n"
                            << "  -g/-grid    grid_value_x grid_value_y (optional, default metal1 and metal2 pitch values)\n"
                            << "  -d/-density density (optional, value interval (0,1], default max(space_utility, 0.7))\n"
                            << "  -n          (optional, if this flag is present, then use naive LEF/DEF parser)\n"
                            << "  -iolayer    metal_layer_num (optional, default 1 for m1)\n"
                            << "  -v          verbosity_level (optional, 0-5, default 1)\n"
                            << "(flag order does not matter)"
                            << "\033[0m\n";
}

}

#endif //DALI_SRC_COMMON_LOGGING_H_
