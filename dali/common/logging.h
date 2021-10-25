//
// Created by Yihang Yang on 11/20/20.
//

#ifndef DALI_DALI_COMMON_LOGGING_H_
#define DALI_DALI_COMMON_LOGGING_H_

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
    const std::string &log_file_name = "",
    bool overwrite_log_file = false,
    severity severity_level = logging::trivial::info
);

void CloseLogging();

#define DaliExpects(e, error_message) DaliExpects_(e, error_message, __FILE__, __LINE__, __FUNCTION__)
inline void DaliExpects_(
    bool e,
    const std::string &error_message,
    const char *file,
    size_t line,
    const char *function
) {
    if (!e) {
        BOOST_LOG_TRIVIAL(fatal)
            << "\033[0;31m"
            << "FATAL ERROR:" << "\n"
            << "    " << error_message << "\n"
            << file << " : " << line << " : " << function
            << "\033[0m" << std::endl;
        exit(1);
    }
}

inline void DaliWarns(bool e, const std::string &warning_message) {
    if (e) {
        BOOST_LOG_TRIVIAL(warning)
            << "\033[0;34m"
            << "WARNING:" << "\n"
            << "    " << warning_message
            << "\033[0m" << std::endl;
    }
}

}

#endif //DALI_DALI_COMMON_LOGGING_H_
