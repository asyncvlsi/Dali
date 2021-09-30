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

namespace logging = boost::log;
typedef boost::log::trivial::severity_level severity;

severity StrToLoggingLevel(const std::string &sl_str);

void InitLogging(
    const std::string &log_file_name,
    bool overwrite_log_file = false,
    severity severity_level = logging::trivial::info,
    const std::string &console_log_prefix = ""
);

void CloseLogging();

inline void DaliExpects(bool e, const std::string &error_message) {
    if (!e) {
        BOOST_LOG_TRIVIAL(fatal)
            << "\033[0;31m" << "Dali FATAL ERROR:" << "\n"
            << "    " << error_message << "\033[0m"
            << std::endl;
        exit(1);
    }
}

inline void DaliWarns(bool e, const std::string &warning_message) {
    if (e) {
        BOOST_LOG_TRIVIAL(warning)
            << "Dali WARNING:" << "\n"
            << "    " << warning_message
            << std::endl;
    }
}

}

#endif //DALI_SRC_COMMON_LOGGING_H_
